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

/* $Header: /CounterStrike/COMPAT.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : COMPAT.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 03/02/95                                                     *
 *                                                                                             *
 *                  Last Update : March 2, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef COMPAT_H
#define COMPAT_H

#include <stdint.h>

#include "FIXED.H"


#define	BuffType		BufferClass
//#define movmem(a,b,c) memmove(b,a,c)
#define ShapeBufferSize	_ShapeBufferSize

/*=========================================================================*/
/* Define some equates for the different graphic routines we will install	*/
/*		later.																					*/
/*=========================================================================*/
#define HIDBUFF	  ((void *)(0xA0000))
#define Size_Of_Region(a, b) ((a)*(b))

/*=========================================================================*/
/* Define some Graphic Routines which will only be fixed by these defines	*/
/*=========================================================================*/
#define Set_Font_Palette(a) 		Set_Font_Palette_Range(a, 0, 15)

/*
**	These are the Open_File, Read_File, and Seek_File constants.
*/
#define READ						1	// Read access.
#define WRITE						2	// Write access.

#ifndef SEEK_SET
#define SEEK_SET					0	// Seek from start of file.
#define SEEK_CUR					1	// Seek relative from current location.
#define SEEK_END					2	// Seek from end of file.
#endif

#define ERROR_WINDOW 			1
#define ErrorWindow 			1


//extern unsigned char *Palette;
extern uint8_t MDisabled;			// Is mouse disabled?
extern uint16_t Hard_Error_Occured;

/*
**	This is the menu control structures.
*/
typedef enum MenuIndexType {
	MENUX,
	MENUY,
	ITEMWIDTH,
	ITEMSHIGH,
	MSELECTED,
	NORMCOL,
	HILITE,
	MENUPADDING=0x1000
} MenuIndexType;



/* These defines handle the various names given to the same color. */
#define	DKGREEN	GREEN
#define	DKBLUE	BLUE
#define	GRAY		GREY
#define	DKGREY	GREY
#define	DKGRAY	GREY
#define	LTGRAY	LTGREY


class IconsetClass;

inline int Get_IconSet_MapWidth(void const * data)
{
	if (data) {
		return(((IControl_Type *)data)->MapWidth);
	}
	return(0);
}

inline int Get_IconSet_MapHeight(void const * data)
{
	if (data) {
		return(((IControl_Type *)data)->MapHeight);
	}
	return(0);
}

inline uint8_t const * Get_IconSet_ControlMap(void const * data)
{
	if (data) {
		return((uint8_t const *)((char *)data + ((IControl_Type *)data)->ColorMap));
	}
	return(0);
}

#pragma pack(push, 1)
class IconsetClass : protected IControl_Type
{
	public:
		/*
		**	Query functions.
		*/
		int Map_Width(void) const {return(MapWidth);};
		int Map_Height(void) const {return(MapHeight);};
		uint8_t * Control_Map(void) {return((uint8_t *)this + ColorMap);};
		uint8_t const * Control_Map(void) const {return((uint8_t const *)this + ColorMap);};
		int Icon_Count(void) const {return(Count);};
		int Pixel_Width(void) const {return(Width);};
		int Pixel_Height(void) const {return(Height);};
		int Total_Size(void) const {return(Size);};
		uint8_t const * Palette_Data(void) const {return((uint8_t const *)this + Palettes);};
		uint8_t * Palette_Data(void) {return((uint8_t *)this + Palettes);};
		uint8_t const * Icon_Data(void) const {return((uint8_t const *)this + Icons);};
		uint8_t * Icon_Data(void) {return((uint8_t *)this + Icons);};
		uint8_t const * Map_Data(void) const {return((uint8_t const *)this + Map);};
		uint8_t * Map_Data(void) {return((uint8_t *)this + Map);};
		uint8_t const * Remap_Data(void) const {return((uint8_t const *)this + Remaps);};
		uint8_t * Remap_Data(void) {return((uint8_t *)this + Remaps);};
		uint8_t const * Trans_Data(void) const {return((uint8_t const *)this + TransFlag);};
		uint8_t * Trans_Data(void) {return((uint8_t *)this + TransFlag);};

	/*
	**	Disallow these operations with an IconsetClass object.
	*/
	private:
		IconsetClass & operator = (IconsetClass const &);
		IconsetClass(void);
		static void * operator new(size_t);
};
#pragma pack(pop)

static_assert(sizeof(IconsetClass) == sizeof(IControl_Type), "IconsetClass layout mismatch");


#endif
