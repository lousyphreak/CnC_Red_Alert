#ifndef RA_WINSOCK_WRAPPER_H
#define RA_WINSOCK_WRAPPER_H

#include "windows.h"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef AF_IPX
#define AF_IPX 4
#endif
#ifndef NSPROTO_IPX
#define NSPROTO_IPX 1000
#endif
#ifndef IPX_PTYPE
#define IPX_PTYPE 0x4001
#endif
#ifndef IPX_FILTERPTYPE
#define IPX_FILTERPTYPE 0x4002
#endif

#define FD_READ 0x0001
#define FD_WRITE 0x0002
#define FD_OOB 0x0004
#define FD_ACCEPT 0x0008
#define FD_CONNECT 0x0010
#define FD_CLOSE 0x0020

struct WSADATA {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
};

struct SOCKADDR_IPX {
    short sa_family;
    char sa_netnum[4];
    char sa_nodenum[6];
    unsigned short sa_socket;
};

using SOCKADDR = struct sockaddr;
using LPSOCKADDR = struct sockaddr*;
using SOCKADDR_IN = struct sockaddr_in;
using LPSOCKADDR_IN = struct sockaddr_in*;
using SOCKADDR_IPX_T = SOCKADDR_IPX;
using LPSOCKADDR_IPX = SOCKADDR_IPX*;
using IN_ADDR = struct in_addr;
using LPIN_ADDR = struct in_addr*;
using LINGER = struct linger;

#ifndef MAXGETHOSTSTRUCT
#define MAXGETHOSTSTRUCT 1024
#endif

#ifndef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#endif

#define WSAGETASYNCERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTEVENT(l_param) LOWORD(l_param)

inline int WSAStartup(WORD version, WSADATA* data)
{
    if (data) {
        ZeroMemory(data, sizeof(*data));
        data->wVersion = version;
        data->wHighVersion = version;
    }
    return 0;
}

inline int WSACleanup(void)
{
    return 0;
}

inline int WSAGetLastError(void)
{
    return errno;
}

inline int closesocket(SOCKET socket_handle)
{
    return close(socket_handle);
}

inline int ioctlsocket(SOCKET socket_handle, long command, u_long* argp)
{
    return ioctl(socket_handle, command, argp);
}

inline int WSAAsyncSelect(SOCKET, HWND, unsigned int, long)
{
    return 0;
}

inline HANDLE WSAAsyncGetHostByName(HWND, unsigned int, const char*, char*, int)
{
    return nullptr;
}

inline HANDLE WSAAsyncGetHostByAddr(HWND, unsigned int, const char*, int, int, char*, int)
{
    return nullptr;
}

inline int WSACancelAsyncRequest(HANDLE)
{
    return 0;
}

#endif
