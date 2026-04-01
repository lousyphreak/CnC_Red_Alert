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

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood Library                         *
 *                                                                         *
 *                    File Name : FILEIO.C                                 *
 *                                                                         *
 *                   Programmer : Joe L. Bostic                            *
 *                                                                         *
 *                   Start Date : August 21, 1991                          *
 *                                                                         *
 *                  Last Update : September 13, 1993   [SKB]               *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef WWSTD_H
#include "wwstd.h"
#endif	   

#ifndef _FILE_H
#include "_file.h"
#endif

#include <direct.h>
#include <fcntl.h>
#include <unistd.h>
#include "sdl_fs.h"

/*=========================================================================*/
/* The following PRIVATE functions are in this file:                       */
/*=========================================================================*/

/*= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =*/


WORD ibm_getdisk(VOID)
{
	CallingDOSInt++;
	const unsigned disk = WWFS_GetCurrentDriveNumber();
	CallingDOSInt--;
	return(disk-1);
}

WORD ibm_setdisk(WORD drive)
{
	CallingDOSInt++;
	WWFS_ChangeToDrive(static_cast<unsigned>(drive + 1));
	CallingDOSInt--;
	return(static_cast<WORD>(WWFS_GetDriveCount()));
}

WORD ibm_close(WORD handle)
{
	WORD success;

	CallingDOSInt++;
	success = WWFS_Close(handle);
	CallingDOSInt--;
	return(success);
}

WORD ibm_unlink(BYTE const *name)
{
	WORD success;

	CallingDOSInt++;
	success = WWFS_Unlink(name);
	CallingDOSInt--;
	return(success);
}

LONG ibm_lseek(WORD handle, LONG offset, WORD where)
{
	LONG new_offset;

	CallingDOSInt++;
	new_offset = WWFS_Seek(handle, offset, where);
	CallingDOSInt--;
	return(new_offset);
}

UWORD ibm_read(WORD handle, VOID *ptr, UWORD bytes)
{
	UWORD bytes_read;

	CallingDOSInt++;
	bytes_read = WWFS_Read(handle, ptr, bytes);
	CallingDOSInt--;
	return(bytes_read);
}

UWORD ibm_write(WORD handle, VOID *ptr, UWORD bytes)
{
	UWORD bytes_written;

	CallingDOSInt++;
	bytes_written = WWFS_Write(handle, ptr, bytes);
	CallingDOSInt--;
	return(bytes_written);
}

WORD ibm_open(BYTE const *name, UWORD mode, WORD attrib)
{
	WORD handle;

	CallingDOSInt++;
	handle = WWFS_Open(name, mode, attrib);
	CallingDOSInt--;
	return(handle);
}

WORD ibm_chdir(BYTE const *path)
{
	WORD retval;

	CallingDOSInt++;
	retval = WWFS_ChangeDirectory(path);
	CallingDOSInt--;
	return(retval);
}




