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

#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
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
using MCIDEVICEID = UINT;
using ATOM = WORD;
using HANDLE = void*;
using HGLOBAL = void*;
using HMODULE = void*;
using FARPROC = void (*)();
using HGDIOBJ = void*;
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
using UCHAR = uint8_t;
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
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010U
#define FILE_ATTRIBUTE_NORMAL 0x00000080U
#define FILE_BEGIN 0U
#define FILE_CURRENT 1U
#define FILE_END 2U

#define DRIVE_NO_ROOT_DIR 1U
#define DRIVE_REMOVABLE 2U
#define DRIVE_FIXED 3U
#define DRIVE_CDROM 5U

#define GHND 0x0042
#define GMEM_FIXED 0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040

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
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000

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

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
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

using LPBITMAPINFOHEADER = BITMAPINFOHEADER*;

struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};

struct RAWindow {
    SDL_Window* sdl_window;
    std::string title;
    int width;
    int height;
};

RAWindow* RA_CreateWindow(const char* title, int width, int height, SDL_WindowFlags flags);
void RA_DestroyWindow(RAWindow* window);
bool RA_GetPresentationRect(RAWindow* window, SDL_FRect* rect);
bool RA_GetRenderSourceRect(RAWindow* window, SDL_FRect* rect);
bool RA_WindowToGamePoint(RAWindow* window, float window_x, float window_y, int* game_x, int* game_y);
bool RA_GameRectToWindowRect(RAWindow* window, const RECT* game_rect, SDL_Rect* window_rect);

extern "C" {

int GetSystemMetrics(int index);
void ExitProcess(UINT exit_code);

int MessageBox(RAWindow* window, LPCSTR text, LPCSTR caption, UINT type);
void OutputDebugString(LPCSTR text);
DWORD GetLastError(void);
UINT SetErrorMode(UINT mode);
void Sleep(DWORD milliseconds);
DWORD GetTickCount(void);
void GetSystemTime(SYSTEMTIME* system_time);
void GetLocalTime(SYSTEMTIME* system_time);
void GlobalMemoryStatus(MEMORYSTATUS* memory_status);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateEvent(LPVOID attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name);
BOOL SetEvent(HANDLE handle);
BOOL ResetEvent(HANDLE handle);
HANDLE CreateFile(LPCSTR file_name, DWORD desired_access, DWORD share_mode, LPVOID security_attributes,
    DWORD creation_disposition, DWORD flags_and_attributes, HANDLE template_file);
BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD number_of_bytes_to_read, LPDWORD number_of_bytes_read, LPVOID overlapped);
BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD number_of_bytes_to_write, LPDWORD number_of_bytes_written, LPVOID overlapped);
DWORD SetFilePointer(HANDLE handle, LONG distance_to_move, LONG* distance_to_move_high, DWORD move_method);
UINT GetDriveType(LPCSTR root_path_name);
BOOL GetVolumeInformation(LPCSTR root_path_name, LPSTR volume_name_buffer, DWORD volume_name_size, DWORD* volume_serial_number,
    DWORD* maximum_component_length, DWORD* file_system_flags, LPSTR file_system_name_buffer, DWORD file_system_name_size);

HGLOBAL GlobalAlloc(UINT flags, size_t bytes);
LPVOID GlobalLock(HGLOBAL memory);
BOOL GlobalUnlock(HGLOBAL memory);
HGLOBAL GlobalFree(HGLOBAL memory);

HMODULE LoadLibrary(LPCSTR file_name);
FARPROC GetProcAddress(HMODULE module, LPCSTR proc_name);
BOOL FreeLibrary(HMODULE module);
DWORD GetModuleFileName(void* instance, LPSTR file_name, DWORD size);

}

inline void ZeroMemory(void* memory, size_t size)
{
    std::memset(memory, 0, size);
}

inline void CopyMemory(void* dest, const void* src, size_t size)
{
    std::memcpy(dest, src, size);
}

inline char* ra_integer_to_string(int64_t value, char* str, int radix)
{
    if (!str || radix < 2 || radix > 36) {
        return str;
    }

    const bool negative = (radix == 10) && (value < 0);
    uint64_t current = negative ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);
    char* out = str;

    do {
        const uint32_t digit_value = static_cast<uint32_t>(current % static_cast<uint64_t>(radix));
        *out++ = static_cast<char>(digit_value < 10 ? ('0' + digit_value) : ('a' + (digit_value - 10)));
        current /= static_cast<uint64_t>(radix);
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

inline char* itoa(int32_t value, char* str, int radix)
{
    return ra_integer_to_string(value, str, radix);
}

inline char* ltoa(int32_t value, char* str, int radix)
{
    return ra_integer_to_string(value, str, radix);
}

inline uint32_t _rotl(uint32_t value, int shift)
{
    shift &= 31;
    if (shift == 0) {
        return value;
    }
    return (value << shift) | (value >> (32 - shift));
}

inline uint32_t _lrotl(uint32_t value, int shift)
{
    return _rotl(value, shift);
}


#endif
