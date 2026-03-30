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
