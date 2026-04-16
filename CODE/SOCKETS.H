/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SOCKETS_H
#define SOCKETS_H

#ifdef _WIN32

#include <cstdint>

using SOCKET = uintptr_t;
using u_char = uint8_t;
using u_short = uint16_t;
using u_int = uint32_t;
using u_long = uint32_t;

struct WSADATA {
	uint16_t wVersion;
	uint16_t wHighVersion;
	char szDescription[257];
	char szSystemStatus[129];
	uint16_t iMaxSockets;
	uint16_t iMaxUdpDg;
	char* lpVendorInfo;
};

struct in_addr {
	u_long s_addr;
};

struct sockaddr {
	u_short sa_family;
	char sa_data[14];
};

struct sockaddr_in {
	short sin_family;
	u_short sin_port;
	struct in_addr sin_addr;
	char sin_zero[8];
};

struct hostent {
	char* h_name;
	char** h_aliases;
	short h_addrtype;
	short h_length;
	char** h_addr_list;
};

struct linger {
	u_short l_onoff;
	u_short l_linger;
};

struct timeval {
	long tv_sec;
	long tv_usec;
};

#ifndef FD_SETSIZE
#define FD_SETSIZE 64
#endif

struct fd_set {
	u_int fd_count;
	SOCKET fd_array[FD_SETSIZE];
};

using SOCKADDR = struct sockaddr;
using LPSOCKADDR = struct sockaddr*;
using SOCKADDR_IN = struct sockaddr_in;
using LPSOCKADDR_IN = struct sockaddr_in*;
using IN_ADDR = struct in_addr;
using LPIN_ADDR = struct in_addr*;
using LINGER = struct linger;

#ifndef h_addr
#define h_addr h_addr_list[0]
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET ((SOCKET)(~(uintptr_t)0))
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef MAXGETHOSTSTRUCT
#define MAXGETHOSTSTRUCT 1024
#endif

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef PF_INET
#define PF_INET AF_INET
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#ifndef INADDR_ANY
#define INADDR_ANY ((u_long)0x00000000)
#endif

#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((u_long)0xffffffff)
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((u_long)0xffffffff)
#endif

#ifndef SOL_SOCKET
#define SOL_SOCKET 0xffff
#endif

#ifndef SO_REUSEADDR
#define SO_REUSEADDR 0x0004
#endif

#ifndef SO_BROADCAST
#define SO_BROADCAST 0x0020
#endif

#ifndef SO_SNDBUF
#define SO_SNDBUF 0x1001
#endif

#ifndef SO_RCVBUF
#define SO_RCVBUF 0x1002
#endif

#ifndef SO_ERROR
#define SO_ERROR 0x1007
#endif

#ifndef SO_LINGER
#define SO_LINGER 0x0080
#endif

#ifndef TCP_NODELAY
#define TCP_NODELAY 0x0001
#endif

#ifndef FIONBIO
#define FIONBIO 0x8004667eu
#endif

#ifndef FD_READ
#define FD_READ 0x0001
#endif

#ifndef FD_WRITE
#define FD_WRITE 0x0002
#endif

#ifndef FD_OOB
#define FD_OOB 0x0004
#endif

#ifndef FD_ACCEPT
#define FD_ACCEPT 0x0008
#endif

#ifndef FD_CONNECT
#define FD_CONNECT 0x0010
#endif

#ifndef FD_CLOSE
#define FD_CLOSE 0x0020
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK 10035
#endif

#ifndef WSAECONNRESET
#define WSAECONNRESET 10054
#endif

#ifndef LOWORD
#define LOWORD(l) ((uint16_t)((uintptr_t)(l) & 0xffff))
#endif

#ifndef HIWORD
#define HIWORD(l) ((uint16_t)(((uintptr_t)(l) >> 16) & 0xffff))
#endif

#ifndef FD_ZERO
#define FD_ZERO(set) ((set)->fd_count = 0)
#endif

#ifndef FD_SET
#define FD_SET(socket_handle, set) \
	do { \
		if ((set)->fd_count < FD_SETSIZE) { \
			(set)->fd_array[(set)->fd_count++] = (socket_handle); \
		} \
	} while (0)
#endif

#ifndef FD_ISSET
#define FD_ISSET(socket_handle, set) RA_FD_ISSET((socket_handle), (set))
#endif

inline int RA_FD_ISSET(SOCKET socket_handle, const fd_set* set)
{
	for (u_int index = 0; index < set->fd_count; ++index) {
		if (set->fd_array[index] == socket_handle) {
			return 1;
		}
	}
	return 0;
}

#define WSAGETASYNCERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTEVENT(l_param) LOWORD(l_param)

#ifdef _M_IX86
#define RA_SOCKET_CALL __stdcall
#else
#define RA_SOCKET_CALL
#endif

extern "C" {
int RA_SOCKET_CALL WSAStartup(uint16_t version, WSADATA* data);
int RA_SOCKET_CALL WSACleanup(void);
int RA_SOCKET_CALL WSAGetLastError(void);
int RA_SOCKET_CALL WSAAsyncSelect(SOCKET socket_handle, void* window_handle, uint32_t message, long events);
void* RA_SOCKET_CALL WSAAsyncGetHostByAddr(void* window_handle, uint32_t message, const char* address, int length, int type, char* buffer, int buffer_length);
void* RA_SOCKET_CALL WSAAsyncGetHostByName(void* window_handle, uint32_t message, const char* name, char* buffer, int buffer_length);
int RA_SOCKET_CALL WSACancelAsyncRequest(void* async_handle);
SOCKET RA_SOCKET_CALL socket(int af, int type, int protocol);
int RA_SOCKET_CALL bind(SOCKET socket_handle, const SOCKADDR* address, int address_length);
int RA_SOCKET_CALL listen(SOCKET socket_handle, int backlog);
SOCKET RA_SOCKET_CALL accept(SOCKET socket_handle, SOCKADDR* address, int* address_length);
int RA_SOCKET_CALL connect(SOCKET socket_handle, const SOCKADDR* address, int address_length);
int RA_SOCKET_CALL recv(SOCKET socket_handle, char* buffer, int buffer_length, int flags);
int RA_SOCKET_CALL send(SOCKET socket_handle, const char* buffer, int buffer_length, int flags);
int RA_SOCKET_CALL recvfrom(SOCKET socket_handle, char* buffer, int buffer_length, int flags, SOCKADDR* address, int* address_length);
int RA_SOCKET_CALL sendto(SOCKET socket_handle, const char* buffer, int buffer_length, int flags, const SOCKADDR* address, int address_length);
int RA_SOCKET_CALL closesocket(SOCKET socket_handle);
int RA_SOCKET_CALL ioctlsocket(SOCKET socket_handle, long command, u_long* argument);
int RA_SOCKET_CALL setsockopt(SOCKET socket_handle, int level, int option_name, const char* option_value, int option_length);
int RA_SOCKET_CALL getsockopt(SOCKET socket_handle, int level, int option_name, char* option_value, int* option_length);
int RA_SOCKET_CALL getpeername(SOCKET socket_handle, SOCKADDR* address, int* address_length);
int RA_SOCKET_CALL gethostname(char* name, int name_length);
int RA_SOCKET_CALL select(int ignored, fd_set* read_set, fd_set* write_set, fd_set* error_set, const timeval* timeout);
u_short RA_SOCKET_CALL htons(u_short hostshort);
u_short RA_SOCKET_CALL ntohs(u_short netshort);
u_long RA_SOCKET_CALL htonl(u_long hostlong);
u_long RA_SOCKET_CALL ntohl(u_long netlong);
u_long RA_SOCKET_CALL inet_addr(const char* cp);
char* RA_SOCKET_CALL inet_ntoa(struct in_addr in);
hostent* RA_SOCKET_CALL gethostbyname(const char* name);
hostent* RA_SOCKET_CALL gethostbyaddr(const char* address, int length, int type);
}

#else

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using SOCKET = int;

struct WSADATA {
	uint16_t wVersion;
	uint16_t wHighVersion;
	char szDescription[257];
	char szSystemStatus[129];
	uint16_t iMaxSockets;
	uint16_t iMaxUdpDg;
	char* lpVendorInfo;
};

using SOCKADDR = struct sockaddr;
using LPSOCKADDR = struct sockaddr*;
using SOCKADDR_IN = struct sockaddr_in;
using LPSOCKADDR_IN = struct sockaddr_in*;
using IN_ADDR = struct in_addr;
using LPIN_ADDR = struct in_addr*;
using LINGER = struct linger;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

#ifndef MAXGETHOSTSTRUCT
#define MAXGETHOSTSTRUCT 1024
#endif

#ifndef FD_READ
#define FD_READ 0x0001
#endif

#ifndef FD_WRITE
#define FD_WRITE 0x0002
#endif

#ifndef FD_OOB
#define FD_OOB 0x0004
#endif

#ifndef FD_ACCEPT
#define FD_ACCEPT 0x0008
#endif

#ifndef FD_CONNECT
#define FD_CONNECT 0x0010
#endif

#ifndef FD_CLOSE
#define FD_CLOSE 0x0020
#endif

#ifndef WSAEWOULDBLOCK
#define WSAEWOULDBLOCK EWOULDBLOCK
#endif

#ifndef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#endif

#ifndef LOWORD
#define LOWORD(l) ((uint16_t)((uintptr_t)(l) & 0xffff))
#endif

#ifndef HIWORD
#define HIWORD(l) ((uint16_t)(((uintptr_t)(l) >> 16) & 0xffff))
#endif

#define WSAGETASYNCERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTERROR(l_param) HIWORD(l_param)
#define WSAGETSELECTEVENT(l_param) LOWORD(l_param)

inline int WSAStartup(uint16_t version, WSADATA* data)
{
	if (data != nullptr) {
		std::memset(data, 0, sizeof(*data));
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

#endif

#endif
