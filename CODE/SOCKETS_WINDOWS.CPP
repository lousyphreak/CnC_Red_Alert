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

#include "SOCKETS.H"

#include <winsock.h>

#include <cstring>

namespace {

int Map_Socket_Type(int type)
{
	switch (type) {
		case SOCKET_TYPE_STREAM:
			return SOCK_STREAM;
		case SOCKET_TYPE_DATAGRAM:
			return SOCK_DGRAM;
		default:
			return 0;
	}
}

int Map_Socket_Protocol(int protocol)
{
	switch (protocol) {
		case SOCKET_PROTOCOL_DEFAULT:
			return 0;
		case SOCKET_PROTOCOL_TCP:
			return IPPROTO_TCP;
		case SOCKET_PROTOCOL_UDP:
			return IPPROTO_UDP;
		default:
			return 0;
	}
}

bool Map_Socket_Option(int option_name, int* level, int* native_option)
{
	switch (option_name) {
		case SOCKET_OPTION_REUSE_ADDRESS:
			*level = SOL_SOCKET;
			*native_option = SO_REUSEADDR;
			return true;
		case SOCKET_OPTION_BROADCAST:
			*level = SOL_SOCKET;
			*native_option = SO_BROADCAST;
			return true;
		case SOCKET_OPTION_SEND_BUFFER:
			*level = SOL_SOCKET;
			*native_option = SO_SNDBUF;
			return true;
		case SOCKET_OPTION_RECEIVE_BUFFER:
			*level = SOL_SOCKET;
			*native_option = SO_RCVBUF;
			return true;
		case SOCKET_OPTION_TCP_NO_DELAY:
			*level = IPPROTO_TCP;
			*native_option = TCP_NODELAY;
			return true;
		default:
			return false;
	}
}

void Fill_Native_Address(SocketAddress const* address, sockaddr_in* native_address)
{
	std::memset(native_address, 0, sizeof(*native_address));
	native_address->sin_family = AF_INET;
	if (address != nullptr) {
		native_address->sin_addr.s_addr = address->Address;
		native_address->sin_port = address->Port;
	}
}

void Fill_Abstract_Address(sockaddr_in const* native_address, SocketAddress* address)
{
	if (address == nullptr) {
		return;
	}

	address->Address = native_address->sin_addr.s_addr;
	address->Port = native_address->sin_port;
}

SOCKET To_Native_Socket(SocketHandle socket_handle)
{
	return static_cast<SOCKET>(socket_handle);
}

SocketHandle From_Native_Socket(SOCKET socket_handle)
{
	return socket_handle == INVALID_SOCKET ? INVALID_SOCKET_HANDLE : static_cast<SocketHandle>(socket_handle);
}

}

bool Socket_Startup(void)
{
	WSADATA data;
	return WSAStartup(MAKEWORD(1, 1), &data) == 0;
}

void Socket_Cleanup(void)
{
	WSACleanup();
}

int Socket_Get_Last_Error(void)
{
	return WSAGetLastError();
}

bool Socket_Error_Is_Would_Block(int error_code)
{
	return error_code == WSAEWOULDBLOCK;
}

bool Socket_Error_Is_Connection_Reset(int error_code)
{
	return error_code == WSAECONNRESET;
}

SocketHandle Socket_Open(int type, int protocol)
{
	const int native_type = Map_Socket_Type(type);
	const int native_protocol = Map_Socket_Protocol(protocol);
	if (native_type == 0) {
		return INVALID_SOCKET_HANDLE;
	}

	return From_Native_Socket(socket(AF_INET, native_type, native_protocol));
}

int Socket_Close(SocketHandle socket_handle)
{
	if (socket_handle == INVALID_SOCKET_HANDLE) {
		return 0;
	}

	return closesocket(To_Native_Socket(socket_handle));
}

int Socket_Bind(SocketHandle socket_handle, SocketAddress const* address)
{
	sockaddr_in native_address;
	Fill_Native_Address(address, &native_address);
	return bind(To_Native_Socket(socket_handle), reinterpret_cast<sockaddr*>(&native_address), sizeof(native_address));
}

int Socket_Listen(SocketHandle socket_handle, int backlog)
{
	return listen(To_Native_Socket(socket_handle), backlog);
}

SocketHandle Socket_Accept(SocketHandle socket_handle, SocketAddress* address)
{
	sockaddr_in native_address;
	int address_length = sizeof(native_address);
	SOCKET accepted_socket = accept(To_Native_Socket(socket_handle), reinterpret_cast<sockaddr*>(&native_address), &address_length);
	if (accepted_socket == INVALID_SOCKET) {
		return INVALID_SOCKET_HANDLE;
	}

	Fill_Abstract_Address(&native_address, address);
	return From_Native_Socket(accepted_socket);
}

int Socket_Connect(SocketHandle socket_handle, SocketAddress const* address)
{
	sockaddr_in native_address;
	Fill_Native_Address(address, &native_address);
	return connect(To_Native_Socket(socket_handle), reinterpret_cast<sockaddr*>(&native_address), sizeof(native_address));
}

int Socket_Read(SocketHandle socket_handle, void* buffer, int buffer_length)
{
	return recv(To_Native_Socket(socket_handle), static_cast<char*>(buffer), buffer_length, 0);
}

int Socket_Write(SocketHandle socket_handle, void const* buffer, int buffer_length)
{
	return send(To_Native_Socket(socket_handle), static_cast<char const*>(buffer), buffer_length, 0);
}

int Socket_Read_From(SocketHandle socket_handle, void* buffer, int buffer_length, SocketAddress* address)
{
	sockaddr_in native_address;
	int address_length = sizeof(native_address);
	int rc = recvfrom(To_Native_Socket(socket_handle),
		static_cast<char*>(buffer),
		buffer_length,
		0,
		reinterpret_cast<sockaddr*>(&native_address),
		&address_length);
	if (rc >= 0) {
		Fill_Abstract_Address(&native_address, address);
	}
	return rc;
}

int Socket_Write_To(SocketHandle socket_handle, void const* buffer, int buffer_length, SocketAddress const* address)
{
	sockaddr_in native_address;
	Fill_Native_Address(address, &native_address);
	return sendto(To_Native_Socket(socket_handle),
		static_cast<char const*>(buffer),
		buffer_length,
		0,
		reinterpret_cast<sockaddr const*>(&native_address),
		sizeof(native_address));
}

int Socket_Set_NonBlocking(SocketHandle socket_handle, bool nonblocking)
{
	u_long mode = nonblocking ? 1UL : 0UL;
	return ioctlsocket(To_Native_Socket(socket_handle), FIONBIO, &mode);
}

int Socket_Set_Integer_Option(SocketHandle socket_handle, int option_name, int value)
{
	int level = 0;
	int native_option = 0;
	if (!Map_Socket_Option(option_name, &level, &native_option)) {
		return SOCKET_ERROR;
	}

	return setsockopt(To_Native_Socket(socket_handle), level, native_option, reinterpret_cast<char const*>(&value), sizeof(value));
}

int Socket_Set_Linger(SocketHandle socket_handle, bool enabled, int seconds)
{
	linger linger_value;
	linger_value.l_onoff = enabled ? 1 : 0;
	linger_value.l_linger = static_cast<u_short>(seconds);
	return setsockopt(To_Native_Socket(socket_handle), SOL_SOCKET, SO_LINGER, reinterpret_cast<char const*>(&linger_value), sizeof(linger_value));
}

void Socket_Clear_Error(SocketHandle socket_handle)
{
	int error_code = 0;
	int length = sizeof(error_code);
	getsockopt(To_Native_Socket(socket_handle), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error_code), &length);
	error_code = 0;
	setsockopt(To_Native_Socket(socket_handle), SOL_SOCKET, SO_ERROR, reinterpret_cast<char const*>(&error_code), length);
}

bool Socket_Is_Write_Ready(SocketHandle socket_handle)
{
	fd_set write_set;
	fd_set error_set;
	timeval timeout;

	FD_ZERO(&write_set);
	FD_ZERO(&error_set);
	FD_SET(To_Native_Socket(socket_handle), &write_set);
	FD_SET(To_Native_Socket(socket_handle), &error_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int rc = select(0, nullptr, &write_set, &error_set, &timeout);
	if (rc <= 0) {
		return false;
	}

	return FD_ISSET(To_Native_Socket(socket_handle), &write_set) != 0 ||
		FD_ISSET(To_Native_Socket(socket_handle), &error_set) != 0;
}

bool Socket_Get_Peer_Address(SocketHandle socket_handle, SocketAddress* address)
{
	sockaddr_in native_address;
	int address_length = sizeof(native_address);
	if (getpeername(To_Native_Socket(socket_handle), reinterpret_cast<sockaddr*>(&native_address), &address_length) != 0) {
		return false;
	}

	Fill_Abstract_Address(&native_address, address);
	return true;
}

bool Socket_Get_Host_Name(char* name, int name_length)
{
	return gethostname(name, name_length) == 0;
}

bool Socket_Lookup_Host_By_Name(char const* name, SocketHostEntry* entry)
{
	hostent* host_entry = gethostbyname(name);
	if (host_entry == nullptr || host_entry->h_addr_list == nullptr || entry == nullptr) {
		return false;
	}

	std::memset(entry, 0, sizeof(*entry));
	if (host_entry->h_name != nullptr) {
		std::strncpy(entry->Name, host_entry->h_name, sizeof(entry->Name) - 1);
	} else {
		std::strncpy(entry->Name, name, sizeof(entry->Name) - 1);
	}

	for (char** address_list = host_entry->h_addr_list;
		*address_list != nullptr && entry->AddressCount < SOCKET_MAX_HOST_ADDRESSES;
		++address_list) {
		std::memcpy(&entry->Addresses[entry->AddressCount], *address_list, sizeof(entry->Addresses[0]));
		++entry->AddressCount;
	}

	return entry->AddressCount > 0;
}

bool Socket_Lookup_Host_By_Address(uint32_t address, char* name, int name_length)
{
	hostent* host_entry = gethostbyaddr(reinterpret_cast<char const*>(&address), sizeof(address), AF_INET);
	if (host_entry == nullptr || host_entry->h_name == nullptr) {
		return false;
	}

	std::strncpy(name, host_entry->h_name, name_length - 1);
	name[name_length - 1] = '\0';
	return true;
}

bool Socket_Address_From_String(char const* text, uint32_t* address)
{
	u_long native_address = inet_addr(text);
	if (native_address == INADDR_NONE && std::strcmp(text, "255.255.255.255") != 0) {
		return false;
	}

	*address = native_address;
	return true;
}

bool Socket_Address_To_String(uint32_t address, char* buffer, int buffer_length)
{
	in_addr native_address;
	native_address.s_addr = address;
	char const* dot_address = inet_ntoa(native_address);
	if (dot_address == nullptr) {
		return false;
	}

	std::strncpy(buffer, dot_address, buffer_length - 1);
	buffer[buffer_length - 1] = '\0';
	return true;
}

bool Socket_Address_Is_Private(uint32_t address)
{
	uint8_t const* octets = reinterpret_cast<uint8_t const*>(&address);
	return octets[0] == 10 ||
		(octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) ||
		(octets[0] == 192 && octets[1] == 168);
}

uint16_t Socket_Host_To_Network16(uint16_t value)
{
	return htons(value);
}

uint16_t Socket_Network_To_Host16(uint16_t value)
{
	return ntohs(value);
}

uint32_t Socket_Host_To_Network32(uint32_t value)
{
	return htonl(value);
}

uint32_t Socket_Network_To_Host32(uint32_t value)
{
	return ntohl(value);
}
