#include "win32_compat.h"
#include "sdl_fs.h"
#include "SDLINPUT.H"

#include <SDL3/SDL_loadso.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace {

enum class HandleKind {
    None,
    File,
    Event,
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

struct EventHandle final : HandleBase {
    EventHandle(bool manual_reset, bool initial_state) : HandleBase(HandleKind::Event), manual(manual_reset), signaled(initial_state) {}
    std::mutex mutex;
    std::condition_variable condition;
    bool manual;
    bool signaled;
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
std::mutex g_named_event_mutex;
std::unordered_map<std::string, EventHandle*> g_named_events;
std::chrono::steady_clock::time_point g_start_time = std::chrono::steady_clock::now();
std::atomic<UINT> g_error_mode{0};

bool compat_uses_wayland_video_driver()
{
    const char* video_driver = SDL_GetCurrentVideoDriver();
    return video_driver != nullptr && SDL_strcasecmp(video_driver, "wayland") == 0;
}

} // end anonymous namespace

namespace {

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

bool RA_GetPresentationRect(HWND window, SDL_FRect* rect)
{
    if (!window || !window->sdl_window || !rect || window->width <= 0 || window->height <= 0) {
        return false;
    }

    int window_width = 0;
    int window_height = 0;
    if (!SDL_GetWindowSize(window->sdl_window, &window_width, &window_height) || window_width <= 0 || window_height <= 0) {
        return false;
    }

    const float logical_width = static_cast<float>(window->width);
    const float logical_height = static_cast<float>(window->height);
    const float scale = std::min(static_cast<float>(window_width) / logical_width, static_cast<float>(window_height) / logical_height);
    rect->w = std::max(1.0f, logical_width * scale);
    rect->h = std::max(1.0f, logical_height * scale);
    rect->x = (static_cast<float>(window_width) - rect->w) * 0.5f;
    rect->y = (static_cast<float>(window_height) - rect->h) * 0.5f;
    return true;
}

bool RA_WindowToGamePoint(HWND window, float window_x, float window_y, int* game_x, int* game_y)
{
    if (!game_x || !game_y) {
        return false;
    }

    SDL_FRect presentation{};
    if (!RA_GetPresentationRect(window, &presentation) || presentation.w <= 0.0f || presentation.h <= 0.0f || !window
        || window->width <= 0 || window->height <= 0) {
        *game_x = static_cast<int>(window_x);
        *game_y = static_cast<int>(window_y);
        return false;
    }

    const float normalized_x = (window_x - presentation.x) / presentation.w;
    const float normalized_y = (window_y - presentation.y) / presentation.h;
    int mapped_x = static_cast<int>(normalized_x * static_cast<float>(window->width));
    int mapped_y = static_cast<int>(normalized_y * static_cast<float>(window->height));
    mapped_x = std::clamp(mapped_x, 0, window->width - 1);
    mapped_y = std::clamp(mapped_y, 0, window->height - 1);
    *game_x = mapped_x;
    *game_y = mapped_y;
    return true;
}

bool RA_GameRectToWindowRect(HWND window, const RECT* game_rect, SDL_Rect* window_rect)
{
    if (!game_rect || !window_rect || !window || window->width <= 0 || window->height <= 0) {
        return false;
    }

    SDL_FRect presentation{};
    if (!RA_GetPresentationRect(window, &presentation) || presentation.w <= 0.0f || presentation.h <= 0.0f) {
        return false;
    }

    const float scale_x = presentation.w / static_cast<float>(window->width);
    const float scale_y = presentation.h / static_cast<float>(window->height);
    const float left = presentation.x + static_cast<float>(game_rect->left) * scale_x;
    const float top = presentation.y + static_cast<float>(game_rect->top) * scale_y;
    const float right = presentation.x + static_cast<float>(game_rect->right) * scale_x;
    const float bottom = presentation.y + static_cast<float>(game_rect->bottom) * scale_y;
    window_rect->x = static_cast<int>(std::floor(left));
    window_rect->y = static_cast<int>(std::floor(top));
    window_rect->w = std::max(0, static_cast<int>(std::ceil(right)) - window_rect->x);
    window_rect->h = std::max(0, static_cast<int>(std::ceil(bottom)) - window_rect->y);
    return true;
}

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
    window->sdl_window = SDL_CreateWindow(window->title.c_str(), window->width, window->height, SDL_WINDOW_RESIZABLE);
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

BOOL ClipCursor(const RECT* rect)
{
    HWND window = first_window();
    if (!window || !window->sdl_window) {
        SDL_GameInput_SetCursorClip(rect);
        return rect == nullptr ? TRUE : FALSE;
    }

    const bool wayland_windowed = compat_uses_wayland_video_driver()
        && (SDL_GetWindowFlags(window->sdl_window) & SDL_WINDOW_FULLSCREEN) == 0;

    if (!rect) {
        if (wayland_windowed) {
            SDL_SetWindowMouseRect(window->sdl_window, nullptr);
            SDL_GameInput_SetCursorClip(nullptr);
            return TRUE;
        }

        const BOOL result = SDL_SetWindowMouseRect(window->sdl_window, nullptr) ? TRUE : FALSE;
        if (result) {
            SDL_GameInput_SetCursorClip(nullptr);
        }
        return result;
    }

    RECT surface_rect = *rect;
    RECT viewport_rect{};
    if (SDL_GameInput_GetViewportRect(&viewport_rect)) {
        surface_rect.left += viewport_rect.left;
        surface_rect.top += viewport_rect.top;
        surface_rect.right += viewport_rect.left;
        surface_rect.bottom += viewport_rect.top;
    }

    SDL_Rect mouse_rect{};
    if (!RA_GameRectToWindowRect(window, &surface_rect, &mouse_rect)) {
        mouse_rect.x = static_cast<int>(surface_rect.left);
        mouse_rect.y = static_cast<int>(surface_rect.top);
        mouse_rect.w = std::max<int>(0, surface_rect.right - surface_rect.left);
        mouse_rect.h = std::max<int>(0, surface_rect.bottom - surface_rect.top);
    }

    if (mouse_rect.w <= 0 || mouse_rect.h <= 0) {
        SDL_SetWindowMouseRect(window->sdl_window, nullptr);
        SDL_GameInput_SetCursorClip(rect);
        return TRUE;
    }

    if (wayland_windowed) {
        /*
        ** Hyprland/Wayland compositor move/resize gestures rely on the real
        ** pointer remaining unconstrained. Keep only the game's logical clip.
        */
        SDL_SetWindowMouseRect(window->sdl_window, nullptr);
        SDL_GameInput_SetCursorClip(rect);
        return TRUE;
    }

    if (!SDL_SetWindowMouseRect(window->sdl_window, &mouse_rect)) {
        return FALSE;
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

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    if (!handle) return WAIT_FAILED;
    auto* base = static_cast<HandleBase*>(handle);
    switch (base->kind) {
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
        default:
            return WAIT_FAILED;
    }
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
    }
    delete base;
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
    const std::string normalized_path = WWFS_NormalizePath(file_name);
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

UINT GetDriveType(LPCSTR root_path_name)
{
    if (!root_path_name || !*root_path_name) {
        return DRIVE_NO_ROOT_DIR;
    }

    if (WWFS_GetVirtualCDIndexForDriveLetter(root_path_name[0]) >= 0) {
        return DRIVE_CDROM;
    }

    const std::string normalized_path = WWFS_NormalizePath(root_path_name);
    if (!WWFS_GetPathInfo(normalized_path.c_str(), nullptr)) {
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
    std::string normalized_path = WWFS_NormalizePath(root_path_name);
    if (WWFS_ResolveVirtualCDPath(root_path_name, virtual_path, &virtual_cd_index) && virtual_cd_index >= 0) {
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

    if (!WWFS_GetPathInfo(normalized_path.c_str(), nullptr)) {
        return FALSE;
    }

    // Extract the last path component as the "volume name"
    std::string volume_name;
    {
        size_t pos = normalized_path.find_last_of('/');
        if (pos != std::string::npos && pos + 1 < normalized_path.size()) {
            volume_name = normalized_path.substr(pos + 1);
        }
    }
    if (volume_name.empty()) {
        volume_name = normalized_path;
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

    const char* base_path = SDL_GetBasePath();
    path = (base_path != nullptr) ? base_path : "./";

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

} // extern "C"
