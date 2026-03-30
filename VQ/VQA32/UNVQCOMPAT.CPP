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

#include <string.h>
#include "UNVQ.H"

extern "C" void cdecl UnVQ_4x2(unsigned char *codebook, unsigned char *pointers,
		unsigned char *buffer, unsigned long blocksperrow,
		unsigned long numrows, unsigned long bufwidth)
{
	const unsigned long entry_count = blocksperrow * numrows;
	unsigned char *lo_ptr = pointers;
	unsigned char *hi_ptr = pointers + entry_count;
	unsigned char *row_ptr = buffer;

	if (codebook == NULL || pointers == NULL || buffer == NULL) {
		return;
	}

	for (unsigned long row = 0; row < numrows; ++row) {
		unsigned char *row0 = row_ptr;
		unsigned char *row1 = row_ptr + bufwidth;

		for (unsigned long column = 0; column < blocksperrow; ++column) {
			const unsigned char low = *lo_ptr++;
			const unsigned char high = *hi_ptr++;

			if (high == 0x0F) {
				memset(row0, low, 4);
				memset(row1, low, 4);
			} else {
				unsigned char *entry = codebook + ((((unsigned long)high << 8) | low) * 8);
				memcpy(row0, entry, 4);
				memcpy(row1, entry + 4, 4);
			}

			row0 += 4;
			row1 += 4;
		}

		row_ptr += bufwidth * 2;
	}
}

extern "C" void cdecl UnVQ_4x4(unsigned char *codebook, unsigned char *pointers,
		unsigned char *buffer, unsigned long blocksperrow,
		unsigned long numrows, unsigned long bufwidth)
{
	static const unsigned short SKIP_PTR = 0x8000u;
	unsigned char *row_ptr = buffer;

	if (codebook == NULL || pointers == NULL || buffer == NULL) {
		return;
	}

	for (unsigned long row = 0; row < numrows; ++row) {
		unsigned char *row0 = row_ptr;
		unsigned char *row1 = row_ptr + bufwidth;
		unsigned char *row2 = row_ptr + (bufwidth * 2);
		unsigned char *row3 = row_ptr + (bufwidth * 3);

		for (unsigned long column = 0; column < blocksperrow; ++column) {
			const unsigned short code = (unsigned short)pointers[0] | ((unsigned short)pointers[1] << 8);
			pointers += 2;

			if ((code & 0x8000u) != 0) {
				if (code != SKIP_PTR) {
					const unsigned char color = (unsigned char)(~code & 0xFFu);
					memset(row0, color, 4);
					memset(row1, color, 4);
					memset(row2, color, 4);
					memset(row3, color, 4);
				}
			} else {
				unsigned char *entry = codebook + code - 4;
				memcpy(row0, entry, 4);
				memcpy(row1, entry + 4, 4);
				memcpy(row2, entry + 8, 4);
				memcpy(row3, entry + 12, 4);
			}

			row0 += 4;
			row1 += 4;
			row2 += 4;
			row3 += 4;
		}

		row_ptr += bufwidth * 4;
	}
}
