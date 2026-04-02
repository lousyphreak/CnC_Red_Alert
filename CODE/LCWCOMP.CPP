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

#include "LCW.H"
#include <string.h>
#include <stdint.h>

extern "C" int  LCW_Comp(void const *source, void *dest, int length)
{
	if (source == 0 || dest == 0 || length < 0) {
		return 0;
	}

	const uint8_t* input = (const uint8_t*)source;
	uint8_t* output = (uint8_t*)dest;
	int remaining = length;

	while (remaining > 0) {
		const uint8_t chunk = (remaining > 0x3F) ? 0x3F : (uint8_t)remaining;
		*output++ = (uint8_t)(0x80U | chunk);
		memcpy(output, input, chunk);
		output += chunk;
		input += chunk;
		remaining -= chunk;
	}

	*output++ = 0x80;
	return (int)(output - (uint8_t*)dest);
}
