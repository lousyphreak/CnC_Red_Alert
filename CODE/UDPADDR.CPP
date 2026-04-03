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

/* $Header: /CounterStrike/UDPADDR.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : UDPADDR.CPP                              *
 *                                                                         *
 *                   Programmer : Bill Randolph                            *
 *                                                                         *
 *                   Start Date : December 19, 1994                        *
 *                                                                         *
 *                  Last Update : December 19, 1994   [BR]                 *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   UDPAddressClass::UDPAddressClass -- class constructor                 *
 *   UDPAddressClass::UDPAddressClass -- class constructor form 2				*
 *   UDPAddressClass::UDPAddressClass -- class constructor form 3				*
 *   UDPAddressClass::Set_Address -- sets the UDP address values        	*
 *   UDPAddressClass::Set_Address -- sets the UDP values from a header		*
 *   UDPAddressClass::Get_Address -- retrieves the UDP address values      *
 *   UDPAddressClass::Get_Address -- copies address into an UDP header		*
 *   UDPAddressClass::Is_Broadcast -- tells if this is a broadcast address *
 *   UDPAddressClass::operator== -- overloaded comparison operator         *
 *   UDPAddressClass::operator!= -- overloaded comparison operator         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "FUNCTION.H"
#include <stdio.h>
#include <string.h>
#include "UDPADDR.H"
#ifdef NETWORK_UDP
#include	"WSPROTO.H"
#endif	//NETWORK_UDP


/***************************************************************************
 * UDPAddressClass::UDPAddressClass -- class constructor                   *
 *                                                                         *
 * This default constructor generates a broadcast address.						*
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
UDPAddressClass::UDPAddressClass(void)
{
	NetworkNumber[0] = 0xff;
	NetworkNumber[1] = 0xff;
	NetworkNumber[2] = 0xff;
	NetworkNumber[3] = 0xff;
	NodeAddress[0] = 0xff;
	NodeAddress[1] = 0xff;
	NodeAddress[2] = 0xff;
	NodeAddress[3] = 0xff;
	NodeAddress[4] = 0xff;
	NodeAddress[5] = 0xff;

}	/* end of UDPAddressClass */


/***************************************************************************
 * UDPAddressClass::UDPAddressClass -- class constructor form 2				*
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
UDPAddressClass::UDPAddressClass(NetNumType net, NetNodeType node)
{
	memcpy(NetworkNumber, net, 4);
	memcpy(NodeAddress, node, 6);

}	/* end of UDPAddressClass */


/***************************************************************************
 * UDPAddressClass::UDPAddressClass -- class constructor form 3				*
 *                                                                         *
 * This form of the constructor takes an UDP header as an argument.  It		*
 * extracts the address from the Source address fields in the header.		*
 *                                                                         *
 * INPUT:                                                                  *
 *		header	Header from which to extract the address							*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
UDPAddressClass::UDPAddressClass(UDPHeaderType *header)
{
	memcpy(NetworkNumber,header->SourceNetworkNumber,4);
	memcpy(NodeAddress,header->SourceNetworkNode,6);

}	/* end of UDPAddressClass */


/***************************************************************************
 * UDPAddressClass::Set_Address -- sets the UDP address values        		*
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
void UDPAddressClass::Set_Address(NetNumType net, NetNodeType node)
{
	memcpy(NetworkNumber,net,4);
	memcpy(NodeAddress,node,6);

}	/* end of Set_Address */


/***************************************************************************
 * UDPAddressClass::Set_Address -- sets the UDP values from a header			*
 *                                                                         *
 * This routine extracts the source addresses from the given UDP header.	*
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
void UDPAddressClass::Set_Address(UDPHeaderType *header)
{
#ifdef NETWORK_UDP
	if (PacketTransport && PacketTransport->Get_Protocol() == PROTOCOL_UDP) {
		uint8_t *addr = (uint8_t*) header;
		memset (NodeAddress, 0, 6);
		memcpy (NodeAddress, addr, 4);
		memset (NetworkNumber, 0, 4);
	} else {
		memcpy(NetworkNumber,header->SourceNetworkNumber,4);
		memcpy(NodeAddress,header->SourceNetworkNode,6);
	}
#else	//NETWORK_UDP

	if (header) {
		memcpy(NetworkNumber,header->SourceNetworkNumber,4);
		memcpy(NodeAddress,header->SourceNetworkNode,6);
	} else {
		/*
		** Address is meaningless when using winsock
		*/
		memset(NetworkNumber, 1 ,4);
		memset(NodeAddress, 1, 6);
	}

#endif	//NETWORK_UDP

}	/* end of Set_Address */


/***************************************************************************
 * UDPAddressClass::Get_Address -- retrieves the UDP address values        *
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
void UDPAddressClass::Get_Address(NetNumType net, NetNodeType node)
{
	memcpy(net,NetworkNumber,4);
	memcpy(node,NodeAddress,6);

}	/* end of Get_Address */


/***************************************************************************
 * UDPAddressClass::Get_Address -- copies address into an UDP header			*
 *                                                                         *
 * INPUT:                                                                  *
 *		net		Network Number for this address										*
 *		node		Node Address for this address											*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
void UDPAddressClass::Get_Address(UDPHeaderType *header)
{
	memcpy(header->DestNetworkNumber,NetworkNumber,4);
	memcpy(header->DestNetworkNode,NodeAddress,6);

}	/* end of Get_Address */


/***************************************************************************
 * UDPAddressClass::Is_Broadcast -- tells if this is a broadcast address   *
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
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::Is_Broadcast(void)
{
	if (	NetworkNumber[0] == 0xff &&
			NetworkNumber[1] == 0xff &&
			NetworkNumber[2] == 0xff &&
			NetworkNumber[3] == 0xff &&
			NodeAddress[0] == 0xff &&
			NodeAddress[1] == 0xff &&
			NodeAddress[2] == 0xff &&
			NodeAddress[3] == 0xff &&
			NodeAddress[4] == 0xff &&
			NodeAddress[5] == 0xff) {
		return(1);
	}
	else {
		return(0);
	}

}	/* end of Is_Broadcast */


/***************************************************************************
 * UDPAddressClass::operator== -- overloaded comparison operator           *
 *                                                                         *
 * Since, if NETX isn't running, the network number on a received packet	*
 * can be bogus (all 0's), only the node address is used for comparison 	*
 * purposes here.																				*
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = not equal, 1 = equal															*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator == (UDPAddressClass & addr)
{
	//------------------------------------------------------------------------
	//	If either Network Number is all 0's (which can happen if the system is
	//	not running NETX), compare only the Node Addresses.
	//------------------------------------------------------------------------
	if ( (NetworkNumber[0]==0 &&
			NetworkNumber[1]==0 &&
			NetworkNumber[2]==0 &&
			NetworkNumber[3]==0) ||
		  (addr.NetworkNumber[0]==0 &&
			addr.NetworkNumber[1]==0 &&
			addr.NetworkNumber[2]==0 &&
			addr.NetworkNumber[3]==0) ) {

		if (memcmp(NodeAddress,addr.NodeAddress,6)==0) {
			return(1);
		}
		else {
			return(0);
		}

	}
	//------------------------------------------------------------------------
	//	Otherwise, compare both the Network Numbers and Node Addresses
	//------------------------------------------------------------------------
	else {
		if (memcmp(NodeAddress,addr.NodeAddress,6)==0 &&
			memcmp(NetworkNumber,addr.NetworkNumber,4)==0) {
			return(1);
		}
		else {
			return(0);
		}
	}

}	/* end of operator== */


/***************************************************************************
 * UDPAddressClass::operator!= -- overloaded comparison operator           *
 *                                                                         *
 * Since, if NETX isn't running, the network number on a received packet	*
 * can be bogus (all 0's), only the node address is used for comparison 	*
 * purposes here.																				*
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		0 = equal, 1 = not equal															*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator != (UDPAddressClass & addr)
{
	//------------------------------------------------------------------------
	//	If either Network Number is all 0's (which can happen if the system is
	//	not running NETX), compare only the Node Addresses.
	//------------------------------------------------------------------------
	if ( (NetworkNumber[0]==0 &&
			NetworkNumber[1]==0 &&
			NetworkNumber[2]==0 &&
			NetworkNumber[3]==0) ||
		  (addr.NetworkNumber[0]==0 &&
			addr.NetworkNumber[1]==0 &&
			addr.NetworkNumber[2]==0 &&
			addr.NetworkNumber[3]==0) ) {

		if (memcmp(NodeAddress,addr.NodeAddress,6)==0) {
			return(0);
		}
		else {
			return(1);
		}
	}
	//------------------------------------------------------------------------
	//	Otherwise, compare both the Network Numbers and Node Addresses
	//------------------------------------------------------------------------
	else {
		if (memcmp(NodeAddress,addr.NodeAddress,6)==0 &&
			memcmp(NetworkNumber,addr.NetworkNumber,4)==0) {
			return(0);
		}
		else {
			return(1);
		}
	}

}	/* end of operator!= */


/***************************************************************************
 * UDPAddressClass::operator > -- overloaded comparison operator           *
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		TRUE = greater, FALSE = not														*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator > (UDPAddressClass & addr)
{
	return(memcmp(this, &addr, 10) > 0);

}	/* end of operator> */


/***************************************************************************
 * UDPAddressClass::operator < -- overloaded comparison operator           *
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		TRUE = less, FALSE = not															*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator < (UDPAddressClass & addr)
{
	return(memcmp(this, &addr, 10) < 0);

}	/* end of operator< */


/***************************************************************************
 * UDPAddressClass::operator >= -- overloaded comparison operator          *
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		TRUE = greater or equal, FALSE = not											*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator >= (UDPAddressClass & addr)
{
	return(memcmp(this, &addr, 10) >= 0);

}	/* end of operator>= */


/***************************************************************************
 * UDPAddressClass::operator <= -- overloaded comparison operator          *
 *                                                                         *
 * INPUT:                                                                  *
 *		addr		address to compare to													*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		TRUE = less or equal, FALSE = not												*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/19/1994 BR : Created.                                              *
 *=========================================================================*/
int UDPAddressClass::operator <= (UDPAddressClass & addr)
{
	return(memcmp(this, &addr, 10) <= 0);

}	/* end of operator<= */

/************************** end of udpaddr.cpp *****************************/
