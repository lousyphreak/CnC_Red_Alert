/*
**Command & Conquer Red Alert(tm)
**Copyright 2025 Electronic Arts Inc.
**
**This program is free software: you can redistribute it and/or modify
**it under the terms of the GNU General Public License as published by
**the Free Software Foundation, either version 3 of the License, or
**(at your option) any later version.
**
**This program is distributed in the hope that it will be useful,
**but WITHOUT ANY WARRANTY; without even the implied warranty of
**MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**GNU General Public License for more details.
**
**You should have received a copy of the GNU General Public License
**along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FUNCTION.H"
#include "EXTERNS.H"
#include <string.h>

static uint8_t TopLine[640];
static uint8_t BottomLine[640];
static uint8_t LineBuffer[640];


static void Interpolate_Single_Line(uint8_t const *source_ptr, uint8_t *dest_ptr, int source_width)
{
if (source_width <= 0) {
return;
}

for (int index = 0; index < source_width - 1; ++index) {
dest_ptr[index * 2] = source_ptr[index];
dest_ptr[index * 2 + 1] = PaletteInterpolationTable[source_ptr[index + 1]][source_ptr[index]];
}

dest_ptr[(source_width - 1) * 2] = source_ptr[source_width - 1];
dest_ptr[(source_width - 1) * 2 + 1] = 0;
}


static void Interpolate_Between_Lines(uint8_t const *source1,
uint8_t const *source2,
uint8_t *destination,
int source_width)
{
for (int index = 0; index < source_width * 2; ++index) {
destination[index] = PaletteInterpolationTable[source2[index]][source1[index]];
}
}


void  Stop_Execution(void){
}


void  Asm_Interpolate(uint8_t *src_ptr,	uint8_t *dest_ptr,
	int lines,
	int source_width,
	int dest_width)
{
int row_bytes = dest_width / 2;

for (int row = 0; row < lines; ++row) {
uint8_t *dest_row = dest_ptr + row * row_bytes * 2;
Interpolate_Single_Line(src_ptr + row * source_width, LineBuffer, source_width);
memcpy(dest_row, LineBuffer, source_width * 2);
memcpy(dest_row + row_bytes, LineBuffer, source_width * 2);
}
}


void  Asm_Interpolate_Line_Double(uint8_t *src_ptr,uint8_t *dest_ptr,
int lines,
int source_width,
int dest_width)
{
int row_bytes = dest_width / 2;

for (int row = 0; row < lines; ++row) {
uint8_t *dest_row = dest_ptr + row * row_bytes * 2;
Interpolate_Single_Line(src_ptr + row * source_width, LineBuffer, source_width);
memcpy(dest_row, LineBuffer, source_width * 2);
memcpy(dest_row + row_bytes, LineBuffer, source_width * 2);
}
}


void  Asm_Interpolate_Line_Interpolate(uint8_t *src_ptr,uint8_t *dest_ptr,
int source_lines,
int source_width,
int dest_width)
{
int row_bytes = dest_width / 2;
uint8_t *last_line = TopLine;
uint8_t *next_line = BottomLine;

if (source_lines <= 0) {
return;
}

Interpolate_Single_Line(src_ptr, next_line, source_width);
memcpy(dest_ptr, next_line, source_width * 2);
memcpy(dest_ptr + row_bytes, next_line, source_width * 2);
src_ptr += source_width;
dest_ptr += row_bytes * 2;
--source_lines;

while (source_lines > 0) {
uint8_t *swap_line;

swap_line = last_line;
last_line = next_line;
next_line = swap_line;
Interpolate_Single_Line(src_ptr, next_line, source_width);
Interpolate_Between_Lines(last_line, next_line, LineBuffer, source_width);
memcpy(dest_ptr, last_line, source_width * 2);
memcpy(dest_ptr + row_bytes, LineBuffer, source_width * 2);
dest_ptr += row_bytes * 2;
src_ptr += source_width;
--source_lines;
}
}
