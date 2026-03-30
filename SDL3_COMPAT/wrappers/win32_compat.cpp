#include "win32_compat.h"
#include "mmsystem.h"
#include "SDLINPUT.H"

#include <SDL3/SDL_loadso.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if !defined(_WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <dirent.h>
#include <sys/stat.h>
#endif

#if defined(__linux__)
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__) && !defined(__unix__)
#define RA_REAL_WINDOWS 1
#else
#define RA_REAL_WINDOWS 0
#endif

namespace {

enum class HandleKind {
    None,
    File,
    Process,
    Thread,
    Event,
    Mutex,
    Search,
    Mapping,
    Global,
};

struct HandleBase {
    explicit HandleBase(HandleKind kind) : kind(kind) {}
    virtual ~HandleBase() = default;
    HandleKind kind;
};

struct FileHandle final : HandleBase {
    FileHandle(SDL_IOStream* io_stream, std::string file_path)
        : HandleBase(HandleKind::File), io(io_stream), path(std::move(file_path)) {}
    SDL_IOStream* io;
    std::string path;
};

struct ProcessHandle final : HandleBase {
    explicit ProcessHandle(SDL_Process* process) : HandleBase(HandleKind::Process), process(process) {}
    SDL_Process* process;
    bool exited = false;
    DWORD exit_code = STILL_ACTIVE;
};

struct ThreadHandle final : HandleBase {
    ThreadHandle() : HandleBase(HandleKind::Thread) {}
    std::thread worker;
    std::mutex mutex;
    std::condition_variable condition;
    bool finished = false;
    DWORD thread_id = 0;
};

struct EventHandle final : HandleBase {
    EventHandle(bool manual_reset, bool initial_state) : HandleBase(HandleKind::Event), manual(manual_reset), signaled(initial_state) {}
    std::mutex mutex;
    std::condition_variable condition;
    bool manual;
    bool signaled;
};

struct MutexHandle final : HandleBase {
    MutexHandle() : HandleBase(HandleKind::Mutex) {}
    std::timed_mutex mutex;
};

struct SearchMatch {
    std::string name;
    DWORD attributes = 0;
    DWORD size_high = 0;
    DWORD size_low = 0;
};

struct SearchHandle final : HandleBase {
    SearchHandle() : HandleBase(HandleKind::Search) {}
    std::vector<SearchMatch> matches;
    size_t index = 0;
};

struct MappingHandle final : HandleBase {
    MappingHandle() : HandleBase(HandleKind::Mapping) {}
    std::vector<std::byte> bytes;
};

struct GlobalHandle final : HandleBase {
    explicit GlobalHandle(size_t size) : HandleBase(HandleKind::Global), bytes(size) {}
    std::vector<std::byte> bytes;
};

std::mutex g_last_error_mutex;
DWORD g_last_error = ERROR_SUCCESS;
std::mutex g_window_class_mutex;
std::unordered_map<std::string, WNDCLASS> g_window_classes;
std::mutex g_window_mutex;
std::unordered_set<HWND> g_windows;
std::mutex g_message_queue_mutex;
std::deque<MSG> g_message_queue;
std::mutex g_registered_message_mutex;
std::unordered_map<std::string, UINT> g_registered_messages;
std::atomic<UINT> g_next_registered_message{0xC000};
std::mutex g_registry_mutex;
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> g_registry_values;
std::mutex g_named_event_mutex;
std::unordered_map<std::string, EventHandle*> g_named_events;
std::atomic<DWORD> g_next_thread_id{1};
std::chrono::steady_clock::time_point g_start_time = std::chrono::steady_clock::now();
std::atomic<UINT> g_error_mode{0};

struct TimerHandle {
    std::thread worker;
    std::atomic<bool> active{true};
};

std::mutex g_timer_mutex;
std::unordered_map<UINT, std::unique_ptr<TimerHandle>> g_timers;
std::atomic<UINT> g_next_timer_id{1};

bool compat_trace_enabled()
{
    static const bool enabled = std::getenv("RA_TRACE_STARTUP") != nullptr;
    return enabled;
}

void compat_trace(const char* format, ...)
{
    if (!compat_trace_enabled()) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    std::fputs("[win32_compat] ", stderr);
    std::vfprintf(stderr, format, arguments);
    std::fputc('\n', stderr);
    std::fflush(stderr);
    va_end(arguments);
}

std::string compat_base_directory()
{
    static const std::string path = []() {
#if defined(__linux__)
        char executable_path[4096];
        const ssize_t path_length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);
        if (path_length > 0) {
            executable_path[path_length] = '\0';
            std::string path_string(executable_path, static_cast<size_t>(path_length));
            const std::string::size_type separator = path_string.find_last_of('/');
            if (separator != std::string::npos) {
                return path_string.substr(0, separator);
            }
            return std::string(".");
        }
#endif

        const char* base_path = SDL_GetBasePath();
        if (base_path && *base_path) {
            std::string path_string(base_path);
            while (!path_string.empty() && (path_string.back() == '/' || path_string.back() == '\\')) {
                path_string.pop_back();
            }
            return path_string.empty() ? std::string(".") : path_string;
        }

        std::error_code error;
        return std::filesystem::current_path(error).string();
    }();

    return path;
}

bool compat_path_exists(const char* path)
{
    if (!path || !*path) {
        return false;
    }

#if RA_REAL_WINDOWS
    std::error_code error;
    return std::filesystem::exists(std::filesystem::path(path), error);
#else
    struct stat status;
    return stat(path, &status) == 0;
#endif
}

int virtual_cd_index_for_drive_letter(char drive_letter)
{
    const int index = std::tolower(static_cast<unsigned char>(drive_letter)) - 'c';
    if (index < 0 || index >= 4) {
        return -1;
    }

    char mix_name[16];
    std::snprintf(mix_name, sizeof(mix_name), "MAIN%d.MIX", index + 1);

    std::string mix_path = compat_base_directory();
    if (!mix_path.empty() && mix_path.back() != '/' && mix_path.back() != '\\') {
        mix_path.push_back('/');
    }
    mix_path += mix_name;

    if (!compat_path_exists(mix_path.c_str())) {
        return -1;
    }

    return index;
}

bool resolve_virtual_cd_path(const char* windows_path, std::string& resolved_path, int* cd_index = nullptr)
{
    if (!windows_path || std::strlen(windows_path) < 2 || windows_path[1] != ':') {
        return false;
    }

    const int index = virtual_cd_index_for_drive_letter(windows_path[0]);
    if (index < 0) {
        return false;
    }

    std::string relative = windows_path + 2;
    std::replace(relative.begin(), relative.end(), '\\', '/');
    while (!relative.empty() && relative.front() == '/') {
        relative.erase(relative.begin());
    }

    const std::string base_path = compat_base_directory();
    if (relative.empty()) {
        resolved_path = base_path;
    } else if (SDL_strcasecmp(relative.c_str(), "main.mix") == 0) {
        char mix_name[16];
        std::snprintf(mix_name, sizeof(mix_name), "MAIN%d.MIX", index + 1);
        resolved_path = base_path + "/" + mix_name;
    } else {
        resolved_path = base_path + "/" + relative;
    }

    if (cd_index) {
        *cd_index = index;
    }
    return true;
}

std::string normalize_compat_path(const char* windows_path)
{
    std::string resolved_path;
    if (resolve_virtual_cd_path(windows_path, resolved_path)) {
        return resolved_path;
    }

    std::string normalized = windows_path ? windows_path : "";
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

HWND first_window()
{
    std::scoped_lock lock(g_window_mutex);
    if (g_windows.empty()) {
        return nullptr;
    }
    return *g_windows.begin();
}

HWND window_for_id(SDL_WindowID window_id)
{
    if (window_id != 0) {
        std::scoped_lock lock(g_window_mutex);
        for (HWND window : g_windows) {
            if (window && window->sdl_window && SDL_GetWindowID(window->sdl_window) == window_id) {
                return window;
            }
        }
    }

    return first_window();
}

void set_last_error(DWORD value)
{
    std::scoped_lock lock(g_last_error_mutex);
    g_last_error = value;
}

void populate_system_time(SYSTEMTIME* system_time, const std::tm& time_info, int milliseconds)
{
    if (!system_time) {
        return;
    }

    system_time->wYear = static_cast<WORD>(time_info.tm_year + 1900);
    system_time->wMonth = static_cast<WORD>(time_info.tm_mon + 1);
    system_time->wDayOfWeek = static_cast<WORD>(time_info.tm_wday);
    system_time->wDay = static_cast<WORD>(time_info.tm_mday);
    system_time->wHour = static_cast<WORD>(time_info.tm_hour);
    system_time->wMinute = static_cast<WORD>(time_info.tm_min);
    system_time->wSecond = static_cast<WORD>(time_info.tm_sec);
    system_time->wMilliseconds = static_cast<WORD>(milliseconds);
}

void fill_current_system_time(SYSTEMTIME* system_time, bool local_time)
{
    if (!system_time) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
    const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    std::tm time_info{};

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    if (local_time) {
        localtime_r(&current_time, &time_info);
    } else {
        gmtime_r(&current_time, &time_info);
    }
#else
    const std::tm* source_time = local_time ? std::localtime(&current_time) : std::gmtime(&current_time);
    if (source_time) {
        time_info = *source_time;
    } else {
        std::memset(&time_info, 0, sizeof(time_info));
    }
#endif

    populate_system_time(system_time, time_info, milliseconds);
}

std::vector<std::string> split_command_line(const char* command_line)
{
    std::vector<std::string> arguments;
    if (!command_line) {
        return arguments;
    }

    std::string current;
    bool in_quotes = false;
    for (const char* cursor = command_line; *cursor; ++cursor) {
        const char ch = *cursor;
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                arguments.push_back(current);
                current.clear();
            }
            continue;
        }
        current += ch;
    }

    if (!current.empty()) {
        arguments.push_back(current);
    }
    return arguments;
}

constexpr uint64_t kWindowsTicksPerSecond = 10000000ULL;
constexpr uint64_t kUnixEpochInWindowsTicks = 11644473600ULL * kWindowsTicksPerSecond;

uint64_t filetime_to_uint64(const FILETIME& file_time)
{
    return (static_cast<uint64_t>(file_time.dwHighDateTime) << 32) | file_time.dwLowDateTime;
}

FILETIME uint64_to_filetime(uint64_t value)
{
    FILETIME file_time{};
    file_time.dwLowDateTime = static_cast<DWORD>(value & 0xffffffffULL);
    file_time.dwHighDateTime = static_cast<DWORD>(value >> 32);
    return file_time;
}

std::chrono::system_clock::time_point filetime_to_system_clock(const FILETIME& file_time)
{
    const uint64_t ticks = filetime_to_uint64(file_time);
    if (ticks <= kUnixEpochInWindowsTicks) {
        return std::chrono::system_clock::time_point{};
    }

    const uint64_t unix_ticks = ticks - kUnixEpochInWindowsTicks;
    const auto duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::nanoseconds(unix_ticks * 100ULL));
    return std::chrono::system_clock::time_point(duration);
}

FILETIME system_clock_to_filetime(std::chrono::system_clock::time_point time_point)
{
    const auto since_epoch = time_point.time_since_epoch();
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count();
    const uint64_t clamped_nanoseconds = nanoseconds > 0 ? static_cast<uint64_t>(nanoseconds) : 0ULL;
    return uint64_to_filetime(kUnixEpochInWindowsTicks + (clamped_nanoseconds / 100ULL));
}

std::chrono::system_clock::time_point filesystem_to_system_clock(std::filesystem::file_time_type file_time)
{
    return std::chrono::system_clock::now()
        + std::chrono::duration_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now());
}

std::filesystem::file_time_type system_to_filesystem_clock(std::chrono::system_clock::time_point time_point)
{
    return std::filesystem::file_time_type::clock::now()
        + std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
            time_point - std::chrono::system_clock::now());
}

std::string wildcard_to_regex(const std::string& pattern)
{
    std::string out = "^";
    for (char ch : pattern) {
        switch (ch) {
            case '*': out += ".*"; break;
            case '?': out += '.'; break;
            case '.': out += "\\."; break;
            case '\\': out += "\\\\"; break;
            default:
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '/' || ch == '_' || ch == '-') {
                    out += ch;
                } else {
                    out += '\\';
                    out += ch;
                }
                break;
        }
    }
    out += '$';
    return out;
}

std::string find_leaf_name(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    if (separator == std::string::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

std::string find_parent_directory(const std::string& path)
{
    const std::string::size_type separator = path.find_last_of("/\\");
    if (separator == std::string::npos) {
        return ".";
    }
    if (separator == 0) {
        return path.substr(0, 1);
    }
    return path.substr(0, separator);
}

std::string join_find_path(const std::string& directory, const std::string& name)
{
    if (directory.empty() || directory == ".") {
        return name;
    }

    if (directory.back() == '/' || directory.back() == '\\') {
        return directory + name;
    }

    return directory + "/" + name;
}

std::string translate_find_directory(std::string directory)
{
    std::replace(directory.begin(), directory.end(), '\\', '/');

    if (directory.size() >= 2 && std::isalpha(static_cast<unsigned char>(directory[0])) && directory[1] == ':') {
#if RA_REAL_WINDOWS
#else
        directory.erase(0, 2);
        if (!directory.empty() && (directory[0] == '/' || directory[0] == '\\')) {
            directory.erase(0, 1);
        }
#endif
    }

    if (directory.empty()) {
        return ".";
    }

    return directory;
}

#if !RA_REAL_WINDOWS
DWORD determine_find_attributes(const char* name, const struct stat& status)
{
    DWORD attributes = 0;

    if (name != nullptr && name[0] == '.') {
        attributes |= FILE_ATTRIBUTE_HIDDEN;
    }

    if (S_ISDIR(status.st_mode)) {
        attributes |= FILE_ATTRIBUTE_DIRECTORY;
    }

    if (attributes == 0) {
        attributes = FILE_ATTRIBUTE_NORMAL;
    }

    return attributes;
}

SearchMatch make_search_match(const char* name, const struct stat& status)
{
    SearchMatch match;
    if (name != nullptr) {
        match.name = name;
    }
    match.attributes = determine_find_attributes(name, status);
    if (S_ISREG(status.st_mode) && status.st_size > 0) {
        const uint64_t size = static_cast<uint64_t>(status.st_size);
        match.size_low = static_cast<DWORD>(size & 0xffffffffu);
        match.size_high = static_cast<DWORD>((size >> 32) & 0xffffffffu);
    }
    return match;
}
#endif

std::string create_file_mode(DWORD desired_access, DWORD creation_disposition)
{
    const bool can_read = (desired_access & GENERIC_READ) != 0;
    const bool can_write = (desired_access & GENERIC_WRITE) != 0;

    switch (creation_disposition) {
        case CREATE_ALWAYS:
            return can_read ? "w+b" : "wb";
        case OPEN_ALWAYS:
            return can_write ? (can_read ? "a+b" : "ab") : "rb";
        case OPEN_EXISTING:
        default:
            if (can_read && can_write) return "r+b";
            if (can_write) return "wb";
            return "rb";
    }
}

BOOL fill_find_data(const SearchMatch& entry, WIN32_FIND_DATA* data)
{
    if (!data) {
        return FALSE;
    }
    ZeroMemory(data, sizeof(*data));
    std::snprintf(data->cFileName, sizeof(data->cFileName), "%s", entry.name.c_str());
    data->dwFileAttributes = entry.attributes != 0 ? entry.attributes : FILE_ATTRIBUTE_NORMAL;
    data->nFileSizeLow = entry.size_low;
    data->nFileSizeHigh = entry.size_high;
    return TRUE;
}

BOOL next_message(MSG* message, bool remove, bool wait)
{
    auto try_dequeue = [&]() -> BOOL {
        std::scoped_lock lock(g_message_queue_mutex);
        if (g_message_queue.empty()) {
            return FALSE;
        }

        if (message) {
            *message = g_message_queue.front();
        }
        if (remove) {
            g_message_queue.pop_front();
        }
        return TRUE;
    };

    if (try_dequeue()) {
        return TRUE;
    }

    if (!wait) {
        SDL_GameInput_Pump();
        return try_dequeue();
    }

    while (!try_dequeue()) {
        if (!SDL_GameInput_Wait()) {
            return FALSE;
        }
    }

    return TRUE;
}

} // namespace

extern "C" {

ATOM RegisterClass(const WNDCLASS* wndclass)
{
    if (!wndclass || !wndclass->lpszClassName) {
        set_last_error(ERROR_INVALID_HANDLE);
        return 0;
    }
    std::scoped_lock lock(g_window_class_mutex);
    g_window_classes[wndclass->lpszClassName] = *wndclass;
    return 1;
}

BOOL UnregisterClass(LPCSTR class_name, HINSTANCE)
{
    std::scoped_lock lock(g_window_class_mutex);
    return g_window_classes.erase(class_name ? class_name : "") ? TRUE : FALSE;
}

HWND CreateWindowEx(DWORD, LPCSTR class_name, LPCSTR window_name, DWORD, INT, INT, INT width, INT height, HWND, HANDLE, HINSTANCE, LPVOID)
{
    WNDCLASS klass{};
    {
        std::scoped_lock lock(g_window_class_mutex);
        auto it = g_window_classes.find(class_name ? class_name : "");
        if (it != g_window_classes.end()) {
            klass = it->second;
        }
    }

    auto* window = new RAWindow{};
    window->class_name = class_name ? class_name : "";
    window->title = window_name ? window_name : "Red Alert";
    window->wnd_proc = klass.lpfnWndProc;
    window->width = width > 0 ? width : 640;
    window->height = height > 0 ? height : 480;
    window->sdl_window = SDL_CreateWindow(window->title.c_str(), window->width, window->height, 0);
    {
        std::scoped_lock lock(g_window_mutex);
        g_windows.insert(window);
    }
    return window;
}

HWND FindWindow(LPCSTR class_name, LPCSTR window_name)
{
    std::scoped_lock lock(g_window_mutex);
    for (HWND window : g_windows) {
        if (!window) {
            continue;
        }
        if (class_name && window->class_name != class_name) {
            continue;
        }
        if (window_name && window->title != window_name) {
            continue;
        }
        return window;
    }
    return nullptr;
}

BOOL IsWindow(HWND window)
{
    std::scoped_lock lock(g_window_mutex);
    return g_windows.find(window) != g_windows.end() ? TRUE : FALSE;
}

BOOL DestroyWindow(HWND window)
{
    if (!window) return FALSE;
    {
        std::scoped_lock lock(g_window_mutex);
        g_windows.erase(window);
    }
    if (window->sdl_window) {
        SDL_DestroyWindow(window->sdl_window);
    }
    delete window;
    return TRUE;
}

BOOL ShowWindow(HWND window, INT command)
{
    if (!window || !window->sdl_window) return FALSE;
    if (command == SW_HIDE) {
        SDL_HideWindow(window->sdl_window);
    } else if (command == SW_MINIMIZE || command == SW_SHOWMINIMIZED) {
        SDL_MinimizeWindow(window->sdl_window);
    } else if (command == SW_MAXIMIZE || command == SW_SHOWMAXIMIZED) {
        SDL_MaximizeWindow(window->sdl_window);
    } else if (command == SW_RESTORE) {
        SDL_RestoreWindow(window->sdl_window);
    } else {
        SDL_ShowWindow(window->sdl_window);
    }
    return TRUE;
}

BOOL SetForegroundWindow(HWND window)
{
    if (!window || !window->sdl_window) {
        return FALSE;
    }

    SDL_ShowWindow(window->sdl_window);
    SDL_RaiseWindow(window->sdl_window);
    PostMessage(window, WM_ACTIVATEAPP, TRUE, 0);
    return TRUE;
}

BOOL UpdateWindow(HWND)
{
    return TRUE;
}

LRESULT DefWindowProc(HWND, UINT message, WPARAM, LPARAM)
{
    if (message == WM_CLOSE) {
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return 0;
}

BOOL PeekMessage(MSG* message, HWND, UINT, UINT, UINT remove_message)
{
    return next_message(message, (remove_message & PM_REMOVE) != 0, false);
}

BOOL GetMessage(MSG* message, HWND, UINT, UINT)
{
    if (!next_message(message, true, true)) {
        return FALSE;
    }
    return message && message->message == WM_QUIT ? FALSE : TRUE;
}

BOOL TranslateMessage(const MSG*)
{
    return TRUE;
}

LRESULT DispatchMessage(const MSG* message)
{
    if (!message) return 0;
    if (message->hwnd && message->hwnd->wnd_proc) {
        return message->hwnd->wnd_proc(message->hwnd, message->message, message->wParam, message->lParam);
    }
    return DefWindowProc(message->hwnd, message->message, message->wParam, message->lParam);
}

void PostQuitMessage(INT exit_code)
{
    MSG msg{};
    msg.message = WM_QUIT;
    msg.wParam = static_cast<WPARAM>(exit_code);
    std::scoped_lock lock(g_message_queue_mutex);
    g_message_queue.push_back(msg);
}

BOOL PostMessage(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    MSG msg{};
    msg.hwnd = window;
    msg.message = message;
    msg.wParam = w_param;
    msg.lParam = l_param;
    msg.time = static_cast<DWORD>(SDL_GetTicks());
    std::scoped_lock lock(g_message_queue_mutex);
    g_message_queue.push_back(msg);
    return TRUE;
}

LRESULT SendMessage(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    MSG msg{};
    msg.hwnd = window;
    msg.message = message;
    msg.wParam = w_param;
    msg.lParam = l_param;
    return DispatchMessage(&msg);
}

UINT RegisterWindowMessage(LPCSTR string)
{
    if (!string) {
        return 0;
    }

    std::scoped_lock lock(g_registered_message_mutex);
    auto it = g_registered_messages.find(string);
    if (it != g_registered_messages.end()) {
        return it->second;
    }

    const UINT value = g_next_registered_message++;
    g_registered_messages.emplace(string, value);
    return value;
}

int GetSystemMetrics(int index)
{
    SDL_Rect bounds{};
    if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds)) {
        if (index == SM_CXSCREEN) {
            return 640;
        }
        if (index == SM_CYSCREEN) {
            return 480;
        }
        return 0;
    }

    if (index == SM_CXSCREEN) {
        return bounds.w;
    }
    if (index == SM_CYSCREEN) {
        return bounds.h;
    }
    return 0;
}

HWND SetFocus(HWND window)
{
    if (window) {
        SetForegroundWindow(window);
    }
    return window;
}

HGDIOBJ LoadIcon(HINSTANCE, LPCSTR)
{
    return nullptr;
}

HGDIOBJ LoadCursor(HINSTANCE, LPCSTR)
{
    return nullptr;
}

INT_PTR DialogBox(HINSTANCE, LPCTSTR, HWND, DLGPROC)
{
    return 0;
}

void ExitProcess(UINT exit_code)
{
    std::exit(static_cast<int>(exit_code));
}

int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    Uint32 flags = SDL_MESSAGEBOX_INFORMATION;
    switch (type & 0x000000F0U) {
    case MB_ICONSTOP:
        flags = SDL_MESSAGEBOX_ERROR;
        break;
    case MB_ICONEXCLAMATION:
        flags = SDL_MESSAGEBOX_WARNING;
        break;
    default:
        break;
    }

    if ((type & MB_YESNO) == MB_YESNO) {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDYES, "Yes"},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDNO, "No"},
        };
        const SDL_MessageBoxData data = {
            flags,
            nullptr,
            caption ? caption : "Red Alert",
            text ? text : "",
            2,
            buttons,
            nullptr,
        };
        int button_id = IDNO;
        if (SDL_ShowMessageBox(&data, &button_id) == 0) {
            return button_id;
        }
        return IDNO;
    }

    SDL_ShowSimpleMessageBox(flags, caption ? caption : "Red Alert", text ? text : "", nullptr);
    return IDOK;
}

void OutputDebugString(LPCSTR text)
{
    if (text) {
        SDL_Log("%s", text);
    }
}

DWORD GetLastError(void)
{
    std::scoped_lock lock(g_last_error_mutex);
    return g_last_error;
}

DWORD GetVersion(void)
{
    return 0;
}

void SetLastError(DWORD error_code)
{
    set_last_error(error_code);
}

UINT SetErrorMode(UINT mode)
{
    return g_error_mode.exchange(mode);
}

void Sleep(DWORD milliseconds)
{
    SDL_Delay(milliseconds);
}

DWORD GetTickCount(void)
{
    auto elapsed = std::chrono::steady_clock::now() - g_start_time;
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

void GetSystemTime(SYSTEMTIME* system_time)
{
    fill_current_system_time(system_time, false);
}

void GetLocalTime(SYSTEMTIME* system_time)
{
    fill_current_system_time(system_time, true);
}

void GlobalMemoryStatus(MEMORYSTATUS* memory_status)
{
    if (!memory_status) {
        return;
    }

    ZeroMemory(memory_status, sizeof(*memory_status));
    memory_status->dwLength = sizeof(*memory_status);

    const int total_ram_mb = SDL_GetSystemRAM();
    const uint64_t total_ram = total_ram_mb > 0 ? static_cast<uint64_t>(total_ram_mb) * 1024ULL * 1024ULL : 0ULL;
    const uint64_t avail_ram = total_ram / 2ULL;

    memory_status->dwTotalPhys = static_cast<DWORD>(total_ram);
    memory_status->dwAvailPhys = static_cast<DWORD>(avail_ram);
    memory_status->dwTotalPageFile = static_cast<DWORD>(total_ram);
    memory_status->dwAvailPageFile = static_cast<DWORD>(avail_ram);
    memory_status->dwTotalVirtual = static_cast<DWORD>(total_ram);
    memory_status->dwAvailVirtual = static_cast<DWORD>(avail_ram);
    memory_status->dwMemoryLoad = total_ram ? static_cast<DWORD>(((total_ram - avail_ram) * 100ULL) / total_ram) : 0U;
}

SHORT GetKeyState(int virtual_key)
{
    return SDL_GameInput_GetKeyState(virtual_key);
}

SHORT GetAsyncKeyState(int virtual_key)
{
    return SDL_GameInput_GetAsyncKeyState(virtual_key);
}

SHORT VkKeyScan(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<SHORT>(ch - 'a' + 'A');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<SHORT>((1 << 8) | ch);
    }
    return static_cast<SHORT>(ch);
}

UINT MapVirtualKey(UINT code, UINT)
{
    return code;
}

int ToAscii(UINT virtual_key_code, UINT, PBYTE key_state, LPWORD translated_char, UINT)
{
    if (!translated_char) {
        return 0;
    }

    const bool shift = key_state && (key_state[VK_SHIFT] & 0x80) != 0;
    const bool ctrl = key_state && (key_state[VK_CONTROL] & 0x80) != 0;
    const bool alt = key_state && (key_state[VK_MENU] & 0x80) != 0;
    const bool caps = key_state && (key_state[VK_CAPITAL] & 0x01) != 0;
    if (ctrl || alt) {
        return 0;
    }

    char ascii = '\0';
    if (virtual_key_code >= 'A' && virtual_key_code <= 'Z') {
        ascii = static_cast<char>((shift ^ caps) ? virtual_key_code : (virtual_key_code - 'A' + 'a'));
    } else if (virtual_key_code >= '0' && virtual_key_code <= '9') {
        static const char shifted_digits[] = ")!@#$%^&*(";
        ascii = shift ? shifted_digits[virtual_key_code - '0'] : static_cast<char>(virtual_key_code);
    } else {
        switch (virtual_key_code) {
        case VK_SPACE:
            ascii = ' ';
            break;
        case VK_TAB:
            ascii = '\t';
            break;
        case VK_RETURN:
            ascii = '\r';
            break;
        case VK_BACK:
            ascii = '\b';
            break;
        case VK_ESCAPE:
            ascii = 27;
            break;
        case 0xBA:
            ascii = shift ? ':' : ';';
            break;
        case 0xBB:
            ascii = shift ? '+' : '=';
            break;
        case 0xBC:
            ascii = shift ? '<' : ',';
            break;
        case 0xBD:
            ascii = shift ? '_' : '-';
            break;
        case 0xBE:
            ascii = shift ? '>' : '.';
            break;
        case 0xBF:
            ascii = shift ? '?' : '/';
            break;
        case 0xC0:
            ascii = shift ? '~' : '`';
            break;
        case 0xDB:
            ascii = shift ? '{' : '[';
            break;
        case 0xDC:
            ascii = shift ? '|' : '\\';
            break;
        case 0xDD:
            ascii = shift ? '}' : ']';
            break;
        case 0xDE:
            ascii = shift ? '"' : '\'';
            break;
        default:
            if (virtual_key_code < 0x80) {
                ascii = static_cast<char>(virtual_key_code);
            }
            break;
        }
    }

    if (ascii == '\0') {
        return 0;
    }

    *translated_char = static_cast<WORD>(static_cast<unsigned char>(ascii));
    return 1;
}

HANDLE SetCursor(HANDLE cursor)
{
    return cursor;
}

int ShowCursor(BOOL show)
{
    const bool was_visible = SDL_CursorVisible();
    if (show) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
    return was_visible ? 1 : 0;
}

MMRESULT timeBeginPeriod(UINT)
{
    return 0;
}

MMRESULT timeEndPeriod(UINT)
{
    return 0;
}

BOOL ClipCursor(const RECT* rect)
{
    HWND window = first_window();
    if (!window || !window->sdl_window) {
        SDL_GameInput_SetCursorClip(rect);
        return rect == nullptr ? TRUE : FALSE;
    }

    if (!rect) {
        const BOOL result = SDL_SetWindowMouseRect(window->sdl_window, nullptr) ? TRUE : FALSE;
        if (result) {
            SDL_GameInput_SetCursorClip(nullptr);
        }
        return result;
    }

    SDL_Rect mouse_rect{};
    mouse_rect.x = static_cast<int>(rect->left);
    mouse_rect.y = static_cast<int>(rect->top);
    mouse_rect.w = std::max<int>(0, rect->right - rect->left);
    mouse_rect.h = std::max<int>(0, rect->bottom - rect->top);
    if (!SDL_SetWindowMouseRect(window->sdl_window, &mouse_rect)) {
        return FALSE;
    }

    if (mouse_rect.w <= 0 || mouse_rect.h <= 0) {
        SDL_GameInput_SetCursorClip(rect);
        return TRUE;
    }

    float x = 0.0f;
    float y = 0.0f;
    SDL_GetMouseState(&x, &y);
    const float clamped_x = std::clamp(x, static_cast<float>(mouse_rect.x), static_cast<float>(mouse_rect.x + mouse_rect.w - (mouse_rect.w > 0 ? 1 : 0)));
    const float clamped_y = std::clamp(y, static_cast<float>(mouse_rect.y), static_cast<float>(mouse_rect.y + mouse_rect.h - (mouse_rect.h > 0 ? 1 : 0)));
    if (clamped_x != x || clamped_y != y) {
        SDL_WarpMouseInWindow(window->sdl_window, clamped_x, clamped_y);
    }

    SDL_GameInput_SetCursorClip(rect);
    return TRUE;
}

BOOL GetCursorPos(POINT* point)
{
    return SDL_GameInput_GetCursorPos(point);
}

DWORD GetCurrentThreadId(void)
{
    return g_next_thread_id.load();
}

HANDLE GetCurrentThread(void)
{
    return nullptr;
}

HANDLE GetCurrentProcess(void)
{
    return nullptr;
}

BOOL DuplicateHandle(HANDLE, HANDLE source_handle, HANDLE, HANDLE* target_handle, DWORD, BOOL, DWORD)
{
    if (!target_handle) return FALSE;
    *target_handle = source_handle;
    return TRUE;
}

BOOL SetThreadPriority(HANDLE, int)
{
    return TRUE;
}

HANDLE CreateThread(LPVOID, size_t, DWORD (WINAPI *start_address)(LPVOID), LPVOID parameter, DWORD, DWORD* thread_id)
{
    auto* handle = new ThreadHandle();
    handle->thread_id = g_next_thread_id.fetch_add(1);
    if (thread_id) {
        *thread_id = handle->thread_id;
    }
    handle->worker = std::thread([handle, start_address, parameter]() {
        if (start_address) {
            start_address(parameter);
        }
        {
            std::scoped_lock lock(handle->mutex);
            handle->finished = true;
        }
        handle->condition.notify_all();
    });
    return handle;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    if (!handle) return WAIT_FAILED;
    auto* base = static_cast<HandleBase*>(handle);
    switch (base->kind) {
        case HandleKind::Process: {
            auto* process = static_cast<ProcessHandle*>(base);
            if (process->exited) {
                return WAIT_OBJECT_0;
            }

            int exit_code = 0;
            if (milliseconds == INFINITE) {
                if (!SDL_WaitProcess(process->process, true, &exit_code)) {
                    return WAIT_FAILED;
                }
                process->exited = true;
                process->exit_code = static_cast<DWORD>(exit_code);
                return WAIT_OBJECT_0;
            }

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
            do {
                if (SDL_WaitProcess(process->process, false, &exit_code)) {
                    process->exited = true;
                    process->exit_code = static_cast<DWORD>(exit_code);
                    return WAIT_OBJECT_0;
                }
                SDL_Delay(1);
            } while (std::chrono::steady_clock::now() < deadline);
            return WAIT_TIMEOUT;
        }
        case HandleKind::Thread: {
            auto* thread = static_cast<ThreadHandle*>(base);
            std::unique_lock lock(thread->mutex);
            if (!thread->finished) {
                if (milliseconds == INFINITE) {
                    thread->condition.wait(lock, [thread]() { return thread->finished; });
                } else if (!thread->condition.wait_for(lock, std::chrono::milliseconds(milliseconds), [thread]() { return thread->finished; })) {
                    return WAIT_TIMEOUT;
                }
            }
            lock.unlock();
            if (thread->worker.joinable()) {
                thread->worker.join();
            }
            return WAIT_OBJECT_0;
        }
        case HandleKind::Event: {
            auto* event = static_cast<EventHandle*>(base);
            std::unique_lock lock(event->mutex);
            if (!event->signaled) {
                if (milliseconds == INFINITE) {
                    event->condition.wait(lock, [event]() { return event->signaled; });
                } else if (!event->condition.wait_for(lock, std::chrono::milliseconds(milliseconds), [event]() { return event->signaled; })) {
                    return WAIT_TIMEOUT;
                }
            }
            if (!event->manual) {
                event->signaled = false;
            }
            return WAIT_OBJECT_0;
        }
        case HandleKind::Mutex: {
            auto* mutex = static_cast<MutexHandle*>(base);
            if (milliseconds == INFINITE) {
                mutex->mutex.lock();
                return WAIT_OBJECT_0;
            }
            return mutex->mutex.try_lock_for(std::chrono::milliseconds(milliseconds)) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
        }
        default:
            return WAIT_FAILED;
    }
}

DWORD WaitForInputIdle(HANDLE handle, DWORD milliseconds)
{
    if (!handle) {
        return WAIT_FAILED;
    }

    if (milliseconds != INFINITE) {
        SDL_Delay(milliseconds > 10 ? 10 : milliseconds);
    }
    return WAIT_OBJECT_0;
}

BOOL GetExitCodeProcess(HANDLE handle, LPDWORD exit_code)
{
    if (!handle || !exit_code) {
        return FALSE;
    }

    auto* base = static_cast<HandleBase*>(handle);
    if (base->kind != HandleKind::Process) {
        return FALSE;
    }

    auto* process = static_cast<ProcessHandle*>(base);
    if (!process->exited) {
        int current_exit_code = 0;
        if (SDL_WaitProcess(process->process, false, &current_exit_code)) {
            process->exited = true;
            process->exit_code = static_cast<DWORD>(current_exit_code);
        }
    }

    *exit_code = process->exited ? process->exit_code : STILL_ACTIVE;
    return TRUE;
}

BOOL CloseHandle(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return TRUE;
    auto* base = static_cast<HandleBase*>(handle);
    if (base->kind == HandleKind::File) {
        auto* file = static_cast<FileHandle*>(base);
        if (file->io) {
            SDL_CloseIO(file->io);
        }
    } else if (base->kind == HandleKind::Process) {
        auto* process = static_cast<ProcessHandle*>(base);
        if (process->process) {
            SDL_DestroyProcess(process->process);
        }
    } else if (base->kind == HandleKind::Thread) {
        auto* thread = static_cast<ThreadHandle*>(base);
        if (thread->worker.joinable()) {
            thread->worker.join();
        }
    }
    delete base;
    return TRUE;
}

HANDLE CreateMutex(LPVOID, BOOL initial_owner, LPCSTR)
{
    auto* handle = new MutexHandle();
    if (initial_owner) {
        handle->mutex.lock();
    }
    return handle;
}

BOOL ReleaseMutex(HANDLE handle)
{
    if (!handle) return FALSE;
    auto* base = static_cast<HandleBase*>(handle);
    if (base->kind != HandleKind::Mutex) return FALSE;
    static_cast<MutexHandle*>(base)->mutex.unlock();
    return TRUE;
}

void InitializeCriticalSection(CRITICAL_SECTION* critical_section)
{
    if (critical_section) {
        critical_section->mutex = new std::recursive_mutex();
    }
}

void DeleteCriticalSection(CRITICAL_SECTION* critical_section)
{
    if (critical_section && critical_section->mutex) {
        delete critical_section->mutex;
        critical_section->mutex = nullptr;
    }
}

void EnterCriticalSection(CRITICAL_SECTION* critical_section)
{
    if (critical_section && critical_section->mutex) {
        critical_section->mutex->lock();
    }
}

void LeaveCriticalSection(CRITICAL_SECTION* critical_section)
{
    if (critical_section && critical_section->mutex) {
        critical_section->mutex->unlock();
    }
}

HANDLE CreateEvent(LPVOID, BOOL manual_reset, BOOL initial_state, LPCSTR name)
{
    auto* handle = new EventHandle(manual_reset != FALSE, initial_state != FALSE);
    if (name && *name) {
        std::scoped_lock lock(g_named_event_mutex);
        g_named_events[name] = handle;
    }
    return handle;
}

HANDLE OpenEvent(DWORD, BOOL, LPCSTR name)
{
    std::scoped_lock lock(g_named_event_mutex);
    auto it = g_named_events.find(name ? name : "");
    return it == g_named_events.end() ? nullptr : it->second;
}

BOOL SetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    auto* event = static_cast<EventHandle*>(handle);
    std::scoped_lock lock(event->mutex);
    event->signaled = true;
    event->condition.notify_all();
    return TRUE;
}

BOOL ResetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    auto* event = static_cast<EventHandle*>(handle);
    std::scoped_lock lock(event->mutex);
    event->signaled = false;
    return TRUE;
}

HANDLE CreateFile(LPCSTR file_name, DWORD desired_access, DWORD, LPVOID, DWORD creation_disposition, DWORD, HANDLE)
{
    auto mode = create_file_mode(desired_access, creation_disposition);
    const std::string normalized_path = normalize_compat_path(file_name);
    SDL_IOStream* io = SDL_IOFromFile(normalized_path.c_str(), mode.c_str());
    if (!io) {
        set_last_error(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    return new FileHandle(io, normalized_path);
}

BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD number_of_bytes_to_read, LPDWORD number_of_bytes_read, LPOVERLAPPED)
{
    if (number_of_bytes_read) *number_of_bytes_read = 0;
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    auto* file = static_cast<FileHandle*>(handle);
    const size_t read = SDL_ReadIO(file->io, buffer, number_of_bytes_to_read);
    if (number_of_bytes_read) {
        *number_of_bytes_read = static_cast<DWORD>(read);
    }
    return TRUE;
}

BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD number_of_bytes_to_write, LPDWORD number_of_bytes_written, LPOVERLAPPED)
{
    if (number_of_bytes_written) *number_of_bytes_written = 0;
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    auto* file = static_cast<FileHandle*>(handle);
    const size_t written = SDL_WriteIO(file->io, buffer, number_of_bytes_to_write);
    if (number_of_bytes_written) {
        *number_of_bytes_written = static_cast<DWORD>(written);
    }
    return written == number_of_bytes_to_write;
}

BOOL GetCommState(HANDLE, DCB* dcb)
{
    if (!dcb) return FALSE;
    ZeroMemory(dcb, sizeof(*dcb));
    dcb->DCBlength = sizeof(*dcb);
    dcb->ByteSize = 8;
    dcb->StopBits = 0;
    dcb->fBinary = TRUE;
    return TRUE;
}

BOOL SetCommState(HANDLE, const DCB*)
{
    return TRUE;
}

BOOL SetCommTimeouts(HANDLE, const COMMTIMEOUTS*)
{
    return TRUE;
}

BOOL SetupComm(HANDLE, DWORD, DWORD)
{
    return TRUE;
}

BOOL PurgeComm(HANDLE, DWORD)
{
    return TRUE;
}

BOOL EscapeCommFunction(HANDLE, DWORD)
{
    return TRUE;
}

BOOL SetCommBreak(HANDLE handle)
{
    return EscapeCommFunction(handle, SETBREAK);
}

BOOL ClearCommBreak(HANDLE handle)
{
    return EscapeCommFunction(handle, CLRBREAK);
}

BOOL GetOverlappedResult(HANDLE, LPOVERLAPPED, LPDWORD number_of_bytes_transferred, BOOL)
{
    if (number_of_bytes_transferred) {
        *number_of_bytes_transferred = 0;
    }
    return TRUE;
}

BOOL ClearCommError(HANDLE, LPDWORD errors, COMSTAT* status)
{
    if (errors) {
        *errors = 0;
    }
    if (status) {
        ZeroMemory(status, sizeof(*status));
    }
    return TRUE;
}

BOOL GetCommModemStatus(HANDLE, LPDWORD modem_status)
{
    if (modem_status) {
        *modem_status = 0;
    }
    return TRUE;
}

DWORD SetFilePointer(HANDLE handle, LONG distance_to_move, LONG*, DWORD move_method)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return 0xffffffffu;
    auto* file = static_cast<FileHandle*>(handle);
    SDL_IOWhence whence = SDL_IO_SEEK_SET;
    if (move_method == FILE_CURRENT) whence = SDL_IO_SEEK_CUR;
    if (move_method == FILE_END) whence = SDL_IO_SEEK_END;
    Sint64 result = SDL_SeekIO(file->io, distance_to_move, whence);
    return result < 0 ? 0xffffffffu : static_cast<DWORD>(result);
}

DWORD GetFileSize(HANDLE handle, DWORD*)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return 0xffffffffu;
    auto* file = static_cast<FileHandle*>(handle);
    Sint64 current = SDL_TellIO(file->io);
    Sint64 end = SDL_SeekIO(file->io, 0, SDL_IO_SEEK_END);
    SDL_SeekIO(file->io, current, SDL_IO_SEEK_SET);
    return end < 0 ? 0xffffffffu : static_cast<DWORD>(end);
}

BOOL GetFileInformationByHandle(HANDLE handle, BY_HANDLE_FILE_INFORMATION* file_information)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || !file_information) {
        return FALSE;
    }

    auto* file = static_cast<FileHandle*>(handle);
    if (file->path.empty()) {
        return FALSE;
    }

    std::error_code ec;
    const std::filesystem::path path(file->path);
    const auto status = std::filesystem::status(path, ec);
    if (ec) {
        return FALSE;
    }

    ZeroMemory(file_information, sizeof(*file_information));
    file_information->dwFileAttributes = std::filesystem::is_directory(status) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

    const uintmax_t size = std::filesystem::is_regular_file(status) ? std::filesystem::file_size(path, ec) : 0;
    if (ec) {
        return FALSE;
    }
    file_information->nFileSizeLow = static_cast<DWORD>(size & 0xffffffffULL);
    file_information->nFileSizeHigh = static_cast<DWORD>(size >> 32);
    file_information->nNumberOfLinks = 1;

    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return FALSE;
    }

    const FILETIME last_write_time = system_clock_to_filetime(filesystem_to_system_clock(write_time));
    file_information->ftCreationTime = last_write_time;
    file_information->ftLastAccessTime = last_write_time;
    file_information->ftLastWriteTime = last_write_time;
    return TRUE;
}

BOOL GetFileTime(HANDLE handle, FILETIME* creation_time, FILETIME* last_access_time, FILETIME* last_write_time)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    auto* file = static_cast<FileHandle*>(handle);
    if (file->path.empty()) {
        return FALSE;
    }

    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(std::filesystem::path(file->path), ec);
    if (ec) {
        return FALSE;
    }

    const FILETIME file_time = system_clock_to_filetime(filesystem_to_system_clock(write_time));
    if (creation_time) {
        *creation_time = file_time;
    }
    if (last_access_time) {
        *last_access_time = file_time;
    }
    if (last_write_time) {
        *last_write_time = file_time;
    }
    return TRUE;
}

BOOL FileTimeToDosDateTime(const FILETIME* file_time, LPWORD dos_date, LPWORD dos_time)
{
    if (!file_time || !dos_date || !dos_time) {
        return FALSE;
    }

    const std::time_t raw_time = std::chrono::system_clock::to_time_t(filetime_to_system_clock(*file_time));
    std::tm time_info{};
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    localtime_r(&raw_time, &time_info);
#else
    const std::tm* local_time = std::localtime(&raw_time);
    if (!local_time) {
        return FALSE;
    }
    time_info = *local_time;
#endif

    if (time_info.tm_year < 80) {
        time_info.tm_year = 80;
        time_info.tm_mon = 0;
        time_info.tm_mday = 1;
        time_info.tm_hour = 0;
        time_info.tm_min = 0;
        time_info.tm_sec = 0;
    }

    *dos_date = static_cast<WORD>(((time_info.tm_year - 80) << 9)
        | ((time_info.tm_mon + 1) << 5)
        | time_info.tm_mday);
    *dos_time = static_cast<WORD>((time_info.tm_hour << 11)
        | (time_info.tm_min << 5)
        | (time_info.tm_sec / 2));
    return TRUE;
}

BOOL DosDateTimeToFileTime(WORD dos_date, WORD dos_time, FILETIME* file_time)
{
    if (!file_time) {
        return FALSE;
    }

    std::tm time_info{};
    time_info.tm_year = ((dos_date >> 9) & 0x7f) + 80;
    time_info.tm_mon = ((dos_date >> 5) & 0x0f) - 1;
    time_info.tm_mday = dos_date & 0x1f;
    time_info.tm_hour = (dos_time >> 11) & 0x1f;
    time_info.tm_min = (dos_time >> 5) & 0x3f;
    time_info.tm_sec = (dos_time & 0x1f) * 2;
    time_info.tm_isdst = -1;

    const std::time_t raw_time = std::mktime(&time_info);
    if (raw_time == static_cast<std::time_t>(-1)) {
        return FALSE;
    }

    *file_time = system_clock_to_filetime(std::chrono::system_clock::from_time_t(raw_time));
    return TRUE;
}

BOOL SetFileTime(HANDLE handle, const FILETIME*, const FILETIME* last_access_time, const FILETIME* last_write_time)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    auto* file = static_cast<FileHandle*>(handle);
    if (file->path.empty()) {
        return FALSE;
    }

    const FILETIME* source_time = last_write_time ? last_write_time : last_access_time;
    if (!source_time) {
        return FALSE;
    }

    std::error_code ec;
    std::filesystem::last_write_time(
        std::filesystem::path(file->path),
        system_to_filesystem_clock(filetime_to_system_clock(*source_time)),
        ec);
    return ec ? FALSE : TRUE;
}

BOOL DeleteFile(LPCSTR file_name)
{
    std::error_code ec;
    return std::filesystem::remove(normalize_compat_path(file_name), ec) ? TRUE : FALSE;
}

UINT GetDriveType(LPCSTR root_path_name)
{
    if (!root_path_name || !*root_path_name) {
        return DRIVE_NO_ROOT_DIR;
    }

    if (virtual_cd_index_for_drive_letter(root_path_name[0]) >= 0) {
        return DRIVE_CDROM;
    }

    const std::string normalized_path = normalize_compat_path(root_path_name);
    if (!compat_path_exists(normalized_path.c_str())) {
        return DRIVE_NO_ROOT_DIR;
    }

    if (normalized_path.find("cdrom") != std::string::npos || normalized_path.find("CDROM") != std::string::npos) {
        return DRIVE_CDROM;
    }

    return DRIVE_FIXED;
}

BOOL GetVolumeInformation(LPCSTR root_path_name, LPSTR volume_name_buffer, DWORD volume_name_size, DWORD* volume_serial_number,
    DWORD* maximum_component_length, DWORD* file_system_flags, LPSTR file_system_name_buffer, DWORD file_system_name_size)
{
    if (!root_path_name || !*root_path_name) {
        return FALSE;
    }

    int virtual_cd_index = -1;
    std::string virtual_path;
    std::filesystem::path path;
    if (resolve_virtual_cd_path(root_path_name, virtual_path, &virtual_cd_index) && virtual_cd_index >= 0) {
        if (volume_name_buffer && volume_name_size > 0) {
            std::snprintf(volume_name_buffer, volume_name_size, "CD%d", virtual_cd_index + 1);
        }
        if (volume_serial_number) {
            *volume_serial_number = 0;
        }
        if (maximum_component_length) {
            *maximum_component_length = 255;
        }
        if (file_system_flags) {
            *file_system_flags = 0;
        }
        if (file_system_name_buffer && file_system_name_size > 0) {
            std::snprintf(file_system_name_buffer, file_system_name_size, "%s", "SDLFS");
        }
        return TRUE;
    }

    std::error_code ec;
    path = std::filesystem::path(normalize_compat_path(root_path_name));
    if (!std::filesystem::exists(path, ec)) {
        return FALSE;
    }

    std::string volume_name = path.filename().string();
    if (volume_name.empty()) {
        volume_name = path.root_name().string();
    }
    if (volume_name.empty()) {
        volume_name = path.string();
    }

    if (volume_name_buffer && volume_name_size > 0) {
        std::snprintf(volume_name_buffer, volume_name_size, "%s", volume_name.c_str());
    }
    if (volume_serial_number) {
        *volume_serial_number = 0;
    }
    if (maximum_component_length) {
        *maximum_component_length = 255;
    }
    if (file_system_flags) {
        *file_system_flags = 0;
    }
    if (file_system_name_buffer && file_system_name_size > 0) {
        std::snprintf(file_system_name_buffer, file_system_name_size, "%s", "SDLFS");
    }

    return TRUE;
}

HANDLE FindFirstFile(LPCSTR file_name, LPWIN32_FIND_DATA find_file_data)
{
    if (file_name == nullptr || find_file_data == nullptr) {
        set_last_error(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    const std::string pattern = normalize_compat_path(file_name);
    const std::string directory = translate_find_directory(find_parent_directory(pattern));
    const std::string wildcard = find_leaf_name(pattern);
    auto regex = std::regex(wildcard_to_regex(wildcard), std::regex::icase);
    auto* handle = new SearchHandle();

#if RA_REAL_WINDOWS
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        const std::string name = entry.path().filename().string();
        if (std::regex_match(name, regex)) {
            SearchMatch match;
            match.name = name;
            if (entry.is_directory(ec) && !ec) {
                match.attributes |= FILE_ATTRIBUTE_DIRECTORY;
            }
            if (match.attributes == 0) {
                match.attributes = FILE_ATTRIBUTE_NORMAL;
            }
            if (entry.is_regular_file(ec) && !ec) {
                const auto size = entry.file_size(ec);
                if (!ec) {
                    match.size_low = static_cast<DWORD>(size & 0xffffffffu);
                    match.size_high = static_cast<DWORD>((size >> 32) & 0xffffffffu);
                }
            }
            handle->matches.push_back(std::move(match));
        }
    }
#else
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        delete handle;
        set_last_error(ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    while (dirent* entry = readdir(dir)) {
        if (!std::regex_match(entry->d_name, regex)) {
            continue;
        }

        const std::string full_path = join_find_path(directory, entry->d_name);
        struct stat status {};
        if (stat(full_path.c_str(), &status) != 0) {
            continue;
        }

        handle->matches.push_back(make_search_match(entry->d_name, status));
    }

    closedir(dir);
#endif

    if (handle->matches.empty()) {
        delete handle;
        set_last_error(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    fill_find_data(handle->matches[0], find_file_data);
    handle->index = 1;
    set_last_error(ERROR_SUCCESS);
    return handle;
}

BOOL FindNextFile(HANDLE find_file, LPWIN32_FIND_DATA find_file_data)
{
    if (!find_file || find_file == INVALID_HANDLE_VALUE) {
        set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    auto* handle = static_cast<SearchHandle*>(find_file);
    if (handle->index >= handle->matches.size()) {
        set_last_error(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    set_last_error(ERROR_SUCCESS);
    return fill_find_data(handle->matches[handle->index++], find_file_data);
}

BOOL FindClose(HANDLE find_file)
{
    return CloseHandle(find_file);
}

HGLOBAL GlobalAlloc(UINT, size_t bytes)
{
    return new GlobalHandle(bytes ? bytes : 1);
}

LPVOID GlobalLock(HGLOBAL memory)
{
    if (!memory) return nullptr;
    auto* handle = static_cast<GlobalHandle*>(memory);
    return handle->bytes.data();
}

BOOL GlobalUnlock(HGLOBAL)
{
    return TRUE;
}

HGLOBAL GlobalFree(HGLOBAL memory)
{
    if (memory) {
        delete static_cast<GlobalHandle*>(memory);
    }
    return nullptr;
}

HMODULE LoadLibrary(LPCSTR file_name)
{
    return SDL_LoadObject(file_name);
}

FARPROC GetProcAddress(HMODULE module, LPCSTR proc_name)
{
    return reinterpret_cast<FARPROC>(SDL_LoadFunction(reinterpret_cast<SDL_SharedObject*>(module), proc_name));
}

BOOL FreeLibrary(HMODULE module)
{
    if (!module) return FALSE;
    SDL_UnloadObject(reinterpret_cast<SDL_SharedObject*>(module));
    return TRUE;
}

DWORD GetModuleFileName(HINSTANCE, LPSTR file_name, DWORD size)
{
    if (!file_name || size == 0) return 0;
    std::string path;

#if defined(__linux__)
    std::error_code error;
    const std::filesystem::path executable_path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        path = executable_path.string();
    }
#endif

    if (path.empty()) {
        const char* base_path = SDL_GetBasePath();
        path = (base_path != nullptr) ? base_path : "./";
    }

    std::snprintf(file_name, size, "%s", path.c_str());
    return static_cast<DWORD>(std::strlen(file_name));
}

HANDLE CreateFileMapping(HANDLE, LPVOID, DWORD, DWORD maximum_size_high, DWORD maximum_size_low, LPCSTR)
{
    auto* mapping = new MappingHandle();
    const uint64_t size = (static_cast<uint64_t>(maximum_size_high) << 32) | maximum_size_low;
    mapping->bytes.resize(size ? static_cast<size_t>(size) : 4096);
    return mapping;
}

LPVOID MapViewOfFile(HANDLE file_mapping_object, DWORD, DWORD, DWORD file_offset_low, size_t)
{
    if (!file_mapping_object) return nullptr;
    auto* mapping = static_cast<MappingHandle*>(file_mapping_object);
    if (file_offset_low >= mapping->bytes.size()) return nullptr;
    return mapping->bytes.data() + file_offset_low;
}

BOOL UnmapViewOfFile(LPCVOID)
{
    return TRUE;
}

LONG RegOpenKeyEx(HKEY, LPCSTR sub_key, DWORD, DWORD, HKEY* result)
{
    if (!result) return ERROR_INVALID_HANDLE;
    *result = new std::string(sub_key ? sub_key : "");
    return ERROR_SUCCESS;
}

LONG RegQueryInfoKey(HKEY, LPSTR, LPDWORD, LPDWORD, LPDWORD sub_keys,
    LPDWORD max_sub_key_len, LPDWORD max_class_len, LPDWORD values, LPDWORD max_value_name_len,
    LPDWORD max_value_len, LPDWORD security_descriptor, FILETIME* last_write_time)
{
    if (sub_keys) *sub_keys = 0;
    if (max_sub_key_len) *max_sub_key_len = 0;
    if (max_class_len) *max_class_len = 0;
    if (values) *values = 0;
    if (max_value_name_len) *max_value_name_len = 0;
    if (max_value_len) *max_value_len = 0;
    if (security_descriptor) *security_descriptor = 0;
    if (last_write_time) ZeroMemory(last_write_time, sizeof(*last_write_time));
    return ERROR_SUCCESS;
}

LONG RegEnumKeyEx(HKEY, DWORD, LPSTR, DWORD*, DWORD*, LPSTR, DWORD*, FILETIME*)
{
    return ERROR_FILE_NOT_FOUND;
}

LONG RegQueryValue(HKEY key, LPCSTR sub_key, LPSTR data, LPLONG data_size)
{
    if (!data_size) {
        return ERROR_INVALID_HANDLE;
    }

    DWORD size = static_cast<DWORD>(*data_size);
    const LONG result = RegQueryValueEx(key, sub_key, nullptr, nullptr, reinterpret_cast<LPBYTE>(data), &size);
    *data_size = static_cast<LONG>(size);
    return result;
}

LONG RegQueryValueEx(HKEY key, LPCSTR value_name, DWORD*, DWORD* type, LPBYTE data, DWORD* data_size)
{
    if (!key || !data_size) return ERROR_INVALID_HANDLE;
    const char* name = value_name ? value_name : "";

    auto write_string_value = [&](const char* value) -> LONG {
        if (type) *type = REG_SZ;
        const size_t value_size = std::strlen(value) + 1;
        if (!data || *data_size < value_size) {
            *data_size = static_cast<DWORD>(value_size);
            return ERROR_SUCCESS;
        }
        std::memcpy(data, value, value_size);
        *data_size = static_cast<DWORD>(value_size);
        return ERROR_SUCCESS;
    };

    auto write_dword_value = [&](DWORD value) -> LONG {
        if (type) *type = REG_DWORD;
        if (!data || *data_size < sizeof(value)) {
            *data_size = sizeof(value);
            return ERROR_SUCCESS;
        }
        std::memcpy(data, &value, sizeof(value));
        *data_size = sizeof(value);
        return ERROR_SUCCESS;
    };

    if (std::strcmp(name, "InstallPath") == 0) {
        const char* base_path = SDL_GetBasePath();
        return write_string_value(base_path ? base_path : "./");
    }

    if (std::strcmp(name, "DVD") == 0 ||
        std::strcmp(name, "CStrikeInstalled") == 0 ||
        std::strcmp(name, "AftermathInstalled") == 0 ||
        std::strcmp(name, "WolapiInstallComplete") == 0 ||
        std::strcmp(name, "WOLAPI Find Enabled") == 0 ||
        std::strcmp(name, "WOLAPI Page Enabled") == 0 ||
        std::strcmp(name, "WOLAPI Lang Filter") == 0 ||
        std::strcmp(name, "WOLAPI Show All Games") == 0) {
        return write_dword_value(0);
    }

    return ERROR_FILE_NOT_FOUND;
}

LONG RegCloseKey(HKEY key)
{
    delete static_cast<std::string*>(key);
    return ERROR_SUCCESS;
}

BOOL CreateProcess(LPCSTR application_name, LPSTR command_line, LPVOID, LPVOID, BOOL, DWORD, LPVOID, LPCSTR,
    STARTUPINFO*, PROCESS_INFORMATION* process_information)
{
    if (process_information) {
        ZeroMemory(process_information, sizeof(*process_information));
    }

    std::vector<std::string> owned_arguments;
    if (application_name && *application_name) {
        owned_arguments.emplace_back(application_name);
    }

    if (command_line && *command_line) {
        std::vector<std::string> parsed_arguments = split_command_line(command_line);
        if (!parsed_arguments.empty() && !owned_arguments.empty() && parsed_arguments.front() == owned_arguments.front()) {
            parsed_arguments.erase(parsed_arguments.begin());
        }
        owned_arguments.insert(owned_arguments.end(), parsed_arguments.begin(), parsed_arguments.end());
    }

    if (owned_arguments.empty()) {
        return FALSE;
    }

    std::vector<const char*> arguments;
    arguments.reserve(owned_arguments.size() + 1);
    for (const std::string& argument : owned_arguments) {
        arguments.push_back(argument.c_str());
    }
    arguments.push_back(nullptr);

    SDL_Process* process = SDL_CreateProcess(arguments.data(), false);
    if (!process) {
        return FALSE;
    }

    auto* process_handle = new ProcessHandle(process);
    if (process_information) {
        process_information->hProcess = process_handle;
        process_information->hThread = nullptr;
        process_information->dwProcessId = 0;
        process_information->dwThreadId = 0;
    }
    return TRUE;
}

HDC BeginPaint(HWND, PAINTSTRUCT* paint)
{
    if (paint) {
        ZeroMemory(paint, sizeof(*paint));
    }
    return nullptr;
}

BOOL EndPaint(HWND, const PAINTSTRUCT*)
{
    return TRUE;
}

HDC GetDC(HWND)
{
    return nullptr;
}

INT ReleaseDC(HWND, HDC)
{
    return 1;
}

HPALETTE CreatePalette(const LOGPALETTE* palette)
{
    return palette ? new std::vector<std::byte>(sizeof(LOGPALETTE) + (palette->palNumEntries * sizeof(PALETTEENTRY))) : nullptr;
}

HPALETTE SelectPalette(HDC, HPALETTE palette, BOOL)
{
    return palette;
}

UINT RealizePalette(HDC)
{
    return 256;
}

BOOL DeleteObject(HGDIOBJ object)
{
    delete static_cast<std::vector<std::byte>*>(object);
    return TRUE;
}

int StretchDIBits(HDC, int, int, int, int, int, int, int, int, const VOID*, const BITMAPINFO*, UINT, DWORD)
{
    return 1;
}

int SetDIBitsToDevice(HDC, int, int, DWORD width, DWORD height, int, int, UINT, UINT, const VOID*, const BITMAPINFO*, UINT)
{
    return static_cast<int>(width * height);
}

MMRESULT timeSetEvent(UINT delay, UINT, LPTIMECALLBACK callback, DWORD user, UINT event_type)
{
    if (!callback) {
        return 0;
    }

    const UINT timer_id = g_next_timer_id.fetch_add(1);
    auto timer = std::make_unique<TimerHandle>();
    TimerHandle* timer_ptr = timer.get();
    timer->worker = std::thread([delay, callback, user, event_type, timer_id, timer_ptr]() {
        if (event_type == TIME_ONESHOT) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            if (timer_ptr->active.load()) {
                callback(timer_id, 0, user, 0, 0);
            }
            timer_ptr->active.store(false);
            return;
        }

        while (timer_ptr->active.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            if (!timer_ptr->active.load()) {
                break;
            }
            callback(timer_id, 0, user, 0, 0);
        }
    });

    std::scoped_lock lock(g_timer_mutex);
    g_timers.emplace(timer_id, std::move(timer));
    return timer_id;
}

MMRESULT timeKillEvent(UINT timer_id)
{
    std::unique_ptr<TimerHandle> timer;
    {
        std::scoped_lock lock(g_timer_mutex);
        auto it = g_timers.find(timer_id);
        if (it == g_timers.end()) {
            return 1;
        }
        timer = std::move(it->second);
        g_timers.erase(it);
    }

    timer->active.store(false);
    if (timer->worker.joinable()) {
        timer->worker.join();
    }
    return 0;
}

MCIERROR mciSendCommand(MCIDEVICEID device_id, UINT message, DWORD, DWORD params)
{
    switch (message) {
        case MCI_SYSINFO: {
            auto* sysinfo = reinterpret_cast<MCI_SYSINFO_PARMS*>(static_cast<uintptr_t>(params));
            if (sysinfo) {
                if (sysinfo->lpstrReturn && sysinfo->dwRetSize > 0) {
                    std::snprintf(sysinfo->lpstrReturn, sysinfo->dwRetSize, "%s", "SDL3");
                }
                sysinfo->dwNumber = 1;
            }
            return 0;
        }
        case MCI_INFO: {
            auto* info = reinterpret_cast<MCI_INFO_PARMS*>(static_cast<uintptr_t>(params));
            if (info && info->lpstrReturn && info->dwRetSize > 0) {
                std::snprintf(info->lpstrReturn, info->dwRetSize, "%s", "SDL3 Multimedia");
            }
            return 0;
        }
        case MCI_OPEN: {
            auto* open = reinterpret_cast<MCI_OPEN_PARMS*>(static_cast<uintptr_t>(params));
            if (open) {
                open->wDeviceID = device_id ? device_id : 1;
            }
            return 0;
        }
        case MCI_GETDEVCAPS: {
            auto* caps = reinterpret_cast<MCI_GETDEVCAPS_PARMS*>(static_cast<uintptr_t>(params));
            if (caps) {
                if (caps->dwItem == MCI_GETDEVCAPS_DEVICE_TYPE) {
                    caps->dwReturn = MCI_DEVTYPE_DIGITAL_VIDEO;
                } else {
                    caps->dwReturn = 1;
                }
            }
            return 0;
        }
        case MCI_CLOSE:
        case MCI_PLAY:
        case MCI_PAUSE:
        case MCI_WHERE:
        case MCI_PUT:
        case MCI_WINDOW:
        case MCI_BREAK:
            return 0;
        default:
            return 1;
    }
}

} // extern "C"
