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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                     $Archive:: /Sun/Internet.h                                             $*
 *                                                                                             *
 *                      $Author:: Joe_b                                                       $*
 *                                                                                             *
 *                     $Modtime:: 8/05/97 6:45p                                               $*
 *                                                                                             *
 *                    $Revision:: 7                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef INTERNET_H
#define INTERNET_H
#include <stdint.h>

#define IP_ADDRESS_MAX 40

typedef enum {
	INTERNET_CMDLINE_NONE,
	INTERNET_CMDLINE_HOST,
	INTERNET_CMDLINE_CONNECT,
} InternetCommandLineType;

void	Register_Game_Start_Time(void);
void	Register_Game_End_Time(void);
void	Send_Statistics_Packet(void);
bool	Internet_Parse_Port(char const* text, uint16_t* port);
void	Internet_Clear_Command_Line_Connect(void);
void	Internet_Clear_Command_Line_Name(void);
void	Internet_Set_Command_Line_Host(uint16_t port);
void	Internet_Set_Command_Line_Connect(char const* address, uint16_t port);
void	Internet_Set_Command_Line_Name(char const* name);
bool	Internet_Has_Command_Line_Name(void);
void	Internet_Apply_Command_Line_Name(void);
bool	Internet_Has_Command_Line_Connect(void);
bool	Internet_Command_Line_Is_Host(void);
bool 	Internet_Remote_Connect(void);
bool 	Server_Remote_Connect(void);
bool 	Client_Remote_Connect(void);

extern char PlanetWestwoodIPAddress[IP_ADDRESS_MAX];
extern uint16_t PlanetWestwoodPortNumber;
extern bool PlanetWestwoodIsHost;
extern InternetCommandLineType PlanetWestwoodCommandLine;

#endif
