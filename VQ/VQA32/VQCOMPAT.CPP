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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "CCFILE.H"
#include "DRAWBUFF.H"
#include "FONT.H"
#include "GBUFFER.H"
#include "INI.H"
#include "PALETTE.H"

extern "C" unsigned char ColorXlat[256];

static unsigned short Read_Font_Word(unsigned char const *ptr)
{
	return (unsigned short)(ptr[0] | ((unsigned short)ptr[1] << 8));
}


static void Ensure_Font_Palette(void)
{
	static bool initialized = false;

	if (!initialized) {
		memset(ColorXlat, 0, sizeof(ColorXlat));
		for (int index = 0; index < 16; ++index) {
			ColorXlat[index] = (unsigned char)index;
			ColorXlat[index << 4] = (unsigned char)index;
		}
		initialized = true;
	}
}


static long Copy_Default_String(char *buffer, long buffer_len, char const *value)
{
	size_t length;

	if (buffer == NULL || buffer_len <= 0) {
		return (0);
	}

	if (value == NULL) {
		value = "";
	}

	length = strlen(value);
	if (length >= (size_t)buffer_len) {
		length = (size_t)buffer_len - 1;
	}

	memcpy(buffer, value, length);
	buffer[length] = '\0';
	return ((long)length);
}


static bool Prepare_Font(unsigned char const *&font,
		unsigned char const *&info_block,
		unsigned char const *&width_block,
		unsigned char const *&height_block,
		unsigned char const *&offset_block,
		int &max_height)
{
	if (FontPtr == NULL) {
		return (false);
	}

	font = (unsigned char const *)FontPtr;
	info_block = font + Read_Font_Word(font + FONTINFOBLOCK);
	width_block = font + Read_Font_Word(font + FONTWIDTHBLOCK);
	height_block = font + Read_Font_Word(font + FONTHEIGHTBLOCK);
	offset_block = font + Read_Font_Word(font + FONTOFFSETBLOCK);
	max_height = info_block[FONTINFOMAXHEIGHT];
	return (true);
}


static void Draw_Char_To_View(GraphicViewPortClass *view, unsigned char character,
		int x, int y)
{
	unsigned char const *font;
	unsigned char const *info_block;
	unsigned char const *width_block;
	unsigned char const *height_block;
	unsigned char const *offset_block;
	unsigned char const *data_ptr;
	unsigned char const *char_height_ptr;
	unsigned char *buffer;
	unsigned char *draw_ptr;
	int max_height;
	int row_bytes;
	int width;
	int top_blank;
	int char_height;
	int bottom_blank;

	if (view == NULL) {
		return;
	}

	if (!Prepare_Font(font, info_block, width_block, height_block, offset_block,
			max_height)) {
		return;
	}

	width = width_block[character];
	if (x < 0 || y < 0 || x + width > view->Get_Width()
			|| y + max_height > view->Get_Height()) {
		return;
	}

	buffer = (unsigned char *)(uintptr_t)view->Get_Offset();
	row_bytes = view->Get_Pitch();
	data_ptr = font + Read_Font_Word(offset_block + character * 2);
	char_height_ptr = height_block + character * 2;
	top_blank = char_height_ptr[0];
	char_height = char_height_ptr[1];
	bottom_blank = max_height - top_blank - char_height;
	draw_ptr = buffer + (y * row_bytes) + x;

	if (ColorXlat[0] != 0) {
		for (int row = 0; row < top_blank; ++row) {
			memset(draw_ptr, ColorXlat[0], (size_t)width);
			draw_ptr += row_bytes;
		}
	} else {
		draw_ptr += top_blank * row_bytes;
	}

	for (int row = 0; row < char_height; ++row) {
		unsigned char *pixel_ptr = draw_ptr;
		int column = 0;

		while (column < width) {
			const unsigned char pair = *data_ptr++;
			const unsigned char lo = ColorXlat[pair & 0x0F];
			if (lo != 0) {
				*pixel_ptr = lo;
			}
			++pixel_ptr;
			++column;

			if (column < width) {
				const unsigned char hi = ColorXlat[(pair >> 4) & 0x0F];
				if (hi != 0) {
					*pixel_ptr = hi;
				}
				++pixel_ptr;
				++column;
			}
		}

		draw_ptr += row_bytes;
	}

	if (ColorXlat[0] != 0) {
		for (int row = 0; row < bottom_blank; ++row) {
			memset(draw_ptr, ColorXlat[0], (size_t)width);
			draw_ptr += row_bytes;
		}
	}
}


long cdecl GetINIInt(char const *section, char const *entry,
		long deflt, char *fname)
{
	INIClass ini;

	if (fname != NULL && *fname != '\0') {
		CCFileClass file(fname);
		if (ini.Load(file)) {
			return ((long)ini.Get_Int(section, entry, (int)deflt));
		}
	}

	return (deflt);
}


long cdecl GetINIString(char const *section, char const *entry,
		char const *deflt, char *retbuffer, long retlen, char *fname)
{
	INIClass ini;

	if (retbuffer == NULL || retlen <= 0) {
		return (0);
	}

	if (fname != NULL && *fname != '\0') {
		CCFileClass file(fname);
		if (ini.Load(file)) {
			return ((long)ini.Get_String(section, entry, deflt ? deflt : "",
					retbuffer, (int)retlen));
		}
	}

	return (Copy_Default_String(retbuffer, retlen, deflt));
}


extern "C" long cdecl Text_Print(char *string, long x, long y, long fcol, long bcol)
{
	unsigned char const *font;
	unsigned char const *info_block;
	unsigned char const *width_block;
	unsigned char const *height_block;
	unsigned char const *offset_block;
	int max_height;
	int current_x;
	int current_y;
	int original_x;
	long lines;

	if (LogicPage == NULL || string == NULL) {
		return (0);
	}

	if (!LogicPage->Lock()) {
		return (0);
	}

	if (!Prepare_Font(font, info_block, width_block, height_block, offset_block,
			max_height)) {
		LogicPage->Unlock();
		return (0);
	}

	if (y < 0 || y + max_height > LogicPage->Get_Height()) {
		LogicPage->Unlock();
		return (0);
	}

	Ensure_Font_Palette();
	ColorXlat[1] = (unsigned char)fcol;
	ColorXlat[16] = (unsigned char)fcol;
	ColorXlat[0] = (unsigned char)bcol;

	current_x = (int)x;
	current_y = (int)y;
	original_x = (int)x;
	lines = 1;

	while (*string != '\0') {
		const unsigned char ch = (unsigned char)*string++;
		const int width = width_block[ch];
		const int advance = width + FontXSpacing;

		if (ch == '\n' || ch == '\r') {
			const int next_y = current_y + max_height + FontYSpacing;
			if (next_y + max_height > LogicPage->Get_Height()) {
				LogicPage->Unlock();
				return (0);
			}

			current_y = next_y;
			current_x = (ch == '\n') ? 0 : original_x;
			++lines;
			continue;
		}

		if (current_x + advance > LogicPage->Get_Width()) {
			const int next_y = current_y + max_height + FontYSpacing;
			if (next_y + max_height > LogicPage->Get_Height()) {
				LogicPage->Unlock();
				return (0);
			}

			current_y = next_y;
			current_x = 0;
			++lines;
		}

		Draw_Char_To_View(LogicPage, ch, current_x, current_y);
		current_x += advance;
	}

	LogicPage->Unlock();
	return (lines);
}


extern "C" void cdecl Draw_Char(long character, long x, long y)
{
	if (LogicPage == NULL) {
		return;
	}

	if (!LogicPage->Lock()) {
		return;
	}

	Ensure_Font_Palette();
	Draw_Char_To_View(LogicPage, (unsigned char)character, (int)x, (int)y);
	LogicPage->Unlock();
}


extern "C" void cdecl Fill_Rect(long x1, long y1, long x2, long y2, long color)
{
	if (LogicPage != NULL) {
		LogicPage->Fill_Rect((int)x1, (int)y1, (int)x2, (int)y2,
				(unsigned char)color);
	}
}


extern "C" void cdecl SetDAC(long color, long red, long green, long blue)
{
	unsigned char palette[PALETTE_BYTES];
	unsigned char *entry;

	if (color < 0 || color >= PALETTE_SIZE) {
		return;
	}

	memcpy(palette, CurrentPalette, sizeof(palette));
	entry = palette + (color * RGB_BYTES);
	entry[0] = (unsigned char)((red > 63) ? 63 : ((red < 0) ? 0 : red));
	entry[1] = (unsigned char)((green > 63) ? 63 : ((green < 0) ? 0 : green));
	entry[2] = (unsigned char)((blue > 63) ? 63 : ((blue < 0) ? 0 : blue));
	Set_Palette(palette);
}


long TestVBIBit(void)
{
	return (0);
}


long GetVBIBit(void)
{
	return (0);
}


extern "C" void cdecl WaitNoVB(short vbibit)
{
	(void)vbibit;
}


extern "C" void cdecl WaitVB(short vbibit)
{
	(void)vbibit;
}
