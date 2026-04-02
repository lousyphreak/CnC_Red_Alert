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
 **     C O N F I D E N T I A L --- W E S T W O O D   S T U D I O S       **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Memory System                            *
 *                                                                         *
 *                    File Name : MEMFLAG.H                                *
 *                                                                         *
 *                   Programmer : Jeff Wilson                              *
 *                                                                         *
 *                   Start Date : April 4, 1994                            *
 *                                                                         *
 *                  Last Update : September 8, 1994   [IML]                *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifndef MEMFLAG_H
#include <stdint.h>
#define MEMFLAG_H
// Memory Flags
/*
**	Memory allocation flags.  These are the flags that are passed into Alloc
**	in order to control the type of memory allocated.
*/
typedef enum {
	MEM_NORMAL = 0x0000,		// Default memory (normal).
	MEM_NEW	  = 0x0001,		// Called by the operator new and was overloaded.
	MEM_CLEAR  = 0x0002,		// Clear memory before returning.
	MEM_REAL   = 0x0004,		// Clear memory before returning.
	MEM_TEMP   = 0x0008,		// Clear memory before returning.
} MemoryFlagType;


/*=========================================================================*/
/* The following prototypes are for the file: ALLOC.CPP							*/
/*=========================================================================*/

void *Alloc(uint32_t bytes_to_alloc, MemoryFlagType flags);
void Free(void *pointer);
void *Resize_Alloc(void *original_ptr, uint32_t new_size_in_bytes);
int32_t Ram_Free(MemoryFlagType flag);
int32_t Heap_Size(MemoryFlagType flag);
int32_t Total_Ram_Free(MemoryFlagType flag);


inline void * operator new(size_t size, MemoryFlagType flag) 
{
	return(Alloc(size, flag));
}
inline void * operator new[] (size_t size, MemoryFlagType flag)
{
	return(Alloc(size, flag));
}


/*=========================================================================*/
/* The following prototypes are for the file: MEM_COPY.ASM						*/
/*=========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

	extern void Mem_Copy(void *source, void *dest, uint32_t bytes_to_copy);

#ifdef __cplusplus
}
#endif


inline void *Add_Long_To_Pointer(void const *ptr, int32_t size) 
{
 	return ((void *) ( (char const *) ptr + size));
}

extern void (*Memory_Error)(void);

extern uint32_t MinRam;		// Record of least memory at worst case.
extern uint32_t MaxRam;		// Record of total allocated at worst case.


#endif


