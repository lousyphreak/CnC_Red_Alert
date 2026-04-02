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

/* $Header: /CounterStrike/RAMFILE.H 1     3/03/97 10:25a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : RAMFILE.H                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 06/30/96                                                     *
 *                                                                                             *
 *                  Last Update : June 30, 1996 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#ifndef RAMFILE_H
#define RAMFILE_H
#include <stdint.h>

#include	<cstdint>
#include	"WWFILE.H"

class RAMFileClass : public FileClass
{
	public:
		RAMFileClass(void * buffer, int len);
		virtual ~RAMFileClass(void);

		virtual char const * File_Name(void) const {return("UNKNOWN");}
		virtual char const * Set_Name(char const * ) {return(File_Name());}
		virtual int Create(void);
		virtual int Delete(void);
		virtual int Is_Available(int forced=false);
		virtual int Is_Open(void) const;
		virtual int Open(char const * filename, int access=READ);
		virtual int Open(int access=READ);
		virtual int32_t Read(void * buffer, int32_t size);
		virtual int32_t Seek(int32_t pos, int dir=SEEK_CUR);
		virtual int32_t Size(void);
		virtual int32_t Write(void const * buffer, int32_t size);
		virtual void Close(void);
		virtual uint32_t Get_Date_Time(void) {return(0);}
		virtual bool Set_Date_Time(uint32_t ) {return(true);}
		virtual void Error(int , int = false, char const * =NULL) {}

		operator char const * () {return File_Name();}

	private:

		/*
		**	Pointer to the buffer that the "file" will reside in.
		*/
		char * Buffer;

		/*
		**	The maximum size of the buffer. The file occupying the buffer
		**	may be smaller than this size.
		*/
		int32_t MaxLength;

		/*
		**	The number of bytes in the sub-file occupying the buffer.
		*/
		int32_t Length;

		/*
		**	The current file position offset within the buffer.
		*/
		int32_t Offset;

		/*
		**	The file was opened with this access mode.
		*/
		int Access;

		/*
		**	Is the file currently open?
		*/
		bool IsOpen;

		/*
		**	Was the file buffer allocated during construction of this object?
		*/
		bool IsAllocated;
};




#endif
