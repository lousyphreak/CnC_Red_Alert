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
using COLORREF = DWORD;
using ULONG_PTR = uintptr_t;
using HANDLE = void*;
using HGLOBAL = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using FARPROC = void (*)();
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

#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_0 0x30
#define VK_1 0x31
#define VK_2 0x32
#define VK_3 0x33
#define VK_4 0x34
#define VK_5 0x35
#define VK_6 0x36
#define VK_7 0x37
#define VK_8 0x38
#define VK_9 0x39
#define VK_A 0x41
#define VK_B 0x42
#define VK_C 0x43
#define VK_D 0x44
#define VK_E 0x45
#define VK_F 0x46
#define VK_G 0x47
#define VK_H 0x48
#define VK_I 0x49
#define VK_J 0x4A
#define VK_K 0x4B
#define VK_L 0x4C
#define VK_M 0x4D
#define VK_N 0x4E
#define VK_O 0x4F
#define VK_P 0x50
#define VK_Q 0x51
#define VK_R 0x52
#define VK_S 0x53
#define VK_T 0x54
#define VK_U 0x55
#define VK_V 0x56
#define VK_W 0x57
#define VK_X 0x58
#define VK_Y 0x59
#define VK_Z 0x5A
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE

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
    std::string class_name;
    std::string title;
    WNDPROC wnd_proc;
    int width;
    int height;
};

bool RA_GetPresentationRect(HWND window, SDL_FRect* rect);
bool RA_WindowToGamePoint(HWND window, float window_x, float window_y, int* game_x, int* game_y);
bool RA_GameRectToWindowRect(HWND window, const RECT* game_rect, SDL_Rect* window_rect);

extern "C" {

ATOM RegisterClass(const WNDCLASS* wndclass);
HWND CreateWindowEx(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style,
    INT x, INT y, INT width, INT height, HWND parent, HANDLE menu, HINSTANCE instance, LPVOID param);
HWND FindWindow(LPCSTR class_name, LPCSTR window_name);
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
INT_PTR DialogBox(HINSTANCE instance, LPCTSTR template_name, HWND owner, DLGPROC dialog_proc);
void ExitProcess(UINT exit_code);

int MessageBox(HWND window, LPCSTR text, LPCSTR caption, UINT type);
void OutputDebugString(LPCSTR text);
DWORD GetLastError(void);
DWORD GetVersion(void);
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
BOOL ClipCursor(const RECT* rect);
BOOL GetCursorPos(POINT* point);
DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL CloseHandle(HANDLE handle);
HANDLE CreateEvent(LPVOID attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name);
HANDLE OpenEvent(DWORD desired_access, BOOL inherit_handle, LPCSTR name);
BOOL SetEvent(HANDLE handle);
BOOL ResetEvent(HANDLE handle);
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
DWORD GetModuleFileName(HINSTANCE instance, LPSTR file_name, DWORD size);

HANDLE CreateFileMapping(HANDLE file, LPVOID attributes, DWORD protect, DWORD maximum_size_high,
    DWORD maximum_size_low, LPCSTR name);
LPVOID MapViewOfFile(HANDLE file_mapping_object, DWORD desired_access, DWORD file_offset_high,
    DWORD file_offset_low, size_t number_of_bytes_to_map);

LONG RegOpenKeyEx(HKEY key, LPCSTR sub_key, DWORD options, DWORD sam_desired, HKEY* result);
LONG RegQueryInfoKey(HKEY key, LPSTR class_name, LPDWORD class_size, LPDWORD reserved, LPDWORD sub_keys,
    LPDWORD max_sub_key_len, LPDWORD max_class_len, LPDWORD values, LPDWORD max_value_name_len,
    LPDWORD max_value_len, LPDWORD security_descriptor, FILETIME* last_write_time);
LONG RegEnumKeyEx(HKEY key, DWORD index, LPSTR name, DWORD* name_size, DWORD* reserved,
    LPSTR class_name, DWORD* class_size, FILETIME* last_write_time);
LONG RegQueryValueEx(HKEY key, LPCSTR value_name, DWORD* reserved, DWORD* type, LPBYTE data, DWORD* data_size);
LONG RegCloseKey(HKEY key);

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
