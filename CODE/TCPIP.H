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

/* $Header: /CounterStrike/TCPIP.H 1     3/03/97 10:25a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : TCPIP.CPP                                *
 *                                                                         *
 *                   Programmer : Steve Tall                               *
 *                                                                         *
 *                   Start Date : March 11th, 1996                         *
 *                                                                         *
 *                  Last Update : March 11th, 1996 [ST]                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 *                                                                         *
 *                                                                         *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *                                                                         *
 *                                                                         *
 *                                                                         *
 *                                                                         *
 *                                                                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "SOCKETS.H"

#include <stdint.h>
#include <vector>
#ifndef WOLAPI_INTEGRATION
extern bool Server;
#endif

#define PORTNUM						0x1000
#define UDP_PORT					0x1001
#define WS_INTERNET_BUFFER_LEN  1024
#define WS_NUM_TX_BUFFERS		16		//Must be a power of 2
#define WS_NUM_RX_BUFFERS		16		//MUst be a power of 2
#define WS_RECEIVE_BUFFER_LEN	1024
//#define WS_IN_BUFFER_LEN			8192
//#define WS_OUT_BUFFER_LEN		8192

#define PLANET_WESTWOOD_HANDLE_MAX 20
#define PLANET_WESTWOOD_PASSWORD_MAX 20
#define IP_ADDRESS_MAX 40
#define PORT_NUMBER_MAX 6

class UDPAddressClass;


class TcpipManagerClass {

	public:

		TcpipManagerClass(void);
		~TcpipManagerClass(void);

		int32_t Init(void);
		void Service(void);
		void Start_Server(void);
		void Start_Client(void);
		void Close_Socket(SocketHandle s);
		void Copy_To_In_Buffer(const void* data, int bytes);
		int  Read(void *buffer, int buffer_len);
		void Write(void *buffer, int buffer_len);
		int32_t Add_Client(void);
		void Close(void);
		void Set_Host_Address(char const *address);
		void Set_Port_Number(uint16_t port);
		void Get_Remote_Address(UDPAddressClass *address) const;
		void Set_Protocol_UDP(int32_t state);
		void Set_Protocol_TCP(int32_t state);
		void Clear_Socket_Error(SocketHandle socket);

		inline int32_t Get_Connected(void) {return (Connected);}
		inline int32_t Using_TCP(void) const { return !UseUDP; }
		inline int32_t Using_UDP(void) const { return UseUDP; }
		inline int32_t Uses_Reliable_Transport(void) const { return Connected && !UseUDP; }

		typedef enum ConnectStatusEnum {
			CONNECTED_OK = 0,
			NOT_CONNECTING,
			CONNECTING,
			UNABLE_TO_CONNECT_TO_SERVER,
			CONTACTING_SERVER,
			SERVER_ADDRESS_LOOKUP_FAILED,
			RESOLVING_HOST_ADDRESS,
			UNABLE_TO_ACCEPT_CLIENT,
			UNABLE_TO_CONNECT,
			CONNECTION_LOST
		} ConnectStatusEnum;

		inline ConnectStatusEnum Get_Connection_Status(void) {return (ConnectStatus);}

	private:

		//...........................................................................
		// This structure defines all the info we need about a host
		//...........................................................................
		typedef struct {
			uint32_t Address;						// address, network byte order
			char DotAddr[SOCKET_MAX_DOT_ADDRESS];		// decimal-dot address string
			char Name[255];						// character-string name
		} HostType;

		typedef struct {
			char	Buffer[WS_INTERNET_BUFFER_LEN];
			int	DataLength;
			bool	InUse:1;
		} InternetBufferType;


		int32_t 					WinsockInitialised;
		SocketHandle		ListenSocket;
		SocketHandle		ConnectSocket;
		SocketHandle		UDPSocket;
		uint32_t			ClientIPAddress;
		char					ClientName[128];
		char					ReceiveBuffer[WS_RECEIVE_BUFFER_LEN];
		//char					InBuffer[WS_IN_BUFFER_LEN];
		//int					InBufferHead;
		//int					InBufferTail;
		//char					OutBuffer[WS_OUT_BUFFER_LEN];
		//int					OutBufferHead;
		//int					OutBufferTail;
		int32_t					IsServer;
		int32_t					Connected;
		HostType				Server;
		char					HostAddress[IP_ADDRESS_MAX];
		ConnectStatusEnum ConnectStatus;
		int32_t					UseUDP;
		uint32_t			UDPIPAddress;
		int					SocketReceiveBuffer;
		int					SocketSendBuffer;
		InternetBufferType ReceiveBuffers[WS_NUM_RX_BUFFERS];
		InternetBufferType TransmitBuffers[WS_NUM_TX_BUFFERS];
		int					TXBufferHead;
		int					TXBufferTail;
		int					RXBufferHead;
		int					RXBufferTail;
		int32_t					ConnectInProgress;
		std::vector<uint8_t> StreamReceiveBuffer;
		uint8_t				StreamSendBuffer[WS_INTERNET_BUFFER_LEN + sizeof(uint32_t)];
		int					StreamSendLength;
		int					StreamSendOffset;
		int32_t					StreamSendActive;

};


extern TcpipManagerClass Winsock;

extern char PlanetWestwoodIPAddress[IP_ADDRESS_MAX];
extern uint16_t PlanetWestwoodPortNumber;
extern bool PlanetWestwoodIsHost;


#define TXT_WINSOCK_CONNECTING							4567+13
#define TXT_WINSOCK_NOT_CONNECTING					4567+14
#define TXT_WINSOCK_UNABLE_TO_CONNECT_TO_SERVER	4567+15
#define TXT_WINSOCK_CONTACTING_SERVER				4567+16
#define TXT_WINSOCK_SERVER_ADDRESS_LOOKUP_FAILED	4567+17
#define TXT_WINSOCK_UNABLE_TO_ACCEPT_CLIENT		4567+18
#define TXT_WINSOCK_UNABLE_TO_CONNECT				4567+19
#define TXT_WINSOCK_CONNECTION_LOST					4567+20
#define TXT_WINSOCK_RESOLVING_HOST_ADDRESS			4567+21
