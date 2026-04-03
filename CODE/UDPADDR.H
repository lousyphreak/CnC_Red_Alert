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

/* $Header: /CounterStrike/UDPADDR.H 1     3/03/97 10:24a Joe_bostic $ */
/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : UDPADDR.H                                *
 *                                                                         *
 *                   Programmer : Bill Randolph                            *
 *                                                                         *
 *                   Start Date : December 19, 1994                        *
 *                                                                         *
 *                  Last Update : December 19, 1994   [BR]                 *
 *                                                                         *
 * This class is useful for any UDP-related code.  It's just a utility		*
 * to help manage those annoying UDP address fields.  This class lets you	*
 * compare addresses, copy addresses to & from the UDP header, etc.			*
 *                                                                         *
 * The class has no virtual functions, so you can treat this class just		*
 * like a data structure; it can be loaded & saved, and even transmitted	*
 * across the net.																			*
 *                                                                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef UDPADDR_H
#define UDPADDR_H

#include "UDP.H"
// for NetNumType & NetNodeType

/*
***************************** Class Declaration *****************************
*/
class UDPAddressClass
{
	/*
	---------------------------- Public Interface ----------------------------
	*/
	public:
		/*.....................................................................
		Constructors:
		.....................................................................*/
		UDPAddressClass(void);
		UDPAddressClass(NetNumType net, NetNodeType node);
		UDPAddressClass(UDPHeaderType *header);

		/*.....................................................................
		Set the address from explicit variables, or from the SOURCE values
		in an UDP packet header.
		.....................................................................*/
		void Set_Address(NetNumType net, NetNodeType node);
		void Set_Address(UDPHeaderType *header);
		/*.....................................................................
		Get the address values explicitly, or copy them into the DESTINATION
		values in an UDP packet header.
		.....................................................................*/
		void Get_Address (NetNumType net, NetNodeType node);
		void Get_Address(UDPHeaderType *header);

		/*.....................................................................
		Tells if this address is a broadcast address
		.....................................................................*/
		int Is_Broadcast(void);

		/*.....................................................................
		Overloaded operators:
		.....................................................................*/
		int operator == (UDPAddressClass & addr);
		int operator != (UDPAddressClass & addr);
		int operator > (UDPAddressClass &addr);
		int operator < (UDPAddressClass &addr);
		int operator >= (UDPAddressClass &addr);
		int operator <= (UDPAddressClass &addr);
	/*
	-------------------------- Protected Interface ---------------------------
	*/
	protected:
	/*
	--------------------------- Private Interface ----------------------------
	*/
	private:
		NetNumType NetworkNumber;
		NetNodeType NodeAddress;
};

#endif

/**************************** end of udpaddr.h *****************************/
