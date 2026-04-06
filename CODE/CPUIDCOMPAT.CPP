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

#if defined(__x86_64__) || defined(_M_X64)
char CPUType = 6;
#elif defined(__i386__) || defined(_M_IX86)
char CPUType = 5;
#else
char CPUType = 0;
#endif

char VendorID[17] = "Generic CPU";

bool  Detect_MMX_Availability(void)
{
#if defined(__MMX__) || defined(_M_IX86_FP) || defined(__x86_64__) || defined(_M_X64)
	return(true);
#else
	return(false);
#endif
}
