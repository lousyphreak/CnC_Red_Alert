#ifndef RA_WIN32_COMPAT_H
#define RA_WIN32_COMPAT_H

#pragma pack(push, 8)
#include <SDL3/SDL.h>
#pragma pack(pop)

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

#ifndef stricmp
#define stricmp SDL_strcasecmp
#endif
#ifndef _stricmp
#define _stricmp SDL_strcasecmp
#endif
#ifndef strnicmp
#define strnicmp SDL_strncasecmp
#endif
#ifndef strcmpi
#define strcmpi SDL_strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp SDL_strncasecmp
#endif
#ifndef memicmp
#define memicmp SDL_strncasecmp
#endif
#ifndef strrev
#define strrev SDL_strrev
#endif
#ifndef strupr
#define strupr SDL_strupr
#endif
inline char* _strlwr(char* string)
{
    if (!string) {
        return nullptr;
    }
    for (char* cursor = string; *cursor; ++cursor) {
        *cursor = static_cast<char>(SDL_tolower(static_cast<unsigned char>(*cursor)));
    }
    return string;
}
#ifndef strlwr
#define strlwr _strlwr
#endif

#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef PASCAL
#define PASCAL
#endif
#ifndef FAR
#define FAR
#endif
#ifndef far
#define far
#endif
#ifndef near
#define near
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall WINAPI
#endif
#ifndef cdecl
#define cdecl __cdecl
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef _export
#define _export
#endif

using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using LONG = int32_t;
using ULONG = uint32_t;
using UINT = uint32_t;
using INT = int32_t;
using BOOL = int32_t;
using HRESULT = LONG;
using LPARAM = intptr_t;
using WPARAM = uintptr_t;
using LRESULT = intptr_t;
using MMRESULT = UINT;
using MCIDEVICEID = UINT;
using ATOM = WORD;
using COLORREF = DWORD;
using ULONG_PTR = uintptr_t;
using HANDLE = void*;
using HGLOBAL = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using FARPROC = void (*)();
using HDC = void*;
using HPALETTE = void*;
using HGDIOBJ = void*;
using HKEY = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using PBYTE = BYTE*;
using LPBYTE = BYTE*;
using LPCBYTE = const BYTE*;
using LPSTR = char*;
using LPCSTR = const char*;
using LPDWORD = DWORD*;
using LPWORD = WORD*;
using LPLONG = LONG*;
using LPBOOL = BOOL*;
using CHAR = char;
using UCHAR = unsigned char;
using TCHAR = char;
using LPTSTR = char*;
using LPCTSTR = const char*;
using SHORT = int16_t;
using USHORT = uint16_t;
using SOCKET = int;
using VOID = void;
using INT_PTR = intptr_t;

inline LPSTR lstrcpy(LPSTR destination, LPCSTR source)
{
    return std::strcpy(destination, source);
}

inline LPSTR lstrcat(LPSTR destination, LPCSTR source)
{
    return std::strcat(destination, source);
}

inline int lstrcmp(LPCSTR string1, LPCSTR string2)
{
    return std::strcmp(string1, string2);
}

inline int lstrcmpi(LPCSTR string1, LPCSTR string2)
{
    return SDL_strcasecmp(string1, string2);
}

inline int lstrlen(LPCSTR string)
{
    return string ? static_cast<int>(std::strlen(string)) : 0;
}

inline LPSTR lstrcpyn(LPSTR destination, LPCSTR source, int max_length)
{
    if (!destination || max_length <= 0) {
        return destination;
    }

    if (!source) {
        destination[0] = '\0';
        return destination;
    }

    std::strncpy(destination, source, static_cast<size_t>(max_length) - 1);
    destination[max_length - 1] = '\0';
    return destination;
}

struct MEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORD dwTotalPhys;
    DWORD dwAvailPhys;
    DWORD dwTotalPageFile;
    DWORD dwAvailPageFile;
    DWORD dwTotalVirtual;
    DWORD dwAvailVirtual;
};

struct SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
};

struct RAWindow;
using HWND = RAWindow*;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#define MAX_PATH 260
#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define MB_OK 0x00000000U
#define MB_ICONSTOP 0x00000010U
#define MB_ICONQUESTION 0x00000020U
#define MB_ICONEXCLAMATION 0x00000030U
#define MB_YESNO 0x00000004U
#define IDOK 1
#define IDYES 6
#define IDNO 7

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA 8
#define SW_RESTORE 9

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002

#define WS_POPUP 0x80000000U
#define WS_EX_TOPMOST 0x00000008U

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_ACTIVATEAPP 0x001C
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_SHOWWINDOW 0x0018
#define WM_SETCURSOR 0x0020
#define WM_SYSCOMMAND 0x0112
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_INITDIALOG 0x0110
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_USER 0x0400

#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010

#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_CAPITAL 0x14
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_NUMLOCK 0x90

#define WAIT_OBJECT_0 0x00000000L
#define WAIT_TIMEOUT 0x00000102L
#define WAIT_FAILED 0xFFFFFFFFU
#define INFINITE 0xFFFFFFFFU

#define SC_CLOSE 0xF060
#define SC_SCREENSAVE 0xF140

#define GENERIC_READ 0x80000000U
#define GENERIC_WRITE 0x40000000U
#define FILE_SHARE_READ 0x00000001U
#define FILE_SHARE_WRITE 0x00000002U
#define CREATE_NEW 1U
#define CREATE_ALWAYS 2U
#define OPEN_EXISTING 3U
#define OPEN_ALWAYS 4U
#define TRUNCATE_EXISTING 5U
#define FILE_ATTRIBUTE_HIDDEN 0x00000002U
#define FILE_ATTRIBUTE_SYSTEM 0x00000004U
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010U
#define FILE_ATTRIBUTE_NORMAL 0x00000080U
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100U
#define FILE_FLAG_OVERLAPPED 0x40000000U
#define FILE_BEGIN 0U
#define FILE_CURRENT 1U
#define FILE_END 2U

#define DRIVE_UNKNOWN 0U
#define DRIVE_NO_ROOT_DIR 1U
#define DRIVE_REMOVABLE 2U
#define DRIVE_FIXED 3U
#define DRIVE_REMOTE 4U
#define DRIVE_CDROM 5U
#define DRIVE_RAMDISK 6U

#define DUPLICATE_SAME_ACCESS 0x00000002U
#define THREAD_ALL_ACCESS 0x001F03FFU
#define THREAD_PRIORITY_TIME_CRITICAL 15

#define GHND 0x0042
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040

#define PAGE_READWRITE 0x04
#define FILE_MAP_WRITE 0x0002
#define EVENT_MODIFY_STATE 0x0002

#define SETDTR 5U
#define CLRDTR 6U
#define SETRTS 3U
#define CLRRTS 4U
#define SETBREAK 8U
#define CLRBREAK 9U

#define DTR_CONTROL_DISABLE 0U
#define DTR_CONTROL_ENABLE 1U
#define RTS_CONTROL_DISABLE 0U
#define RTS_CONTROL_ENABLE 1U
#define RTS_CONTROL_HANDSHAKE 2U

#define PURGE_TXABORT 0x0001U
#define PURGE_RXABORT 0x0002U
#define PURGE_TXCLEAR 0x0004U
#define PURGE_RXCLEAR 0x0008U

#define CE_RXOVER 0x0001U
#define CE_OVERRUN 0x0002U
#define CE_RXPARITY 0x0004U
#define CE_FRAME 0x0008U
#define CE_IOE 0x4000U
#define CE_TXFULL 0x0100U

#define MS_CTS_ON 0x0010U
#define MS_DSR_ON 0x0020U
#define MS_RING_ON 0x0040U
#define MS_RLSD_ON 0x0080U

#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_IO_PENDING 997L
#define ERROR_IO_INCOMPLETE 996L
#define STILL_ACTIVE 259L

#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000

#define KEY_READ 0x20019
#define REG_SZ 1
#define REG_DWORD 4
#define HKEY_CLASSES_ROOT ((HKEY)(intptr_t)0x80000000)
#define HKEY_LOCAL_MACHINE ((HKEY)(intptr_t)0x80000002)

#define WSAEWOULDBLOCK 10035
#define FD_WRITE 0x0002

#define MAKEWORD(a, b) ((WORD)(((BYTE)((a) & 0xff)) | ((WORD)((BYTE)((b) & 0xff))) << 8))
#define MAKEINTRESOURCE(i) ((LPCTSTR)(uintptr_t)((WORD)(i)))
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((w) & 0xff))
#define HIBYTE(w) ((BYTE)(((w) >> 8) & 0xff))
#define MAKELONG(a, b) ((LONG)(((WORD)((a) & 0xffff)) | ((DWORD)((WORD)((b) & 0xffff))) << 16))

using DWORD_PTR = uintptr_t;

struct POINT {
    LONG x;
    LONG y;
};

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

struct PAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
};

struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
};
using MSG = tagMSG;

struct alignas(void*) CRITICAL_SECTION {
    std::recursive_mutex* mutex;
};

using WNDPROC = LRESULT (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
using DLGPROC = INT_PTR (CALLBACK*)(HWND, UINT, WPARAM, LPARAM);

struct WNDCLASS {
    UINT style;
    WNDPROC lpfnWndProc;
    INT cbClsExtra;
    INT cbWndExtra;
    HINSTANCE hInstance;
    HGDIOBJ hIcon;
    HGDIOBJ hCursor;
    HGDIOBJ hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
};

struct OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        LPVOID Pointer;
    };
    HANDLE hEvent;
};
using LPOVERLAPPED = OVERLAPPED*;

struct DCB {
    DWORD DCBlength;
    DWORD BaudRate;
    DWORD fBinary : 1;
    DWORD fParity : 1;
    DWORD fOutxCtsFlow : 1;
    DWORD fOutxDsrFlow : 1;
    DWORD fDtrControl : 2;
    DWORD fDsrSensitivity : 1;
    DWORD fTXContinueOnXoff : 1;
    DWORD fOutX : 1;
    DWORD fInX : 1;
    DWORD fErrorChar : 1;
    DWORD fNull : 1;
    DWORD fRtsControl : 2;
    DWORD fAbortOnError : 1;
    DWORD fDummy2 : 17;
    WORD wReserved;
    WORD XonLim;
    WORD XoffLim;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    CHAR XonChar;
    CHAR XoffChar;
    CHAR ErrorChar;
    CHAR EofChar;
    CHAR EvtChar;
    WORD wReserved1;
};

struct COMMTIMEOUTS {
    DWORD ReadIntervalTimeout;
    DWORD ReadTotalTimeoutMultiplier;
    DWORD ReadTotalTimeoutConstant;
    DWORD WriteTotalTimeoutMultiplier;
    DWORD WriteTotalTimeoutConstant;
};

struct COMSTAT {
    DWORD fCtsHold : 1;
    DWORD fDsrHold : 1;
    DWORD fRlsdHold : 1;
    DWORD fXoffHold : 1;
    DWORD fXoffSent : 1;
    DWORD fEof : 1;
    DWORD fTxim : 1;
    DWORD fReserved : 25;
    DWORD cbInQue;
    DWORD cbOutQue;
};

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

struct WIN32_FIND_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
};
using LPWIN32_FIND_DATA = WIN32_FIND_DATA*;

struct BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
};

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
};
#pragma pack(pop)

struct RGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
};

struct BITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};

struct BITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
};
using LPBITMAPINFO = BITMAPINFO*;
using LPBITMAPINFOHEADER = BITMAPINFOHEADER*;

struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};

struct LOGPALETTE {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[1];
};
using LPLOGPALETTE = LOGPALETTE*;

struct STARTUPINFO {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};

struct RAWindow {
    SDL_Window* sdl_window;
    std::string class_name;
    std::string title;
    WNDPROC wnd_proc;
    int width;
    int height;
};

extern "C" {

ATOM RegisterClass(const WNDCLASS* wndclass);
BOOL UnregisterClass(LPCSTR class_name, HINSTANCE instance);
HWND CreateWindowEx(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style,
    INT x, INT y, INT width, INT height, HWND parent, HANDLE menu, HINSTANCE instance, LPVOID param);
HWND FindWindow(LPCSTR class_name, LPCSTR window_name);
BOOL IsWindow(HWND window);
BOOL DestroyWindow(HWND window);
BOOL ShowWindow(HWND window, INT command);
BOOL SetForegroundWindow(HWND window);
BOOL UpdateWindow(HWND window);
LRESULT DefWindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
BOOL PeekMessage(MSG* message, HWND window, UINT min_filter, UINT max_filter, UINT remove_message);
BOOL GetMessage(MSG* message, HWND window, UINT min_filter, UINT max_filter);
BOOL TranslateMessage(const MSG* message);
LRESULT DispatchMessage(const MSG* message);
void PostQuitMessage(INT exit_code);
BOOL PostMessage(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
LRESULT SendMessage(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
UINT RegisterWindowMessage(LPCSTR string);
int GetSystemMetrics(int index);
HWND SetFocus(HWND window);
HGDIOBJ LoadIcon(HINSTANCE instance, LPCSTR icon_name);
HGDIOBJ LoadCursor(HINSTANCE instance, LPCSTR cursor_name);
INT_PTR DialogBox(HINSTANCE instance, LPCTSTR template_name, HWND owner, DLGPROC dialog_proc);
void ExitProcess(UINT exit_code);

int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type);
void OutputDebugString(LPCSTR text);
DWORD GetLastError(void);
DWORD GetVersion(void);
void SetLastError(DWORD error_code);
UINT SetErrorMode(UINT mode);
void Sleep(DWORD milliseconds);
DWORD GetTickCount(void);
void GetSystemTime(SYSTEMTIME* system_time);
void GetLocalTime(SYSTEMTIME* system_time);
void GlobalMemoryStatus(MEMORYSTATUS* memory_status);
SHORT GetKeyState(int virtual_key);
SHORT GetAsyncKeyState(int virtual_key);
SHORT VkKeyScan(unsigned char ch);
UINT MapVirtualKey(UINT code, UINT map_type);
int ToAscii(UINT virtual_key_code, UINT scan_code, PBYTE key_state, LPWORD translated_char, UINT flags);
HANDLE SetCursor(HANDLE cursor);
int ShowCursor(BOOL show);
MMRESULT timeBeginPeriod(UINT period);
MMRESULT timeEndPeriod(UINT period);
BOOL ClipCursor(const RECT* rect);
BOOL GetCursorPos(POINT* point);
DWORD GetCurrentThreadId(void);
HANDLE GetCurrentThread(void);
HANDLE GetCurrentProcess(void);
BOOL DuplicateHandle(HANDLE source_process_handle, HANDLE source_handle, HANDLE target_process_handle,
    HANDLE* target_handle, DWORD desired_access, BOOL inherit_handle, DWORD options);
BOOL SetThreadPriority(HANDLE thread, int priority);
HANDLE CreateThread(LPVOID thread_attributes, size_t stack_size, DWORD (WINAPI *start_address)(LPVOID),
    LPVOID parameter, DWORD creation_flags, DWORD* thread_id);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
DWORD WaitForInputIdle(HANDLE handle, DWORD milliseconds);
BOOL GetExitCodeProcess(HANDLE handle, LPDWORD exit_code);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateMutex(LPVOID attributes, BOOL initial_owner, LPCSTR name);
BOOL ReleaseMutex(HANDLE handle);
HANDLE CreateEvent(LPVOID attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name);
HANDLE OpenEvent(DWORD desired_access, BOOL inherit_handle, LPCSTR name);
BOOL SetEvent(HANDLE handle);
BOOL ResetEvent(HANDLE handle);
BOOL CreateProcess(LPCSTR application_name, LPSTR command_line, LPVOID process_attributes,
    LPVOID thread_attributes, BOOL inherit_handles, DWORD creation_flags, LPVOID environment,
    LPCSTR current_directory, STARTUPINFO* startup_info, PROCESS_INFORMATION* process_information);
void InitializeCriticalSection(CRITICAL_SECTION* critical_section);
void DeleteCriticalSection(CRITICAL_SECTION* critical_section);
void EnterCriticalSection(CRITICAL_SECTION* critical_section);
void LeaveCriticalSection(CRITICAL_SECTION* critical_section);

HANDLE CreateFile(LPCSTR file_name, DWORD desired_access, DWORD share_mode, LPVOID security_attributes,
    DWORD creation_disposition, DWORD flags_and_attributes, HANDLE template_file);
BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD number_of_bytes_to_read, LPDWORD number_of_bytes_read, LPOVERLAPPED overlapped);
BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD number_of_bytes_to_write, LPDWORD number_of_bytes_written, LPOVERLAPPED overlapped);
BOOL GetCommState(HANDLE handle, DCB* dcb);
BOOL SetCommState(HANDLE handle, const DCB* dcb);
BOOL SetCommTimeouts(HANDLE handle, const COMMTIMEOUTS* timeouts);
BOOL SetupComm(HANDLE handle, DWORD in_queue, DWORD out_queue);
BOOL PurgeComm(HANDLE handle, DWORD flags);
BOOL EscapeCommFunction(HANDLE handle, DWORD function);
BOOL SetCommBreak(HANDLE handle);
BOOL ClearCommBreak(HANDLE handle);
BOOL GetOverlappedResult(HANDLE handle, LPOVERLAPPED overlapped, LPDWORD number_of_bytes_transferred, BOOL wait);
BOOL ClearCommError(HANDLE handle, LPDWORD errors, COMSTAT* status);
BOOL GetCommModemStatus(HANDLE handle, LPDWORD modem_status);
DWORD SetFilePointer(HANDLE handle, LONG distance_to_move, LONG* distance_to_move_high, DWORD move_method);
DWORD GetFileSize(HANDLE handle, DWORD* file_size_high);
BOOL GetFileInformationByHandle(HANDLE handle, BY_HANDLE_FILE_INFORMATION* file_information);
BOOL GetFileTime(HANDLE handle, FILETIME* creation_time, FILETIME* last_access_time, FILETIME* last_write_time);
BOOL FileTimeToDosDateTime(const FILETIME* file_time, LPWORD dos_date, LPWORD dos_time);
BOOL DosDateTimeToFileTime(WORD dos_date, WORD dos_time, FILETIME* file_time);
BOOL SetFileTime(HANDLE handle, const FILETIME* creation_time, const FILETIME* last_access_time, const FILETIME* last_write_time);
BOOL DeleteFile(LPCSTR file_name);
UINT GetDriveType(LPCSTR root_path_name);
BOOL GetVolumeInformation(LPCSTR root_path_name, LPSTR volume_name_buffer, DWORD volume_name_size, DWORD* volume_serial_number,
    DWORD* maximum_component_length, DWORD* file_system_flags, LPSTR file_system_name_buffer, DWORD file_system_name_size);

HANDLE FindFirstFile(LPCSTR file_name, LPWIN32_FIND_DATA find_file_data);
BOOL FindNextFile(HANDLE find_file, LPWIN32_FIND_DATA find_file_data);
BOOL FindClose(HANDLE find_file);

HGLOBAL GlobalAlloc(UINT flags, size_t bytes);
LPVOID GlobalLock(HGLOBAL memory);
BOOL GlobalUnlock(HGLOBAL memory);
HGLOBAL GlobalFree(HGLOBAL memory);

HMODULE LoadLibrary(LPCSTR file_name);
FARPROC GetProcAddress(HMODULE module, LPCSTR proc_name);
BOOL FreeLibrary(HMODULE module);
DWORD GetModuleFileName(HINSTANCE instance, LPSTR file_name, DWORD size);

HANDLE CreateFileMapping(HANDLE file, LPVOID attributes, DWORD protect, DWORD maximum_size_high,
    DWORD maximum_size_low, LPCSTR name);
LPVOID MapViewOfFile(HANDLE file_mapping_object, DWORD desired_access, DWORD file_offset_high,
    DWORD file_offset_low, size_t number_of_bytes_to_map);
BOOL UnmapViewOfFile(LPCVOID base_address);

LONG RegOpenKeyEx(HKEY key, LPCSTR sub_key, DWORD options, DWORD sam_desired, HKEY* result);
LONG RegQueryInfoKey(HKEY key, LPSTR class_name, LPDWORD class_size, LPDWORD reserved, LPDWORD sub_keys,
    LPDWORD max_sub_key_len, LPDWORD max_class_len, LPDWORD values, LPDWORD max_value_name_len,
    LPDWORD max_value_len, LPDWORD security_descriptor, FILETIME* last_write_time);
LONG RegEnumKeyEx(HKEY key, DWORD index, LPSTR name, DWORD* name_size, DWORD* reserved,
    LPSTR class_name, DWORD* class_size, FILETIME* last_write_time);
LONG RegQueryValue(HKEY key, LPCSTR sub_key, LPSTR data, LPLONG data_size);
LONG RegQueryValueEx(HKEY key, LPCSTR value_name, DWORD* reserved, DWORD* type, LPBYTE data, DWORD* data_size);
LONG RegCloseKey(HKEY key);

HDC BeginPaint(HWND window, PAINTSTRUCT* paint);
BOOL EndPaint(HWND window, const PAINTSTRUCT* paint);
HDC GetDC(HWND window);
INT ReleaseDC(HWND window, HDC dc);
HPALETTE CreatePalette(const LOGPALETTE* palette);
HPALETTE SelectPalette(HDC dc, HPALETTE palette, BOOL force_background);
UINT RealizePalette(HDC dc);
BOOL DeleteObject(HGDIOBJ object);
int StretchDIBits(HDC dc, int x_dest, int y_dest, int dest_width, int dest_height,
    int x_src, int y_src, int src_width, int src_height, const VOID* bits, const BITMAPINFO* bits_info,
    UINT usage, DWORD rop);
int SetDIBitsToDevice(HDC dc, int x_dest, int y_dest, DWORD width, DWORD height,
    int x_src, int y_src, UINT start_scan, UINT scan_lines, const VOID* bits, const BITMAPINFO* bits_info, UINT usage);

}

inline void ZeroMemory(void* memory, size_t size)
{
    std::memset(memory, 0, size);
}

inline void CopyMemory(void* dest, const void* src, size_t size)
{
    std::memcpy(dest, src, size);
}

inline char* ra_integer_to_string(long long value, char* str, int radix)
{
    if (!str || radix < 2 || radix > 36) {
        return str;
    }

    const bool negative = (radix == 10) && (value < 0);
    unsigned long long current = negative ? static_cast<unsigned long long>(-value) : static_cast<unsigned long long>(value);
    char* out = str;

    do {
        const unsigned digit_value = static_cast<unsigned>(current % static_cast<unsigned long long>(radix));
        *out++ = static_cast<char>(digit_value < 10 ? ('0' + digit_value) : ('a' + (digit_value - 10)));
        current /= static_cast<unsigned long long>(radix);
    } while (current != 0);

    if (negative) {
        *out++ = '-';
    }

    *out = '\0';
    for (char* left = str, *right = out - 1; left < right; ++left, --right) {
        const char temp = *left;
        *left = *right;
        *right = temp;
    }
    return str;
}

inline char* itoa(int value, char* str, int radix)
{
    return ra_integer_to_string(value, str, radix);
}

inline char* ltoa(long value, char* str, int radix)
{
    return ra_integer_to_string(value, str, radix);
}

inline unsigned long _rotl(unsigned long value, int shift)
{
    shift &= 31;
    return (value << shift) | (value >> (32 - shift));
}

inline unsigned long _lrotl(unsigned long value, int shift)
{
    return _rotl(value, shift);
}

#endif
