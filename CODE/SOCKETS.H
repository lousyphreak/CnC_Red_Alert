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

#include <winsock.h>

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
