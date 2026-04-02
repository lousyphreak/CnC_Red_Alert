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

#include "RGB.H"

#include <stdint.h>
extern "C" void Set_Palette_Register(int number, int red, int green, int blue);

static int CurrentPaletteRegister = 0;


void outportb(int port, uint8_t data)
{
	if (port == 0x03C8) {
		CurrentPaletteRegister = data;
	}
}


void outport(int port, uint16_t data)
{
	if (port == 0x03C8) {
		CurrentPaletteRegister = (uint8_t)data;
	}
}


void outrgb(uint8_t red, uint8_t green, uint8_t blue)
{
	Set_Palette_Register(CurrentPaletteRegister & 0xFF, red, green, blue);
	CurrentPaletteRegister = (CurrentPaletteRegister + 1) & 0xFF;
}
