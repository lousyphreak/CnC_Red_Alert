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

/* $Header: /CounterStrike/UDP.H 1     3/03/97 10:24a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : UDP.H                                    *
 *                                                                         *
 *                   Programmer : Barry Nance										*
 * 										 from Client/Server LAN Programming			*
 *											 Westwood-ized by Bill Randolph				*
 *                                                                         *
 *                   Start Date : December 14, 1994                        *
 *                                                                         *
 *                  Last Update : December 14, 1994   [BR]                 *
 *                                                                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef UDP_H
#define UDP_H
#include <stdint.h>

/*
******************************** Structures *********************************
*/
typedef uint8_t NetNumType[4];
typedef uint8_t NetNodeType[6];
typedef char UserID[48];

/*---------------------------------------------------------------------------
This is the UDP Packet structure.  It's followed by the data itself, which
can be up to 546 bytes long.  Annotation of 'UDP' means UDP will set this
field; annotation of 'APP' means the application must set the field.
NOTE: All header fields are ordered high-byte,low-byte.
---------------------------------------------------------------------------*/
typedef struct UDPHEADER {
	uint16_t CheckSum;					// UDP: Not used; always 0xffff
	uint16_t Length;						// UDP: Total size, incl header & data
	uint8_t TransportControl;			// UDP: # bridges message crossed
	uint8_t PacketType;					// APP: Set to 4 for UDP (5 for SPX)
	uint8_t DestNetworkNumber[4];		// APP: destination Network Number
	uint8_t DestNetworkNode[6];		// APP: destination Node Address
	uint16_t DestNetworkSocket;		// APP: destination Socket Number
	uint8_t SourceNetworkNumber[4];	// UDP: source Network Number
	uint8_t SourceNetworkNode[6];		// UDP: source Node Address
	uint16_t SourceNetworkSocket;		// UDP: source Socket Number
} UDPHeaderType;

/*---------------------------------------------------------------------------
This is the UDP Event Control Block.  It serves as a communications area
between UDP and the application for a single UDP operation.  You should set
up a separate ECB for each UDP operation you perform.
---------------------------------------------------------------------------*/
typedef struct ECB {
	void *Link_Address;
	void (*Event_Service_Routine)(void);	// APP: event handler (NULL=none)
	uint8_t InUse;							// UDP: 0 = event complete
	uint8_t CompletionCode;				// UDP: event's return code
	uint16_t SocketNumber;				// APP: socket to send data through
	uint16_t ConnectionID;				// returned by Listen (???)
	uint16_t RestOfWorkspace;
	uint8_t DriverWorkspace[12];
	uint8_t ImmediateAddress[6];		// returned by Get_Local_Target
	uint16_t PacketCount;
	struct {
		void *Address;
		uint16_t Length;
	} Packet[2];
} ECBType;

#endif

/***************************** end of udp.h ********************************/
