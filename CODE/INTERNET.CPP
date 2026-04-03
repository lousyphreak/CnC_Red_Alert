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

/* $Header: /counterstrike/INTERNET.CPP 6     3/17/97 1:05a Steve_tall $ */
/*************************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D    S T U D I O S                  **
 *************************************************************************************
 *                                                                                   *
 *                 Project Name : Command & Conquer - Red Alert                      *
 *                                                                                   *
 *                    File Name : INTERNET.CPP                                       *
 *                                                                                   *
 *                   Programmer : Steve Tall                                         *
 *                                                                                   *
 *                   Start Date : March 11th, 1996                                   *
 *                                                                                   *
 *                  Last Update : August 5th, 1996 [ST]                              *
 *                                                                                   *
 *-----------------------------------------------------------------------------------*
 * Overview:                                                                         *
 *                                                                                   *
 *  Miscellaneous junk related to H2H internet connection.                           *
 *                                                                                   *
 *-----------------------------------------------------------------------------------*
 * Functions:                                                             	 			*
 *                                                                         				*
 *                                                                         				*
 *                                                                         				*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */



#include "FUNCTION.H"
#include "TCPIP.H"


#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
extern bool Is_Mission_126x126 (char *file_name);
extern bool Is_Mission_Aftermath (char *file_name);
extern bool Is_Mission_Counterstrike (char *file_name);
#endif

/***************************************************************************
** Internet specific globals
*/
char PlanetWestwoodIPAddress[IP_ADDRESS_MAX] = {"206.154.108.87"};	//IP of server or other player
uint16_t PlanetWestwoodPortNumber = 1234;					//Port number to send to
bool PlanetWestwoodIsHost = false;						//Flag true if player has control of game options
uint32_t PlanetWestwoodGameID;						//Game ID
uint32_t PlanetWestwoodStartTime;					//Time that game was started
bool	GameStatisticsPacketSent;							//Flag that game stats have been sent to wchat
bool	ConnectionLost;										//Flag that the connection to the other player was lost

#ifdef FRENCH
#define TXT_HACKHACK  "Connexion En Cours..."
#endif
#if defined(ENGLISH) || defined(GERMAN)
#define TXT_HACKHACK  Text_String(TXT_CONNECTING)
#endif
