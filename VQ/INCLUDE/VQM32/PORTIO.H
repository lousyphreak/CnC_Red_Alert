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

#ifndef VQMPORTIO_H
#include <stdint.h>
#define VQMPORTIO_H
/****************************************************************************
*
*        C O N F I D E N T I A L -- W E S T W O O D  S T U D I O S
*
*----------------------------------------------------------------------------
*
* FILE
*     Portio.h (32-Bit protected mode)
*
* DESCRIPTION
*     Hardware port I/O
*
* PROGRAMMER
*     Denzil E. Long, Jr.
*
* DATE
*     Febuary 3, 1995
*
****************************************************************************/

#ifdef __BORLANDC__

#ifdef __cplusplus
extern "C" {
#endif

int16_t inp(uint16_t portid);
void outp(uint16_t portid, int16_t value);

#ifdef __cplusplus
}
#endif

#endif /* __BORLANDC__ */

#endif /* VQMPORTIO_H */

