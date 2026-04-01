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

/* $Header: /CounterStrike/IPX.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : IPX.CPP                                  *
 *                                                                         *
 *                   Programmer : Barry Nance										*
 * 										 from Client/Server LAN Programming			*
 *											 Westwood-ized by Bill Randolph				*
 *                                                                         *
 *                   Start Date : December 14, 1994                        *
 *                                                                         *
 *                  Last Update : December 15, 1994   [BR]                 *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Pitfalls:																					*
 * - Never try to use a closed socket; always check the return code from	*
 *   IPX_Open_Socket().																		*
 * - Always give IPX an outstanding ECB for listening, before you send.		*
 * - It turns out that IPX is pretty bad about saving registers, so if		*
 *   you have any register variables in your program, they may get			*
 *   trashed.  To circumvent this, all functions in this module save &		*
 *   restore the registers before invoking any IPX or NETX function.			*
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   IPX_SPX_Installed -- checks for installation of IPX/SPX               *
 *   IPX_Open_Socket -- opens an IPX socket for sending or receiving       *
 *   IPX_Close_Socket -- closes an open socket                             *
 *   IPX_Get_Connection_Number -- gets local Connection Number					*
 *   IPX_Get_1st_Connection_Num -- gets 1st Connect Number for given user  *
 *   IPX_Get_Internet_Address -- gets Network Number & Node Address			*
 *   IPX_Get_User_ID -- gets user ID from Connection Number                *
 *   IPX_Listen_For_Packet -- commands IPX to listen for a packet          *
 *   IPX_Send_Packet -- commands IPX to send a packet                      *
 *   IPX_Get_Local_Target -- fills in ImmediateAddress field of ECB        *
 *   IPX_Cancel_Event -- cancels an operation in progress                  *
 *   Let_IPX_Breath -- gives IPX some CPU time                             *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "FUNCTION.H"
#include <stdio.h>
#include <string.h>
#include "IPX.H"

#include "IPX95.H"


// Turn off "expression is not meaningful".
#pragma warning 628 9


/***************************************************************************
 * IPX_SPX_Installed -- checks for installation of IPX/SPX                 *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = not installed; 1 = IPX only, 2 = IPX and SPX are installed			*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/14/1994 BR : Created.                                              *
 *=========================================================================*/
int IPX_SPX_Installed(void)
{

#ifdef TIBERIAN_SUN
	return(false);
#else
	if ( Load_IPX_Dll () ){
		return ( IPX_Initialise() );
	}else{
		return(false);
	}
#endif

}	/* end of IPX_SPX_Installed */


/***************************************************************************
 * IPX_Open_Socket -- opens an IPX socket for sending or receiving         *
 *                                                                         *
 * INPUT:                                                                  *
 *		socket		the socket number to open											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = OK																					*
 *		-1 = IPX not installed																*
 *		0xfe = socket table is full														*
 *		0xff = socket is already open														*
 *                                                                         *
 * WARNINGS:                                                               *
 *		The application must define its socket number carefully.  Use			*
 *		values from 0x4000 to 0x8000 for custom socket numbers.  The app		*
 *		must know its own socket number as well as the socket number of		*
 *		a destination workstation.															*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/


/***************************************************************************
 * IPX_Close_Socket -- closes an open socket                               *
 *                                                                         *
 * INPUT:                                                                  *
 *		socket		socket number to close												*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = ok, -1 = error																	*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/


/***************************************************************************
 * IPX_Get_Connection_Number -- gets local Connection Number					*
 *                                                                         *
 * This Novell call will the return the user's local "Connection Number".	*
 * This value will be 0 if the user isn't logged into Novell, so this		*
 * routine can be used to detect if other calls (such as Get_Local_Target)	*
 * will be OK.																					*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		Connection Number, 0 = none														*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/


/***************************************************************************
 * IPX_Get_1st_Connection_Num -- gets 1st Connect Number for given user    *
 *                                                                         *
 * This gets the Connection Number for the given User ID.  Since a user		*
 * may be logged in more than once, this just returns the first connection	*
 * found and ignores the others.															*
 *                                                                         *
 * INPUT:                                                                  *
 *		username		name of the user to get the Connection Number for			*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		first-found Connection Number for that user, 0 if user not logged in	*
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Get_Internet_Address -- gets Network Number & Node Address				*
 *                                                                         *
 * Once you've obtained a Connection Number from IPX_Get_Connection_Number	*
 * or IPX_Get_1st_Connection_Num, use this function to translate it into	*
 * a Network Number and Node Address; then, place those numbers in the		*
 * IPX header for outgoing packets.														*
 *                                                                         *
 * INPUT:                                                                  *
 *		connection_number		Connection Number to translate						*
 *		network_number			ptr: will hold Network Number							*
 *		physical_node			ptr: will hold Node Address							*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = OK, -1 = error																	*
 *                                                                         *
 * WARNINGS:                                                               *
 *		If connection_number is 0 and NETX isn't running, this routine			*
 *		will just put garbage into the network_number and physical_node.		*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Get_User_ID -- gets user ID from Connection Number                  *
 *                                                                         *
 * INPUT:                                                                  *
 *		connection_number		Connection Number to get User ID for				*
 *		user_id					ptr to buffer to put User ID into; 					*
 *									size must be >= 48 chars								*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = OK, -1 = error																	*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Listen_For_Packet -- commands IPX to listen for a packet            *
 *                                                                         *
 * Before calling this function, you must fill in an ECB:						*
 *		SocketNumber:				must contain the socket you've opened,			*
 *										and are "listening" on								*
 *		Event_Service_Routine:	optionally points to a callback routine		*
 *		PacketCount:				set to 2, to tell IPX there are 2 areas to	*
 *										store the incoming data in							*
 *		Packet[0].Address:		set to the address of an IPXHeaderType			*
 *		Packet[0].Length:			sizeof(IPXHeaderType)								*
 *		Packet[1].Address:		address of data buffer, for the packet			*
 *		Packet[1].Length:			size of the data buffer								*
 *                                                                         *
 * When the packet is received, ECBType.CompletionCode will be 0 if 			*
 * successful.  Otherwise, some error occurred.										*
 *                                                                         *
 * You should initialize the ECB to 0's before filling it in.					*
 *                                                                         *
 * INPUT:                                                                  *
 *		ecb_ptr		pointer to a filled-in ECB; MUST be real-mode memory		*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = OK, IPX error otherwise														*
 *                                                                         *
 * WARNINGS:                                                               *
 *		The ECB must be located in real-mode memory, as well as the values	*
 *		pointed to in the ECB (the IPX Header, the buffer to send, etc.)		*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Send_Packet -- commands IPX to send a packet                        *
 *                                                                         *
 * Before calling this function, you must fill in an ECB:						*
 *		SocketNumber:				must contain the socket you've opened,			*
 *										and are sending on									*
 *		Event_Service_Routine:	optionally points to a callback routine		*
 *		PacketCount:				set to 2, to tell IPX there are 2 areas the	*
 *										outgoing data is stored in							*
 *		Packet[0].Address:		set to the address of an IPXHeaderType			*
 *		Packet[0].Length:			sizeof(IPXHeaderType)								*
 *		Packet[1].Address:		address of buffer containing data to send		*
 *		Packet[1].Length:			size of the data buffer								*
 *		ImmediateAddress:			must be filled in with the node address of	*
 *										the bridge that will route the message;		*
 *										fill this in by calling IPX_Get_Local_Target	*
 *                                                                         *
 * Also, you must fill in the IPXHeaderType with certain values:				*
 *		PacketType:					set to 4 for IPX										*
 *		DestNetworkNumber:		Network Number of the destination system		*
 *		DestNetworkNode:			Node Address of the destination system			*
 *		DestNetworkSocket:		the destination system's socket to send to;	*
 *										this doesn't have to be the same as the		*
 *										socket opened on the local machine.				*
 *                                                                         *
 * You should initialize the ECB & IPXHeader to 0's before filling them in.*
 *                                                                         *
 * INPUT:                                                                  *
 *		ecb_ptr		pointer to a filled-in ECB											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.  This function doesn't return anything; the caller must check	*
 *    its ECB.CompletionCode for: 0 = OK, IPX Error otherwise.					*
 *                                                                         *
 * WARNINGS:                                                               *
 *		The ECB must be located in real-mode memory, as well as the values	*
 *		pointed to in the ECB (the IPX Header, the buffer to send, etc.)		*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Get_Local_Target -- fills in ImmediateAddress field of ECB          *
 *                                                                         *
 * Use this function to fill in the ECB's ImmediateAddress field when		*
 * sending a packet.  The Immediate Address is the node address of the		*
 * bridge that must route the message.  If there is no bridge, it's 			*
 * filled in with the destination Node Address.  In either case, it			*
 * will contain the proper value for sending.										*
 *                                                                         *
 * INPUT:                                                                  *
 *		dest_network			destination Network Number								*
 *		dest_node				destination Node Address								*
 *		dest_socket				destination Socket Number								*
 *		bridge_address			field to fill in with Immediate Address			*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = OK, error otherwise																*
 *                                                                         *
 * WARNINGS:                                                               *
 *		If the Connection Number is 0 (user not logged in), dest_network		*
 *		and dest_node will be garbage, and this routine will probably crash.	*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * IPX_Cancel_Event -- cancels an operation in progress                    *
 *                                                                         *
 * INPUT:                                                                  *
 *		ecb_ptr		pointer to ECB event to cancel									*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		???																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		The ECB must be located in real-mode memory, as well as the values	*
 *		pointed to in the ECB (the IPX Header, the buffer to send, etc.)		*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/

/***************************************************************************
 * Let_IPX_Breath -- gives IPX some CPU time                               *
 *                                                                         *
 * Use this function if you're polling the ECB's InUse flag, waiting			*
 * for it to go to 0:																		*
 *                                                                         *
 *		while ECBType.InUse																	*
 *			Let_IPX_Breath();																	*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/15/1994 BR : Created.                                              *
 *=========================================================================*/
/**************************** end of ipx.cpp *******************************/
