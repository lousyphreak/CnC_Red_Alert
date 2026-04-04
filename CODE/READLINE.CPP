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


#include	<ctype.h>
#include	<string.h>
#include	"WWFILE.H"
#include	"XSTRAW.H"
#include	"READLINE.H"


#include <stdint.h>
// Disable the "temporary object used to initialize a non-constant reference" warning.


void strtrim(char * buffer)
{
	if (buffer) {

		/*
		**	Strip leading white space from the string.
		*/
		char * source = buffer;
		while (isspace(static_cast<uint8_t>(*source))) {
			source++;
		}
		if (source != buffer) {
			memmove(buffer, source, strlen(source) + 1);
		}

		/*
		**	Clip trailing white space from the string.
		*/
		int length = static_cast<int>(strlen(buffer));
		while (length > 0 && isspace(static_cast<uint8_t>(buffer[length - 1]))) {
			buffer[--length] = '\0';
		}
	}
}



int Read_Line(FileClass & file, char * buffer, int len, bool & eof)
{
	FileStraw straw(file);
	return(Read_Line(straw, buffer, len, eof));
}


int Read_Line(Straw & file, char * buffer, int len, bool & eof)
{
	if (len == 0 || buffer == NULL) return(0);

	int count = 0;
	for (;;) {
		char c;
		if (file.Get(&c, sizeof(c)) != sizeof(c)) {
			eof = true;
			buffer[0] = '\0';
			break;
		}

		if (c == '\x0A') break;
		if (c != '\x0D' && count+1 < len) {
			buffer[count++] = c;
		}
	}
	buffer[count] = '\0';

	strtrim(buffer);
	return(strlen(buffer));
}
