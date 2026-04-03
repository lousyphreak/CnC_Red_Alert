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

/* $Header: /CounterStrike/UDPCONN.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : UDPCONN.CPP                              *
 *                                                                         *
 *                   Programmer : Bill Randolph                            *
 *                                                                         *
 *                   Start Date : December 20, 1994                        *
 *                                                                         *
 *                  Last Update : April 9, 1995 [BRR]								*
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:    			                                                   *
 *   UDPConnClass::UDPConnClass -- class constructor           				*
 *   UDPConnClass::~UDPConnClass -- class destructor           				*
 *   UDPConnClass::Init -- hardware-specific initialization routine			*
 *   UDPConnClass::Configure -- One-time initialization routine				*
 *   UDPConnClass::Start_Listening -- commands UDP to listen               *
 *   UDPConnClass::Stop_Listening -- commands UDP to stop  listen          *
 *   UDPConnClass::Send -- sends a packet; invoked by SequencedConnection	*
 *   UDPConnClass::Open_Socket -- opens communications socket    				*
 *   UDPConnClass::Close_Socket -- closes the socket 								*
 *   UDPConnClass::Send_To -- sends the packet to the given address			*
 *   UDPConnClass::Broadcast -- broadcasts the given packet						*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#include "FUNCTION.H"
#include "SOCKETS.H"
#include "UDPCONN.H"
#include "TCPIP.H"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

namespace
{
using SocketLengthType =
#ifdef _WIN32
	int;
#else
	socklen_t;
#endif

SOCKET UDPListenSocket = INVALID_SOCKET;

void Close_UDP_Socket()
{
	if (UDPListenSocket != INVALID_SOCKET) {
		closesocket(UDPListenSocket);
		UDPListenSocket = INVALID_SOCKET;
	}
}

bool Set_UDP_NonBlocking(SOCKET socket_handle)
{
#ifdef _WIN32
	u_long nonblocking = 1;
	return ioctlsocket(socket_handle, FIONBIO, &nonblocking) == 0;
#else
	const int flags = fcntl(socket_handle, F_GETFL, 0);
	if (flags < 0) {
		return false;
	}

	return fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool Set_UDP_Socket_Option(SOCKET socket_handle, int option_name, int value)
{
	return setsockopt(socket_handle, SOL_SOCKET, option_name, reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool Is_All_Value(const uint8_t* bytes, int count, uint8_t value)
{
	for (int index = 0; index < count; ++index) {
		if (bytes[index] != value) {
			return false;
		}
	}

	return true;
}

void Fill_Address_Bytes(const SOCKADDR_IN& socket_address, NetNumType net, NetNodeType node)
{
	const uint32_t address = socket_address.sin_addr.s_addr;
	const uint16_t port = socket_address.sin_port;

	memcpy(net, &address, sizeof(address));
	memcpy(node, &port, sizeof(port));
	memset(node + sizeof(port), 0, sizeof(NetNodeType) - sizeof(port));
}

void Fill_Socket_Address(UDPAddressClass* address, uint16_t default_socket, SOCKADDR_IN& socket_address)
{
	NetNumType net;
	NetNodeType node;
	uint16_t port = htons(default_socket);

	memset(&socket_address, 0, sizeof(socket_address));
	socket_address.sin_family = AF_INET;

	address->Get_Address(net, node);
	memcpy(&socket_address.sin_addr.s_addr, net, sizeof(net));

	if (!Is_All_Value(node, sizeof(node), 0x00) && !Is_All_Value(node, sizeof(node), 0xff)) {
		memcpy(&port, node, sizeof(port));
	}

	socket_address.sin_port = port;
}
}

/*
********************************* Globals ***********************************
*/
uint16_t 	UDPConnClass::Socket;
int			 		UDPConnClass::ConnectionNum;
ECBType *			UDPConnClass::ListenECB;
UDPHeaderType *	UDPConnClass::ListenHeader;
char *				UDPConnClass::ListenBuf;
ECBType *			UDPConnClass::SendECB;
UDPHeaderType *	UDPConnClass::SendHeader;
char *				UDPConnClass::SendBuf;
int32_t 					UDPConnClass::Handler;
int 					UDPConnClass::Configured = 0;
int 					UDPConnClass::SocketOpen = 0;
int 					UDPConnClass::Listening = 0;
int					UDPConnClass::PacketLen;


/***************************************************************************
 * UDPConnClass::UDPConnClass -- class constructor             				*
 *=========================================================================*/
UDPConnClass::UDPConnClass (int numsend, int numreceive, int maxlen,
	uint16_t magicnum, UDPAddressClass *address, int id, char *name,
	int extralen) :
	ConnectionClass (numsend, numreceive, maxlen, magicnum,
		2,
		-1,
		60,
		extralen)
{
	if (address) {
		Address = (*address);
	}

	ID = id;
	strcpy(Name, name);
	memset(ImmediateAddress, 0, sizeof(ImmediateAddress));
	Immed_Set = 0;
}	/* end of UDPConnClass */


/***************************************************************************
 * UDPConnClass::Init -- hardware-specific initialization routine				*
 *=========================================================================*/
void UDPConnClass::Init (void)
{
	ConnectionClass::Init();
}	/* end of Init */


/***************************************************************************
 * UDPConnClass::Configure -- One-time initialization routine					*
 *=========================================================================*/
void UDPConnClass::Configure (uint16_t socket, int conn_num,
	ECBType *listen_ecb, ECBType *send_ecb, UDPHeaderType *listen_header,
	UDPHeaderType *send_header, char *listen_buf, char *send_buf,
	int32_t handler_rm_ptr, int maxpacketlen)
{
	Socket = socket;
	ConnectionNum = conn_num;
	ListenECB = listen_ecb;
	SendECB = send_ecb;
	ListenHeader = listen_header;
	SendHeader = send_header;
	ListenBuf = listen_buf;
	SendBuf = send_buf;
	Handler = handler_rm_ptr;
	PacketLen = maxpacketlen;
	Configured = 1;
}	/* end of Configure */


/***************************************************************************
 * UDPConnClass::Start_Listening -- commands UDP to listen                 *
 *=========================================================================*/
int UDPConnClass::Start_Listening(void)
{
	if (Winsock.Get_Connected()) {
		Listening = 1;
		return(true);
	}

	if (!Open_Socket(Socket)) {
		return(false);
	}

	Listening = 1;
	return(true);
}	/* end of Start_Listening */


/***************************************************************************
 * UDPConnClass::Stop_Listening -- commands UDP to stop  listen            *
 *=========================================================================*/
int UDPConnClass::Stop_Listening(void)
{
	if (Listening == 0) {
		return(0);
	}

	if (!Winsock.Get_Connected()) {
		Close_Socket(Socket);
	}

	Listening = 0;
	return(1);
}	/* end of Stop_Listening */


/***************************************************************************
 * UDPConnClass::Receive -- polls for a received UDP packet                *
 *=========================================================================*/
int UDPConnClass::Receive(char *buf, int buflen, UDPAddressClass *address)
{
	if (Winsock.Get_Connected() || UDPListenSocket == INVALID_SOCKET) {
		return(0);
	}

	SOCKADDR_IN source_address;
	SocketLengthType source_length = sizeof(source_address);
	const int recv_length = recvfrom(UDPListenSocket,
		buf,
		buflen,
		0,
		reinterpret_cast<SOCKADDR*>(&source_address),
		&source_length);
	if (recv_length == SOCKET_ERROR) {
		const int error = WSAGetLastError();
		if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) {
			return(0);
		}
		return(0);
	}

	if (address != NULL) {
		NetNumType net;
		NetNodeType node;
		Fill_Address_Bytes(source_address, net, node);
		address->Set_Address(net, node);
	}

	return(recv_length);
}	/* end of Receive */


/***************************************************************************
 * UDPConnClass::Send -- sends a packet; invoked by SequencedConnection		*
 *=========================================================================*/
int UDPConnClass::Send(char *buf, int buflen, void *, int)
{
	if (Immed_Set) {
		return(Send_To(buf, buflen, &Address, ImmediateAddress));
	}

	return(Send_To(buf, buflen, &Address, NULL));
}	/* end of Send */


/***************************************************************************
 * UDPConnClass::Open_Socket -- opens communications socket    				*
 *=========================================================================*/
int UDPConnClass::Open_Socket(uint16_t socket)
{
	if (Winsock.Get_Connected()) {
		SocketOpen = 1;
		return(true);
	}

	if (SocketOpen && UDPListenSocket != INVALID_SOCKET) {
		return(true);
	}

	Close_UDP_Socket();
	UDPListenSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (UDPListenSocket == INVALID_SOCKET) {
		SocketOpen = 0;
		return(false);
	}

	const int reuse = 1;
	if (!Set_UDP_Socket_Option(UDPListenSocket, SO_REUSEADDR, reuse) ||
		!Set_UDP_Socket_Option(UDPListenSocket, SO_BROADCAST, reuse) ||
		!Set_UDP_NonBlocking(UDPListenSocket)) {
		Close_UDP_Socket();
		SocketOpen = 0;
		return(false);
	}

	SOCKADDR_IN listen_address;
	memset(&listen_address, 0, sizeof(listen_address));
	listen_address.sin_family = AF_INET;
	listen_address.sin_addr.s_addr = htonl(INADDR_ANY);
	listen_address.sin_port = htons(socket);

	if (bind(UDPListenSocket, reinterpret_cast<const SOCKADDR*>(&listen_address), sizeof(listen_address)) == SOCKET_ERROR) {
		Close_UDP_Socket();
		SocketOpen = 0;
		return(false);
	}

	SocketOpen = 1;
	return(true);
}	/* end of Open_Socket */


/***************************************************************************
 * UDPConnClass::Close_Socket -- closes the socket 								*
 *=========================================================================*/
void UDPConnClass::Close_Socket(uint16_t socket)
{
	socket = socket;

	if (Winsock.Get_Connected()) {
		SocketOpen = 0;
		return;
	}

	if (SocketOpen == 1) {
		Close_UDP_Socket();
	}

	SocketOpen = 0;
}	/* end of Close_Socket */


/***************************************************************************
 * UDPConnClass::Send_To -- sends the packet to the given address				*
 *=========================================================================*/
int UDPConnClass::Send_To(char *buf, int buflen, UDPAddressClass *address,
	NetNodeType immed)
{
	immed = immed;

	if (Winsock.Get_Connected()) {
		Winsock.Write((void*)buf, buflen);
		return(true);
	}

	if (UDPListenSocket == INVALID_SOCKET) {
		return(false);
	}

	SOCKADDR_IN destination_address;
	Fill_Socket_Address(address, Socket, destination_address);

	if (sendto(UDPListenSocket,
			buf,
			buflen,
			0,
			reinterpret_cast<const SOCKADDR*>(&destination_address),
			sizeof(destination_address)) == SOCKET_ERROR) {
		return(false);
	}

	return(true);
}	/* end of Send_To */


/***************************************************************************
 * UDPConnClass::Broadcast -- broadcasts the given packet						*
 *=========================================================================*/
int UDPConnClass::Broadcast(char *buf, int buflen)
{
	if (Winsock.Get_Connected()) {
		Winsock.Write((void*)buf, buflen);
		return(true);
	}

	if (UDPListenSocket == INVALID_SOCKET) {
		return(false);
	}

	SOCKADDR_IN broadcast_address;
	memset(&broadcast_address, 0, sizeof(broadcast_address));
	broadcast_address.sin_family = AF_INET;
	broadcast_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	broadcast_address.sin_port = htons(Socket);

	if (sendto(UDPListenSocket,
			buf,
			buflen,
			0,
			reinterpret_cast<const SOCKADDR*>(&broadcast_address),
			sizeof(broadcast_address)) == SOCKET_ERROR) {
		return(false);
	}

	return(true);
}	/* end of Broadcast */

/************************** end of udpconn.cpp *****************************/
