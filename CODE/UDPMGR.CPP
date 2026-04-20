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

/* $Header: /CounterStrike/UDPMGR.CPP 3     10/13/97 2:20p Steve_t $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : UDPMGR.CPP                               *
 *                                                                         *
 *                   Programmer : Bill Randolph                            *
 *                                                                         *
 *                   Start Date : December 20, 1994                        *
 *                                                                         *
 *                  Last Update : May 4, 1995 [BRR]                        *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                       			*
 *   UDPManagerClass::UDPManagerClass -- class constructor                 *
 *   UDPManagerClass::~UDPManagerClass -- class destructor                 *
 *   UDPManagerClass::Init -- initialization routine								*
 *   UDPManagerClass::Is_UDP -- tells if UDP is installed or not				*
 *   UDPManagerClass::Set_Timing -- sets timing for all connections			*
 *   UDPManagerClass::Create_Connection -- creates a new connection        *
 *   UDPManagerClass::Delete_Connection -- deletes a connection            *
 *   UDPManagerClass::Num_Connections -- gets the # of connections			*
 *   UDPManagerClass::Connection_ID -- gets the given connection's ID      *
 *   UDPManagerClass::Connection_Name -- gets name for given connection    *
 *   UDPManagerClass::Connection_Address -- retrieves connection's address	*
 *   UDPManagerClass::Connection_Index -- gets given connection's index    *
 *   UDPManagerClass::Set_Connection_Parms -- sets connection's name & id	*
 *   UDPManagerClass::Send_Global_Message -- sends a Global Message			*
 *   UDPManagerClass::Get_Global_Message -- polls the Global Message queue	*
 *   UDPManagerClass::Send_Private_Message -- Sends a Private Message		*
 *   UDPManagerClass::Get_Private_Message -- Polls Private Message queue	*
 *   UDPManagerClass::Service -- main polling routine for UDP Connections	*
 *   UDPManagerClass::Get_Bad_Connection -- returns bad connection ID      *
 *   UDPManagerClass::Global_Num_Send	-- gets # entries in send queue	   *
 *   UDPManagerClass::Global_Num_Receive -- gets # entries in recv queue   *
 *   UDPManagerClass::Private_Num_Send -- gets # entries in send queue	   *
 *   UDPManagerClass::Private_Num_Receive -- gets # entries in recv queue  *
 *   UDPManagerClass::Set_Bridge -- prepares to cross a bridge             *
 *   UDPManagerClass::Set_Socket -- sets socket ID for all connections		*
 *   UDPManagerClass::Response_Time -- Returns largest Avg Response Time   *
 *   UDPManagerClass::Global_Response_Time -- Returns Avg Response Time    *
 *   UDPManagerClass::Reset_Response_Time -- Reset response time 				*
 *   UDPManagerClass::Oldest_Send -- gets ptr to oldest send buf           *
 *   UDPManagerClass::Mono_Debug_Print -- debug output routine					*
 *   UDPManagerClass::Alloc_RealMode_Mem -- allocates real-mode memory		*
 *   UDPManagerClass::Free_RealMode_Mem -- frees real-mode memory				*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include <stdio.h>
#include <string.h>
#include "UDPMGR.H"
#include "WWLIB32.H"
#include "TCPIP.H"
// to enable mono output

// Turn off "expression is not meaningful".

#include "WOLDEBUG.H"

/***************************************************************************
 * UDPManagerClass::UDPManagerClass -- class constructor                   *
 *                                                                         *
 * INPUT:                                                                  *
 *		glb_maxlen		Global Channel maximum packet length						*
 *		pvt_maxlen		Private Channel maximum packet length						*
 *		socket			socket ID to use													*
 *		product_id		a unique numerical ID for this product						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		The socket number is byte-swapped, since UDP requires socket ID's		*
 *		to be stored high/low.																*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
UDPManagerClass::UDPManagerClass (int glb_maxlen, int pvt_maxlen,
	int glb_num_packets, int pvt_num_packets, uint16_t socket,
	uint16_t product_id)
{
	int i;

#if RA_ENABLE_UDP
	UDPStatus = 1;
#else
	UDPStatus = 0;
#endif

	//........................................................................
	//	Set listening state flag to off
	//........................................................................
	Listening = 0;

	//........................................................................
	//	No memory has been alloc'd yet
	//........................................................................
	RealMemAllocd = 0;

	//........................................................................
	//	Set max packet sizes, for allocating real-mode memory
	//........................................................................
	Glb_MaxPacketLen = glb_maxlen;
	Glb_NumPackets = glb_num_packets;
	Pvt_MaxPacketLen = pvt_maxlen;
	Pvt_NumPackets = pvt_num_packets;

	//........................................................................
	//	Save the app's product ID
	//........................................................................
	ProductID = product_id;

	Socket = socket;

	ConnectionNum = 0;

	//------------------------------------------------------------------------
	//	Init connection states
	//------------------------------------------------------------------------
	NumConnections = 0;
	CurConnection = 0;
	for (i = 0; i < CONNECT_MAX; i++) {
		Connection[i] = 0;
	}
	GlobalChannel = 0;

	SendOverflows = 0;
	ReceiveOverflows = 0;
	BadConnection = CONNECTION_NONE;

	//------------------------------------------------------------------------
	//	Init timing parameters
	//------------------------------------------------------------------------
	RetryDelta = 2;		// 2 ticks between retries
	MaxRetries = -1;		// disregard # retries
	Timeout = 60;			// report bad connection after 1 second

}	/* end of UDPManagerClass */


/***************************************************************************
 * UDPManagerClass::~UDPManagerClass -- class destructor                   *
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
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
UDPManagerClass::~UDPManagerClass()
{
	int i;

	//------------------------------------------------------------------------
	//	Stop all UDP events
	//------------------------------------------------------------------------
	if (Listening) {
		UDPConnClass::Stop_Listening();
		Listening = 0;
	}

	//------------------------------------------------------------------------
	//	Free all protected-mode memory
	//------------------------------------------------------------------------
	if (GlobalChannel) {
		delete GlobalChannel;
		GlobalChannel = 0;
	}
	for (i = 0; i < NumConnections; i++) {
		delete Connection[i];
		Connection[i] = 0;
	}
	NumConnections = 0;

	//------------------------------------------------------------------------
	//	Free all real-mode memory
	//------------------------------------------------------------------------
	if (RealMemAllocd) {
		Free_RealMode_Mem();
		RealMemAllocd = 0;
	}
}	/* end of ~UDPManagerClass */


/***************************************************************************
 * UDPManagerClass::Init -- initialization routine									*
 *                                                                         *
 * This routine allocates memory, & initializes variables						*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Init()
{
	int i;

	if (!UDPStatus) {
		return(0);
	}

	if (Listening) {
		UDPConnClass::Stop_Listening();
		Listening = 0;
	}

	if (RealMemAllocd) {
		Free_RealMode_Mem();
		RealMemAllocd = 0;
	}

	//------------------------------------------------------------------------
	//	Free protected-mode memory
	//------------------------------------------------------------------------
	if (GlobalChannel) {
		delete GlobalChannel;
		GlobalChannel = 0;
	}
	for (i = 0; i < NumConnections; i++) {
		delete Connection[i];
		Connection[i] = 0;
	}
	NumConnections = 0;

	//------------------------------------------------------------------------
	//	Allocate the Global Channel
	//------------------------------------------------------------------------
	GlobalChannel = new UDPGlobalConnClass (Glb_NumPackets, Glb_NumPackets,
		Glb_MaxPacketLen, ProductID);
	if (!GlobalChannel) {
		return(0);
	}
	GlobalChannel->Init();
	GlobalChannel->Set_Retry_Delta (RetryDelta);
	GlobalChannel->Set_Max_Retries (MaxRetries);
	GlobalChannel->Set_TimeOut (Timeout);

	//------------------------------------------------------------------------
	//	Configure the UDP Connections
	//------------------------------------------------------------------------
	UDPConnClass::Configure(Socket, ConnectionNum, ListenECB, SendECB,
		FirstHeaderBuf, SendHeader, FirstDataBuf, SendBuf, Handler, PacketLen);

	//------------------------------------------------------------------------
	//	Start Listening
	//------------------------------------------------------------------------
	if (!UDPConnClass::Start_Listening()) return(0);
	Listening = 1;

	return(1);

}	/* end of Init */


/***************************************************************************
 * UDPManagerClass::Is_UDP -- tells if UDP is installed or not					*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = UDP is installed; 0 = isn't													*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Is_UDP(void)
{
	return(UDPStatus);

}	/* end of Is_UDP */


/***************************************************************************
 * UDPManagerClass::Set_Timing -- sets timing for all connections				*
 *                                                                         *
 * This will set the timing parameters for all existing connections, and	*
 * all connections created from now on.  This allows an application to		*
 * measure the Response_Time while running, and adjust timing accordingly.	*
 *                                                                         *
 * INPUT:                                                                  *
 *		retrydelta	value to set for retry delta 										*
 *		maxretries	value to set for max # retries									*
 *		timeout		value to set for connection timeout								*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   07/02/1995 BR : Created.                                              *
 *=========================================================================*/
void UDPManagerClass::Set_Timing (uint32_t retrydelta,
	uint32_t maxretries, uint32_t timeout)
{
	int i;

	RetryDelta = retrydelta;
	MaxRetries = maxretries;
	Timeout = timeout;

	if (GlobalChannel) {
		GlobalChannel->Set_Retry_Delta (RetryDelta);
		GlobalChannel->Set_Max_Retries (MaxRetries);
		GlobalChannel->Set_TimeOut (Timeout);
	}

	for (i = 0; i < NumConnections; i++) {
		Connection[i]->Set_Retry_Delta (RetryDelta);
		Connection[i]->Set_Max_Retries (MaxRetries);
		Connection[i]->Set_TimeOut (Timeout);
	}

}	/* end of Set_Timing */


/***************************************************************************
 * UDPManagerClass::Create_Connection -- creates a new connection          *
 *                                                                         *
 * INPUT:                                                                  *
 *		id				application-specific numerical ID for this connection		*
 *		node			ptr to UDPNodeIDType (name & address)							*
 *		address		UDP address for this connection									*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		Never create a connection with an 'id' of -1.								*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Create_Connection(int id, char *name,
	UDPAddressClass *address)
{
	//------------------------------------------------------------------------
	//	Error if UDP not installed
	//------------------------------------------------------------------------
	if (!UDPStatus) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Error if no more room
	//------------------------------------------------------------------------
	if (NumConnections==CONNECT_MAX) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Create new connection
	//------------------------------------------------------------------------
	Connection[NumConnections] = new UDPConnClass(Pvt_NumPackets,
		Pvt_NumPackets, Pvt_MaxPacketLen, ProductID, address, id, name);
	if (!Connection[NumConnections]) {
		return(0);
	}

	Connection[NumConnections]->Init ();
	Connection[NumConnections]->Set_Retry_Delta (RetryDelta);
	Connection[NumConnections]->Set_Max_Retries (MaxRetries);
	Connection[NumConnections]->Set_TimeOut (Timeout);

	NumConnections++;

	return(1);

}	/* end of Create_Connection */


/***************************************************************************
 * UDPManagerClass::Delete_Connection -- deletes a connection              *
 *                                                                         *
 * INPUT:                                                                  *
 *		id		ID of connection to delete													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Delete_Connection(int id)
{
	int i,j;

	//------------------------------------------------------------------------
	//	Error if UDP not installed
	//------------------------------------------------------------------------
	if (!UDPStatus) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Error if no connections to delete
	//------------------------------------------------------------------------
	if (NumConnections==0) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Loop through all connections
	//------------------------------------------------------------------------
	for (i = 0; i < NumConnections; i++) {
		//.....................................................................
		//	If a match, delete it
		//.....................................................................
		if (Connection[i]->ID==id) {
			delete Connection[i];

			//..................................................................
			//	Move array elements back one index
			//..................................................................
			for (j = i; j < NumConnections - 1; j++) {
				Connection[j] = Connection[j+1];
			}

			//..................................................................
			//	Adjust counters
			//..................................................................
			NumConnections--;
			if (CurConnection >= NumConnections)
				CurConnection = 0;
			return(1);
		}
	}

	//------------------------------------------------------------------------
	//	No match; error
	//------------------------------------------------------------------------
	return(0);

}	/* end of Delete_Connection */


/***************************************************************************
 * UDPManagerClass::Num_Connections -- gets the # of connections				*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		# of connections																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Num_Connections(void)
{
	return(NumConnections);

}	/* end of Num_Connections */


/***************************************************************************
 * UDPManagerClass::Connection_ID -- gets the given connection's ID        *
 *                                                                         *
 * INPUT:                                                                  *
 *		index			index of connection to retrieve									*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		ID for that connection, CONNECTION_NONE if invalid index					*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Connection_ID(int index)
{
	if (index >= 0 && index < NumConnections) {
		return(Connection[index]->ID);
	}
	else {
		return(CONNECTION_NONE);
	}
}	/* end of Connection_ID */


/***************************************************************************
 * UDPManagerClass::Connection_Name -- retrieves name for given connection *
 *                                                                         *
 * INPUT:                                                                  *
 *		id		ID of connection to get name of											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		ptr to connection's name, NULL if not found									*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/19/1995 BR : Created.                                              *
 *=========================================================================*/
char *UDPManagerClass::Connection_Name(int id)
{
	int i;

	for (i = 0; i < NumConnections; i++) {
		if (Connection[i]->ID==id) {
			return(Connection[i]->Name);
		}
	}

	return(NULL);

}	/* end of Connection_Name */


/***************************************************************************
 * UDPManagerClass::Connection_Address -- retrieves connection's address	*
 *                                                                         *
 * INPUT:                                                                  *
 *		id		ID of connection to get address for										*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		pointer to IXPAddressClass, NULL if not found								*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/19/1995 BR : Created.                                              *
 *=========================================================================*/
UDPAddressClass * UDPManagerClass::Connection_Address(int id)
{
	int i;

	for (i = 0; i < NumConnections; i++) {
		if (Connection[i]->ID==id) {
			return(&Connection[i]->Address);
		}
	}

	return(NULL);

}	/* end of Connection_Address */


/***************************************************************************
 * UDPManagerClass::Connection_Index -- gets given connection's index      *
 *                                                                         *
 * INPUT:                                                                  *
 *		ID to retrieve index for															*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		index for this connection, CONNECTION_NONE if not found					*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Connection_Index(int id)
{
	int i;

	for (i = 0; i < NumConnections; i++) {
		if (Connection[i]->ID==id) {
			return(i);
		}
	}

	return(CONNECTION_NONE);

}	/* end of Connection_Index */


/***************************************************************************
 * UDPManagerClass::Set_Connection_Parms -- sets connection's name & id		*
 *                                                                         *
 * INPUT:                                                                  *
 *		index		connection index															*
 *		id			new connection ID															*
 *		name		new connection name														*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
void UDPManagerClass::Set_Connection_Parms(int index, int id, char *name)
{
	if (index >= NumConnections)
		return;

	Connection[index]->ID = id;
	strcpy(Connection[index]->Name,name);

}	/* end of Set_Connection_Parms */


/***************************************************************************
 * UDPManagerClass::Send_Global_Message -- sends a Global Message				*
 *                                                                         *
 * INPUT:                                                                  *
 *		buf			buffer to send															*
 *		buflen		length of buf															*
 *		ack_req		1 = ACK required; 0 = no ACK required							*
 *		address		address to send to; NULL = broadcast							*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Send_Global_Message(void *buf, int buflen,
	int ack_req, UDPAddressClass *address)
{
	int rc;
	if (Winsock.Uses_Reliable_Transport()) {
		ack_req = 0;
	}

	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening) return(0);

	rc = GlobalChannel->Send_Packet (buf, buflen, address, ack_req);
	if (!rc) {
		SendOverflows++;
	}

	return(rc);

}	/* end of Send_Global_Message */


/***************************************************************************
 * UDPManagerClass::Get_Global_Message -- polls the Global Message queue	*
 *                                                                         *
 * INPUT:                                                                  *
 *		buf				buffer to store received packet								*
 *		buflen			length of data placed in 'buf'								*
 *		address			UDP address of sender											*
 *		product_id		product ID of sender												*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Get_Global_Message(void *buf, int *buflen,
	UDPAddressClass *address, uint16_t *product_id)
{
	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening) return(0);

	return(GlobalChannel->Get_Packet (buf, buflen, address, product_id));

}	/* end of Get_Global_Message */


/***************************************************************************
 * UDPManagerClass::Send_Private_Message -- Sends a Private Message			*
 *                                                                         *
 * INPUT:                                                                  *
 *		buf			buffer to send															*
 *		buflen		length of 'buf'														*
 *		conn_id		connection ID to send to (CONNECTION_NONE = all)			*
 *		ack_req		1 = ACK required; 0 = no ACK required							*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Send_Private_Message(void *buf, int buflen, int ack_req,
	int conn_id)
{
	int i;						// loop counter
	int connect_idx;			// index of channel to send to, if specified
	if (Winsock.Uses_Reliable_Transport()) {
		ack_req = 0;
	}

	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------

	if (!UDPStatus || !Listening || (NumConnections==0)) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Send the message to all connections
	//------------------------------------------------------------------------
	if (conn_id==CONNECTION_NONE) {
		//.....................................................................
		//	Check for room in all connections
		//.....................................................................
		for (i = 0; i < NumConnections; i++) {
			if (Connection[i]->Queue->Num_Send() ==
				Connection[i]->Queue->Max_Send()) {
				SendOverflows++;
				return(0);
			}
		}

		//.....................................................................
		//	Send packet to all connections
		//.....................................................................
		for (i = 0; i < NumConnections; i++) {
			Connection[i]->Send_Packet (buf, buflen, ack_req);
		}
		return(1);
	}

	//------------------------------------------------------------------------
	//	Send the message to the specified connection
	//------------------------------------------------------------------------
	else {
		connect_idx = Connection_Index (conn_id);
		if (connect_idx == CONNECTION_NONE) {
			SendOverflows++;
			return(0);
		}

		//.....................................................................
		//	Check for room in the connection
		//.....................................................................
		if (Connection[connect_idx]->Queue->Num_Send() ==
			Connection[connect_idx]->Queue->Max_Send()) {
			SendOverflows++;
			return(0);
		}

		//.....................................................................
		//	Send the packet to that connection
		//.....................................................................
		Connection[connect_idx]->Send_Packet (buf, buflen, ack_req);
		return(1);
	}

}	/* end of Send_Private_Message */


/***************************************************************************
 * UDPManagerClass::Get_Private_Message -- Polls the Private Message queue	*
 *                                                                         *
 * INPUT:                                                                  *
 *		buf			buffer to store incoming packet									*
 *		buflen		length of data placed in 'buf'									*
 *		conn_id		filled in with connection ID of sender							*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Get_Private_Message(void *buf, int *buflen, int *conn_id)
{
	int i;
	int rc;
	int c_id;

	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening || (NumConnections==0)) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	Safety check: ensure CurConnection is in range.
	//------------------------------------------------------------------------
	if (CurConnection >= NumConnections) {
		CurConnection = 0;
	}

	//------------------------------------------------------------------------
	//	Scan all connections for a received packet, starting with 'CurConnection'
	//------------------------------------------------------------------------
	for (i = 0; i < NumConnections; i++) {

		//.....................................................................
		//	Check this connection for a packet
		//.....................................................................
		rc = Connection[CurConnection]->Get_Packet (buf, buflen);
		c_id = Connection[CurConnection]->ID;

		//.....................................................................
		//	Increment CurConnection to the next connection index
		//.....................................................................
		CurConnection++;
		if (CurConnection >= NumConnections) {
			CurConnection = 0;
		}

		//.....................................................................
		//	If we got a packet, return the connection ID
		//.....................................................................
		if (rc) {
			(*conn_id) = c_id;
			return(1);
		}
	}

	return(0);

}	/* end of Get_Private_Message */


/***************************************************************************
 * UDPManagerClass::Service -- main polling routine for UDP Connections		*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Service(void)
{
	int rc = 1;
	int i;
	CommHeaderType *packet;
	int packetlen;
	UDPAddressClass address;


	uint8_t temp_receive_buffer[1024];
	int	recv_length;

	if (Winsock.Using_TCP()) {
		Winsock.Service();
		if (Winsock.Get_Connection_Status() == TcpipManagerClass::CONNECTION_LOST) {
			BadConnection = (NumConnections > 0) ? Connection[0]->ID : CONNECTION_NONE;
			return(0);
		}
	}

	if (Winsock.Get_Connected()) {

		if (!Winsock.Get_Connected()) return (0);

		/*
		** This is an internet connection so get the packets from winsock
		*/
		while ((recv_length = Winsock.Read(temp_receive_buffer, 1024))!=0) {

			CurHeaderBuf = NULL;
			CurDataBuf = (char*)&temp_receive_buffer[0];

			/*.....................................................................
			Compute the length of the packet (byte-swap the length in the UDP hdr)
			.....................................................................*/
			packetlen = recv_length;

			/*.....................................................................
			Extract the sender's address from the UDP header
			.....................................................................*/
			Winsock.Get_Remote_Address(&address);

			/*.....................................................................
			Examine the Magic Number of the received packet to determine if this
			packet goes into the Global Queue, or into one of the Private Queues
			.....................................................................*/
			packet = (CommHeaderType *)CurDataBuf;
			if (packet->MagicNumber == GlobalChannel->Magic_Num()) {
				/*..................................................................
				Put the packet in the Global Queue
				..................................................................*/
				if (!GlobalChannel->Receive_Packet (packet, packetlen, &address))
					ReceiveOverflows++;
			} else {
				if (packet->MagicNumber == ProductID) {
					/*..................................................................
					Find the Private Queue that this packet is for
					..................................................................*/
					for (i = 0; i < NumConnections; i++) {
						if (Connection[i]->Address == address) {
							if (!Connection[i]->Receive_Packet (packet, packetlen))
								ReceiveOverflows++;
							break;
						}
					}
				}
			}

		}
	} else {
		while ((packetlen = UDPConnClass::Receive((char*)&temp_receive_buffer[0], sizeof(temp_receive_buffer), &address)) != 0) {
			CurHeaderBuf = NULL;
			CurDataBuf = (char*)&temp_receive_buffer[0];

			packet = (CommHeaderType *)CurDataBuf;
			if (packet->MagicNumber == GlobalChannel->Magic_Num()) {
				if (!GlobalChannel->Receive_Packet(packet, packetlen, &address))
					ReceiveOverflows++;
			} else {
				if (packet->MagicNumber == ProductID) {
					for (i = 0; i < NumConnections; i++) {
						if (Connection[i]->Address == address) {
							if (!Connection[i]->Receive_Packet(packet, packetlen))
								ReceiveOverflows++;
							break;
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------
	//	Service all connections.  If a connection reports that it's gone "bad",
	//	report an error to the caller.  If it's the Global Channel, un-queue the
	//	send entry that's holding things up.  This will keep the Global Channel
	//	from being clogged by one un-ACK'd outgoing packet.
	//------------------------------------------------------------------------
	if (GlobalChannel) {
		if (!GlobalChannel->Service()) {
			GlobalChannel->Queue->UnQueue_Send (NULL, NULL, 0);
			rc = 0;
		}
	}
	for (i = 0; i < NumConnections; i++) {
		if (!Connection[i]->Service()) {
			rc = 0;
			BadConnection = Connection[i]->ID;
		}
	}

	if (rc) {
		BadConnection = CONNECTION_NONE;
	}

	return(rc);

}	/* end of Service */

/***************************************************************************
 * UDPManagerClass::Get_Bad_Connection -- returns bad connection ID        *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		ID of bad connection; CONNECTION_NONE if none.								*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/04/1995 BRR : Created.                                             *
 *=========================================================================*/
int UDPManagerClass::Get_Bad_Connection(void)
{
	return(BadConnection);

}	/* end of Get_Bad_Connection */


/***************************************************************************
 * UDPManagerClass::Global_Num_Send	-- reports # entries in send queue		*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		# entries in the Global Send Queue												*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Global_Num_Send(void)
{
	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening) {
		return(0);
	}

	return(GlobalChannel->Queue->Num_Send());

}	/* end of Global_Num_Send */


/***************************************************************************
 * UDPManagerClass::Global_Num_Receive -- reports # entries in recv queue	*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		# entries in the Global Receive Queue											*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Global_Num_Receive(void)
{
	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening) {
		return(0);
	}

	return(GlobalChannel->Queue->Num_Receive());

}	/* end of Global_Num_Receive */


/***************************************************************************
 * UDPManagerClass::Private_Num_Send -- reports # entries in send queue		*
 *                                                                         *
 * INPUT:                                                                  *
 *		# entries in the Private Send Queue												*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Private_Num_Send(int id)
{
	int i;
	int maxnum;

	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening || (NumConnections==0)) {
		return(0);
	}

	//------------------------------------------------------------------------
	//	If connection ID specified, return that connection's # of packets
	//------------------------------------------------------------------------
	if (id != CONNECTION_NONE) {
		i = Connection_Index(id);
		if (i != CONNECTION_NONE) {
			return(Connection[i]->Queue->Num_Send());
		}
		else {
			return(0);
		}

	}

	//------------------------------------------------------------------------
	//	Otherwise, return the max # of all connections
	//------------------------------------------------------------------------
	else {
		maxnum = 0;
		for (i = 0; i < NumConnections; i++) {
			if (Connection[i]->Queue->Num_Send() > maxnum) {
				maxnum = Connection[i]->Queue->Num_Send();
			}
		}
		return(maxnum);
	}

}	/* end of Private_Num_Send */


/***************************************************************************
 * UDPManagerClass::Private_Num_Receive -- reports # entries in recv queue	*
 *                                                                         *
 * INPUT:                                                                  *
 *		id		ID of connection to query													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		# entries in the Private Receive Queue											*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Private_Num_Receive(int id)
{
	int i;
	int maxnum;

	//------------------------------------------------------------------------
	//	Error if UDP not installed or not Listening
	//------------------------------------------------------------------------
	if (!UDPStatus || !Listening || (NumConnections==0))
		return(0);

	//------------------------------------------------------------------------
	//	If connection ID specified, return that connection's # of packets
	//------------------------------------------------------------------------
	if (id != CONNECTION_NONE) {
		i = Connection_Index(id);
		if (i != CONNECTION_NONE) {
			return(Connection[i]->Queue->Num_Receive());
		}
		else {
			return(0);
		}

	}

	//------------------------------------------------------------------------
	//	Otherwise, return the max # of all connections
	//------------------------------------------------------------------------
	else {
		maxnum = 0;
		for (i = 0; i < NumConnections; i++) {
			if (Connection[i]->Queue->Num_Receive() > maxnum) {
				maxnum = Connection[i]->Queue->Num_Receive();
			}
		}
		return(maxnum);
	}

}	/* end of Private_Num_Receive */


/***************************************************************************
 * UDPManagerClass::Set_Socket -- sets socket ID for all connections			*
 *                                                                         *
 * INPUT:                                                                  *
 *		socket	new socket ID to use														*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		Do not call this function after communications have started; you		*
 *		must call it before calling Init().												*
 *		The socket number is byte-swapped, since UDP requires socket ID's		*
 *		to be stored high/low.																*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
void UDPManagerClass::Set_Socket(uint16_t socket)
{
	Socket = socket;

}	/* end of Set_Socket */


/***************************************************************************
 * UDPManagerClass::Response_Time -- Returns largest Avg Response Time     *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		largest avg response time															*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/04/1995 BRR : Created.                                             *
 *=========================================================================*/
uint32_t UDPManagerClass::Response_Time(void)
{
	uint32_t resp;
	uint32_t maxresp = 0;
	int i;

	for (i = 0; i < NumConnections; i++) {
		resp = Connection[i]->Queue->Avg_Response_Time();
		if (resp > maxresp) {
			maxresp = resp;
		}
	}

	return(maxresp);

}	/* end of Response_Time */


/***************************************************************************
 * UDPManagerClass::Global_Response_Time -- Returns Avg Response Time      *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		avg global channel response time													*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/04/1995 BRR : Created.                                             *
 *=========================================================================*/
uint32_t UDPManagerClass::Global_Response_Time(void)
{
	if (GlobalChannel) {
		return (GlobalChannel->Queue->Avg_Response_Time());
	}
	else {
		return (0);
	}

}	/* end of Global_Response_Time */


/***************************************************************************
 * UDPManagerClass::Reset_Response_Time -- Reset response time					*
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
 *   05/04/1995 BRR : Created.                                             *
 *=========================================================================*/
void UDPManagerClass::Reset_Response_Time(void)
{
	int i;

	for (i = 0; i < NumConnections; i++) {
		Connection[i]->Queue->Reset_Response_Time();
	}

	if (GlobalChannel)
		GlobalChannel->Queue->Reset_Response_Time();

}	/* end of Reset_Response_Time */


void UDPManagerClass::Reset_Global_Response_Time(void)
{
	if (GlobalChannel) {
		GlobalChannel->Queue->Reset_Response_Time();
	}
}


/***************************************************************************
 * UDPManagerClass::Oldest_Send -- gets ptr to oldest send buf             *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		buf ptr																					*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/04/1995 BRR : Created.                                             *
 *=========================================================================*/
void * UDPManagerClass::Oldest_Send(void)
{
	int i,j;
	uint32_t time;
	uint32_t mintime = 0xffffffff;
	SendQueueType *send_entry;				// ptr to send entry header
	CommHeaderType *packet;
	void *buf = NULL;

	for (i = 0; i < NumConnections; i++) {

		send_entry = NULL;

		for (j = 0; j < Connection[i]->Queue->Num_Send(); j++) {
			send_entry = Connection[i]->Queue->Get_Send(j);
			if (send_entry) {
				packet = (CommHeaderType *)send_entry->Buffer;
				if (packet->Code == ConnectionClass::PACKET_DATA_ACK &&
					send_entry->IsACK == 0) {
					break;
				}
				else {
					send_entry = NULL;
				}
			}
		}

		if (send_entry!=NULL) {

			time = send_entry->FirstTime;

			if (time < mintime) {
				mintime = time;
				buf = send_entry->Buffer;
			}
		}
	}

	return(buf);

}	/* end of Oldest_Send */


/***************************************************************************
 * UDPManagerClass::Set_Bridge -- prepares to cross a bridge               *
 *                                                                         *
 * This routine is designed to prevent the connection from having to			*
 * call Get_Local_Target, except the minimum number of times, since that	*
 * routine is buggy & goes away for long periods sometimes.						*
 *                                                                         *
 * INPUT:                                                                  *
 *		bridge		network number of the destination bridge						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   07/06/1995 BRR : Created.                                             *
 *=========================================================================*/
void UDPManagerClass::Set_Bridge(NetNumType bridge)
{
	if (GlobalChannel) {
		GlobalChannel->Set_Bridge(bridge);
	}

}	/* end of Set_Bridge */


/***************************************************************************
 * UDPManagerClass::Configure_Debug -- sets up special debug values        *
 *                                                                         *
 * Mono_Debug_Print2() can look into a packet to pull out a particular		*
 * ID, and can print both that ID and a string corresponding to				*
 * that ID.  This routine configures these values so it can find				*
 * and decode the ID.  This ID is used in addition to the normal				*
 * CommHeaderType values.																	*
 *                                                                         *
 * INPUT:                                                                  *
 *		index				connection index to configure (-1 = Global Channel)	*
 *		type_offset		ID's byte offset into packet									*
 *		type_size		size of ID, in bytes; 0 if none								*
 *		names				ptr to array of names; use ID as an index into this	*
 *		namestart		numerical value of 1st name in the array					*
 *		namecount		# in the names array; 0 if none.								*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		Names shouldn't be longer than 12 characters.								*
 *                                                                         *
 * HISTORY:                                                                *
 *   05/31/1995 BRR : Created.                                             *
 *=========================================================================*/
void UDPManagerClass::Configure_Debug(int index, int type_offset,
	int type_size, char const **names, int namestart, int namecount)
{
	if (index == -1) {
		GlobalChannel->Queue->Configure_Debug (type_offset, type_size, names,
			namestart, namecount);
	}
	else if (Connection[index]) {
		Connection[index]->Queue->Configure_Debug (type_offset, type_size, names,
			namestart, namecount);
	}

}	/* end of Configure_Debug */


/***************************************************************************
 * UDPManagerClass::Mono_Debug_Print -- debug output routine					*
 *                                                                         *
 * INPUT:                                                                  *
 *		index			index of connection to display (-1 = Global Channel)		*
 *		refresh		1 = complete screen refresh										*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   01/25/1995 BR : Created.                                              *
 *=========================================================================*/
void UDPManagerClass::Mono_Debug_Print(int index, int refresh)
{
#ifdef WWLIB32_H
	char txt[80];
	int i;

	if (index == -1)
		GlobalChannel->Queue->Mono_Debug_Print (refresh);

	else if (Connection[index])
		Connection[index]->Queue->Mono_Debug_Print (refresh);

	if (refresh) {
		Mono_Set_Cursor (20,1);
		Mono_Printf ("UDP Queue:");

		Mono_Set_Cursor (9,2);
		Mono_Printf ("Average Response Time:");

		Mono_Set_Cursor (43,1);
		Mono_Printf ("Send Overflows:");

		Mono_Set_Cursor (40,2);
		Mono_Printf ("Receive Overflows:");

	}

	Mono_Set_Cursor (32,1);
	Mono_Printf ("%d",index);

	Mono_Set_Cursor (32,2);
	if (index == -1) {
		Mono_Printf ("%d  ", GlobalChannel->Queue->Avg_Response_Time());
	}
	else {
		Mono_Printf ("%d  ", Connection[index]->Queue->Avg_Response_Time());
	}

	Mono_Set_Cursor (59,1);
	Mono_Printf ("%d  ", SendOverflows);

	Mono_Set_Cursor (59,2);
	Mono_Printf ("%d  ", ReceiveOverflows);

	for (i = 0; i < NumBufs; i++) {
		if (BufferFlags[i]) {
			txt[i] = 'X';
		}
		else {
			txt[i] = '_';
		}
	}
	txt[i] = 0;
	Mono_Set_Cursor ((80-NumBufs)/2,3);
	Mono_Printf ("%s",txt);

#else
	index = index;
	refresh = refresh;
#endif

}	/* end of Mono_Debug_Print */


/***************************************************************************
 * UDPManagerClass::Alloc_RealMode_Mem -- allocates real-mode memory			*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Alloc_RealMode_Mem(void)
{
	return (1);
}	/* end of Alloc_Realmode_Mem */


/***************************************************************************
 * UDPManagerClass::Free_RealMode_Mem -- frees real-mode memory				*
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		1 = OK, 0 = error																		*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/20/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPManagerClass::Free_RealMode_Mem(void)
{
	return (1);
}	/* end of Free_Realmode_Mem */

/*************************** end of udpmgr.cpp *****************************/
