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

/* $Header: /CounterStrike/PALETTE.H 2     9/23/97 11:00p Steve_t $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : PALETTEC.H                                                   *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 12/02/95                                                     *
 *                                                                                             *
 *                  Last Update : December 2, 1995 [JLB]                                       *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef PALETTEC_H
#define PALETTEC_H

#include "RGB.H"

/*
**	The palette class is used to manipulate a palette as a whole. All 256 colors are
**	represented by the palette class object.
*/
class PaletteClass
{
	public:
		enum {
			COLOR_COUNT = 256
		};

		PaletteClass(void) {}
		PaletteClass(RGBClass const & rgb);

		RGBClass & operator[] (unsigned index) { return(Palette[index % COLOR_COUNT]); }
		RGBClass const & operator[] (unsigned index) const { return(Palette[index % COLOR_COUNT]); }
		int operator == (PaletteClass const & palette) const;
		int operator != (PaletteClass const & palette) const { return(!(operator == (palette))); }
		PaletteClass & operator = (PaletteClass const & palette);

		operator unsigned char * (void) { return(reinterpret_cast<unsigned char *>(&Palette[0])); }
		operator unsigned char const * (void) const { return(reinterpret_cast<unsigned char const *>(&Palette[0])); }

		void * Get_Data(void) { return(&Palette[0]); }
		void const * Get_Data(void) const { return(&Palette[0]); }

		void Adjust(int ratio);
		void Adjust(int ratio, PaletteClass const & palette);
		void Partial_Adjust(int ratio, char * lut);
		void Partial_Adjust(int ratio, PaletteClass const & palette, char * lut);
		void Set(int time = 0, void (*callback)(void) = 0) const;
		int Closest_Color(RGBClass const & rgb) const;

		static PaletteClass const & CurrentPalette;

	protected:
		RGBClass Palette[COLOR_COUNT];
};

#endif
