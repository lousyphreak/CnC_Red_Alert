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

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S        **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : IPX95PP                                  *
 *                                                                         *
 *                   Programmer : Steve Tall                               *
 *                                                                         *
 *                   Start Date : January 22nd, 1996                       *
 *                                                                         *
 *                  Last Update : January 22nd, 1996   [ST]                *
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



/*
** Types for function pointers
*/
#include <stdint.h>
typedef int32_t  (*IPXInitialiseType) (void);
typedef int32_t  (*IPXGetOutstandingBuffer95Type) (uint8_t*);
typedef void  (*IPXShutDown95Type) (void);
typedef int   (*IPXSendPacket95Type) (uint8_t *, uint8_t *, int, uint8_t*, uint8_t*);
typedef int   (*IPXBroadcastPacket95Type) (uint8_t *, int);
typedef int32_t  (*IPXStartListening95Type) (void);
typedef int   (*IPXOpenSocket95Type) (int);
typedef void  (*IPXCloseSocket95Type) (int);
typedef int   (*IPXGetConnectionNumber95Type) (void);
typedef int   (*IPXGetLocalTarget95) (uint8_t *, uint8_t*, uint16_t, uint8_t*);



/*
** Function pointers
*/
//extern "C"{
	extern IPXInitialiseType					IPX_Initialise;
	extern IPXGetOutstandingBuffer95Type  	IPX_Get_Outstanding_Buffer95;
	extern IPXShutDown95Type              	IPX_Shut_Down95;
	extern IPXSendPacket95Type            	IPX_Send_Packet95;
	extern IPXBroadcastPacket95Type       	IPX_Broadcast_Packet95;
	extern IPXStartListening95Type        	IPX_Start_Listening95;
	extern IPXOpenSocket95Type            	IPX_Open_Socket95;
	extern IPXCloseSocket95Type           	IPX_Close_Socket95;
	extern IPXGetConnectionNumber95Type   	IPX_Get_Connection_Number95;
	extern IPXGetLocalTarget95            	IPX_Get_Local_Target95;
//}

/*
** Functions
*/
bool Load_IPX_Dll (void);
void Unload_IPX_Dll (void);


#if (0)
extern "C"{
	extern int32_t  IPX_Initialise(void);
	extern int32_t  IPX_Get_Outstanding_Buffer95(uint8_t *buffer);
	extern void  IPX_Shut_Down95(void);
	extern int   IPX_Send_Packet95(uint8_t *, uint8_t *, int, uint8_t*, uint8_t*);
	extern int   IPX_Broadcast_Packet95(uint8_t *, int);
	extern int32_t  IPX_Start_Listening95(void);
	extern int   IPX_Open_Socket95(int socket);
	extern void  IPX_Close_Socket95(int socket);
	extern int   IPX_Get_Connection_Number95(void);
	extern int   IPX_Get_Local_Target95(uint8_t *, uint8_t*, uint16_t, uint8_t*);
}
#endif	//(0)
