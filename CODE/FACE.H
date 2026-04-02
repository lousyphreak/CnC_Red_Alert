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

/* $Header: /CounterStrike/FACE.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : FACE.H                                                       *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 03/08/96                                                     *
 *                                                                                             *
 *                  Last Update : March 8, 1996 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef FACE_H
#define FACE_H

#include <stdint.h>

// Enumerations of the facing values returned from Desired_Facing().
typedef enum DirType : uint8_t {
	DIR_MIN=0,
	DIR_N=0,
	DIR_NE=1<<5,
	DIR_E=2<<5,
	DIR_SE=3<<5,
	DIR_S=4<<5,
	DIR_SW=5<<5,
	DIR_SW_X1=(5<<5)-8,
	DIR_SW_X2=(5<<5)-16,
	DIR_W=6<<5,
	DIR_NW=7<<5,
	DIR_MAX=255
} DirType;
static_assert(sizeof(DirType) == 1, "DirType must remain 8-bit");

// Operators that allow simple math with DirType.
inline DirType operator + (DirType f1, DirType f2)
{
	return (DirType)(uint8_t)((uint32_t)f1 + (uint32_t)f2);
}
inline DirType operator + (DirType f1, int f2)
{
	return (DirType)(uint8_t)((uint32_t)f1 + (uint32_t)f2);
}
inline DirType operator - (DirType f1, DirType f2)
{
	return (DirType)(uint8_t)((uint32_t)f1 - (uint32_t)f2);
}
inline DirType operator - (DirType f1, int f2)
{
	return (DirType)(uint8_t)((uint32_t)f1 - (uint32_t)f2);
}


// Function prototypes.
DirType Desired_Facing8(int x1, int y1, int x2, int y2);
DirType Desired_Facing256(int srcx, int srcy, int dstx, int dsty);

#endif

