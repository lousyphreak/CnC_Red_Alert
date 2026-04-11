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

/* $Header: /CounterStrike/CCFILE.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : CCFILE.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : October 17, 1994                                             *
 *                                                                                             *
 *                  Last Update : October 17, 1994   [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef CCFILE_H
#define CCFILE_H

//#include	<WWLIB32.H>
#include	<limits.h>
#include	<stdint.h>
#include	<cstdint>
#include	"MIXFILE.H"
#include	"BFIOFILE.H"
#include	"BUFF.H"

/*
**	This derived class for file access knows about mixfiles (packed files). It can handle opening
**	a file that is embedded within a mixfile. This is true if the mixfile is cached or resides on
**	disk. It is functionally similar to pakfiles, except much faster and less RAM intensive.
*/
class CCFileClass : public BufferIOFileClass
{
	public:
		CCFileClass(char const * filename);
		CCFileClass(void);
		virtual ~CCFileClass(void) {Position = 0;};

		static void Set_Local_Search_Root(char const* path);
		static char const* Get_Local_Search_Root(void);

		// Delete should be overloaded here as well. Don't allow deletes of mixfiles.

		bool Is_Resident(void) const {return(Data.Get_Buffer() != NULL);}
		virtual int Is_Available(int forced=false);
		virtual int Is_Open(void) const;
		virtual int Open(char const * filename, int rights=READ) {Set_Name(filename);return Open(rights);};
		virtual int Open(int rights=READ);
		virtual int32_t Read(void * buffer, int32_t size);
		virtual int32_t Seek(int32_t pos, int dir=SEEK_CUR);
		virtual int32_t Size(void);
		virtual int32_t Write(void const * buffer, int32_t size);
		virtual void Close(void);
		virtual uint32_t Get_Date_Time(void);
		virtual bool Set_Date_Time(uint32_t datetime);
		virtual void Error(int error, int canretry = false, char const * filename=NULL);

	private:

		/*
		**	This indicates the file is actually part of a resident image of the mixfile
		**	itself. In this case, the embedded file handle is invalid. All file access actually
		**	gets routed through the cached version of the file. This is a pointer to the start
		**	of the RAM image of the file.
		*/
		::Buffer Data;
		void * Pointer;
		bool FromDisk;

		/*
		**	This is the size of the file if it was embedded in a mixfile. The size must be manually
		**	kept track of because the DOS file size is invalid.
		*/
		int32_t Length;
		int32_t Start;

		/*
		**	This is the current seek position of the file. It is duplicated here if the file is
		**	part of a mixfile since the DOS seek position is not accurate. This value will
		**	range from zero to the size of the file in bytes.
		*/
		int32_t Position;

		// Force these to never be invoked.
		CCFileClass const & operator = (CCFileClass const & c);
		CCFileClass (CCFileClass const & );
};

template<class T> class MixFileClass;

#endif
