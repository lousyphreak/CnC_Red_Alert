#include "win32_compat.h"
#include "mmsystem.h"

#include <SDL3/SDL_loadso.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

enum class HandleKind {
    None,
    File,
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
    explicit FileHandle(SDL_IOStream* io_stream) : HandleBase(HandleKind::File), io(io_stream) {}
    SDL_IOStream* io;
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

struct SearchHandle final : HandleBase {
    SearchHandle() : HandleBase(HandleKind::Search) {}
    std::vector<std::filesystem::directory_entry> matches;
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
std::mutex g_message_queue_mutex;
std::deque<MSG> g_message_queue;
std::mutex g_registry_mutex;
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> g_registry_values;
std::mutex g_named_event_mutex;
std::unordered_map<std::string, EventHandle*> g_named_events;
std::atomic<DWORD> g_next_thread_id{1};
std::chrono::steady_clock::time_point g_start_time = std::chrono::steady_clock::now();

struct TimerHandle {
    std::thread worker;
    std::atomic<bool> active{true};
};

std::mutex g_timer_mutex;
std::unordered_map<UINT, std::unique_ptr<TimerHandle>> g_timers;
std::atomic<UINT> g_next_timer_id{1};

void set_last_error(DWORD value)
{
    std::scoped_lock lock(g_last_error_mutex);
    g_last_error = value;
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

BOOL fill_find_data(const std::filesystem::directory_entry& entry, WIN32_FIND_DATA* data)
{
    if (!data) {
        return FALSE;
    }
    ZeroMemory(data, sizeof(*data));
    std::string name = entry.path().filename().string();
    std::snprintf(data->cFileName, sizeof(data->cFileName), "%s", name.c_str());
    if (entry.is_directory()) {
        data->dwFileAttributes = 0x10;
    }
    if (entry.is_regular_file()) {
        auto size = entry.file_size();
        data->nFileSizeLow = static_cast<DWORD>(size & 0xffffffffu);
        data->nFileSizeHigh = static_cast<DWORD>((size >> 32) & 0xffffffffu);
    }
    return TRUE;
}

BOOL next_message(MSG* message, bool remove, bool wait)
{
    for (;;) {
        {
            std::scoped_lock lock(g_message_queue_mutex);
            if (!g_message_queue.empty()) {
                if (message) {
                    *message = g_message_queue.front();
                }
                if (remove) {
                    g_message_queue.pop_front();
                }
                return TRUE;
            }
        }

        SDL_Event event;
        const bool got_event = wait ? SDL_WaitEvent(&event) : SDL_PollEvent(&event);
        if (!got_event) {
            return FALSE;
        }

        MSG translated{};
        translated.time = static_cast<DWORD>(SDL_GetTicks());
        switch (event.type) {
            case SDL_EVENT_QUIT:
                translated.message = WM_QUIT;
                break;
            case SDL_EVENT_KEY_DOWN:
                translated.message = WM_KEYDOWN;
                translated.wParam = static_cast<WPARAM>(event.key.key);
                break;
            case SDL_EVENT_KEY_UP:
                translated.message = WM_KEYUP;
                translated.wParam = static_cast<WPARAM>(event.key.key);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                translated.message = WM_MOUSEMOVE;
                translated.pt.x = static_cast<LONG>(event.motion.x);
                translated.pt.y = static_cast<LONG>(event.motion.y);
                translated.lParam = MAKELONG(static_cast<int>(event.motion.x), static_cast<int>(event.motion.y));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                translated.message = event.button.button == SDL_BUTTON_LEFT ? WM_LBUTTONDOWN : WM_RBUTTONDOWN;
                translated.pt.x = static_cast<LONG>(event.button.x);
                translated.pt.y = static_cast<LONG>(event.button.y);
                translated.lParam = MAKELONG(static_cast<int>(event.button.x), static_cast<int>(event.button.y));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                translated.message = event.button.button == SDL_BUTTON_LEFT ? WM_LBUTTONUP : WM_RBUTTONUP;
                translated.pt.x = static_cast<LONG>(event.button.x);
                translated.pt.y = static_cast<LONG>(event.button.y);
                translated.lParam = MAKELONG(static_cast<int>(event.button.x), static_cast<int>(event.button.y));
                break;
            default:
                continue;
        }

        std::scoped_lock lock(g_message_queue_mutex);
        g_message_queue.push_back(translated);
    }
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
    return window;
}

BOOL DestroyWindow(HWND window)
{
    if (!window) return FALSE;
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

BOOL UpdateWindow(HWND)
{
    return TRUE;
}

LRESULT DefWindowProc(HWND, UINT message, WPARAM, LPARAM)
{
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

int MessageBox(HWND, LPCSTR text, LPCSTR caption, UINT)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption ? caption : "Red Alert", text ? text : "", nullptr);
    return 0;
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

void SetLastError(DWORD error_code)
{
    set_last_error(error_code);
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
    const SDL_Keymod mod_state = SDL_GetModState();

    switch (virtual_key) {
    case VK_SHIFT:
        return (mod_state & SDL_KMOD_SHIFT) ? static_cast<SHORT>(0x8000) : static_cast<SHORT>(0);
    case VK_CONTROL:
        return (mod_state & SDL_KMOD_CTRL) ? static_cast<SHORT>(0x8000) : static_cast<SHORT>(0);
    case VK_MENU:
        return (mod_state & SDL_KMOD_ALT) ? static_cast<SHORT>(0x8000) : static_cast<SHORT>(0);
    case VK_CAPITAL:
        return (mod_state & SDL_KMOD_CAPS) ? static_cast<SHORT>(0x0001) : static_cast<SHORT>(0);
    case VK_NUMLOCK:
        return (mod_state & SDL_KMOD_NUM) ? static_cast<SHORT>(0x0001) : static_cast<SHORT>(0);
    default:
        return 0;
    }
}

SHORT GetAsyncKeyState(int virtual_key)
{
    return GetKeyState(virtual_key);
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

HANDLE SetCursor(HANDLE cursor)
{
    return cursor;
}

MMRESULT timeBeginPeriod(UINT)
{
    return 0;
}

MMRESULT timeEndPeriod(UINT)
{
    return 0;
}

BOOL ClipCursor(const RECT*)
{
    return TRUE;
}

BOOL GetCursorPos(POINT* point)
{
    if (!point) {
        return FALSE;
    }

    float x = 0.0f;
    float y = 0.0f;
    SDL_GetGlobalMouseState(&x, &y);
    point->x = static_cast<LONG>(x);
    point->y = static_cast<LONG>(y);
    return TRUE;
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

BOOL CloseHandle(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return TRUE;
    auto* base = static_cast<HandleBase*>(handle);
    if (base->kind == HandleKind::File) {
        auto* file = static_cast<FileHandle*>(base);
        if (file->io) {
            SDL_CloseIO(file->io);
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
    SDL_IOStream* io = SDL_IOFromFile(file_name, mode.c_str());
    if (!io) {
        set_last_error(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    return new FileHandle(io);
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

BOOL DeleteFile(LPCSTR file_name)
{
    std::error_code ec;
    return std::filesystem::remove(file_name ? file_name : "", ec) ? TRUE : FALSE;
}

UINT GetDriveType(LPCSTR root_path_name)
{
    if (!root_path_name || !*root_path_name) {
        return DRIVE_NO_ROOT_DIR;
    }

    std::error_code ec;
    const std::filesystem::path path(root_path_name);
    if (!std::filesystem::exists(path, ec)) {
        return DRIVE_NO_ROOT_DIR;
    }

    const std::string path_string = path.string();
    if (path_string.find("cdrom") != std::string::npos || path_string.find("CDROM") != std::string::npos) {
        return DRIVE_CDROM;
    }

    return DRIVE_FIXED;
}

HANDLE FindFirstFile(LPCSTR file_name, LPWIN32_FIND_DATA find_file_data)
{
    std::filesystem::path pattern = file_name ? file_name : "";
    auto directory = pattern.parent_path();
    if (directory.empty()) {
        directory = ".";
    }
    auto regex = std::regex(wildcard_to_regex(pattern.filename().string()), std::regex::icase);
    auto* handle = new SearchHandle();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (std::regex_match(entry.path().filename().string(), regex)) {
            handle->matches.push_back(entry);
        }
    }
    if (handle->matches.empty()) {
        delete handle;
        return INVALID_HANDLE_VALUE;
    }
    fill_find_data(handle->matches[0], find_file_data);
    handle->index = 1;
    return handle;
}

BOOL FindNextFile(HANDLE find_file, LPWIN32_FIND_DATA find_file_data)
{
    if (!find_file || find_file == INVALID_HANDLE_VALUE) return FALSE;
    auto* handle = static_cast<SearchHandle*>(find_file);
    if (handle->index >= handle->matches.size()) {
        return FALSE;
    }
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
    const char* path = SDL_GetBasePath();
    if (!path) path = "./";
    std::snprintf(file_name, size, "%s", path);
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

LONG RegQueryValueEx(HKEY key, LPCSTR value_name, DWORD*, DWORD*, LPBYTE data, DWORD* data_size)
{
    if (!key || !data_size) return ERROR_INVALID_HANDLE;
    const auto& path = *static_cast<std::string*>(key);
    std::string value;
    if (std::strcmp(value_name ? value_name : "", "InstallPath") == 0) {
        value = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    } else if (std::strcmp(value_name ? value_name : "", "DVD") == 0) {
        value = "0";
    } else {
        return ERROR_FILE_NOT_FOUND;
    }
    if (!data || *data_size < value.size() + 1) {
        *data_size = static_cast<DWORD>(value.size() + 1);
        return ERROR_SUCCESS;
    }
    std::memcpy(data, value.c_str(), value.size() + 1);
    *data_size = static_cast<DWORD>(value.size() + 1);
    return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key)
{
    delete static_cast<std::string*>(key);
    return ERROR_SUCCESS;
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
