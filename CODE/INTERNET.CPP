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
#include "INTERNET.H"
#include "TCPIP.H"


#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
extern bool Is_Mission_126x126 (char *file_name);
extern bool Is_Mission_Aftermath (char *file_name);
extern bool Is_Mission_Counterstrike (char *file_name);
#endif

/***************************************************************************
** Internet specific globals
*/
char PlanetWestwoodIPAddress[IP_ADDRESS_MAX] = {""};	//IP of server or other player
uint16_t PlanetWestwoodPortNumber = 0x8813;				//Port number to send to
bool PlanetWestwoodIsHost = false;						//Flag true if player has control of game options
InternetCommandLineType PlanetWestwoodCommandLine = INTERNET_CMDLINE_NONE;
static char PlanetWestwoodCommandLineName[MPLAYER_NAME_MAX] = {""};
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

bool Internet_Parse_Port(char const* text, uint16_t* port)
{
	char* end = NULL;
	long value;

	if (text == NULL || port == NULL || text[0] == '\0') {
		return(false);
	}

	value = strtol(text, &end, 10);
	if (end == NULL || *end != '\0' || value <= 0 || value > 65535) {
		return(false);
	}

	*port = (uint16_t)value;
	return(true);
}

void Internet_Clear_Command_Line_Connect(void)
{
	PlanetWestwoodCommandLine = INTERNET_CMDLINE_NONE;
}

void Internet_Clear_Command_Line_Name(void)
{
	PlanetWestwoodCommandLineName[0] = '\0';
}

void Internet_Set_Command_Line_Host(uint16_t port)
{
	PlanetWestwoodCommandLine = INTERNET_CMDLINE_HOST;
	PlanetWestwoodIsHost = true;
	PlanetWestwoodIPAddress[0] = '\0';
	PlanetWestwoodPortNumber = port;
}

void Internet_Set_Command_Line_Connect(char const* address, uint16_t port)
{
	PlanetWestwoodCommandLine = INTERNET_CMDLINE_CONNECT;
	PlanetWestwoodIsHost = false;
	SDL_strlcpy(PlanetWestwoodIPAddress, address != NULL ? address : "", sizeof(PlanetWestwoodIPAddress));
	PlanetWestwoodPortNumber = port;
}

void Internet_Set_Command_Line_Name(char const* name)
{
	SDL_strlcpy(PlanetWestwoodCommandLineName, name != NULL ? name : "", sizeof(PlanetWestwoodCommandLineName));
}

bool Internet_Has_Command_Line_Name(void)
{
	return(PlanetWestwoodCommandLineName[0] != '\0');
}

void Internet_Apply_Command_Line_Name(void)
{
	if (PlanetWestwoodCommandLineName[0] != '\0') {
		SDL_strlcpy(Session.Handle, PlanetWestwoodCommandLineName, sizeof(Session.Handle));
	}
}

bool Internet_Has_Command_Line_Connect(void)
{
	return(PlanetWestwoodCommandLine != INTERNET_CMDLINE_NONE);
}

bool Internet_Command_Line_Is_Host(void)
{
	return(PlanetWestwoodCommandLine == INTERNET_CMDLINE_HOST);
}
