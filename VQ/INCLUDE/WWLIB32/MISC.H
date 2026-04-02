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
 *                 Project Name : 32 bit library                           *
 *                                                                         *
 *                    File Name : MISC.H                                   *
 *                                                                         *
 *                   Programmer : Scott K. Bowen                           *
 *                                                                         *
 *                   Start Date : August 3, 1994                           *
 *                                                                         *
 *                  Last Update : August 3, 1994   [SKB]                   *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef MISC_H
#include <stdint.h>
#define MISC_H

/*========================= C++ Routines ==================================*/

/*=========================================================================*/
/* The following prototypes are for the file: EXIT.CPP							*/
/* Prog_End Must be supplied by the user program in startup.cpp				*/
/*=========================================================================*/
void  Prog_End(void);
void  Exit(int32_t errorval, const char *message, ...);

/*=========================================================================*/
/* The following prototypes are for the file: DELAY.CPP							*/
/*=========================================================================*/
void Delay(int duration);
void Vsync(void);


/*=========================================================================*/
/* The following prototypes are for the file: FINDARGV.CPP						*/
/*=========================================================================*/

char * Find_Argv(char const *str);

/*=========================================================================*/
/* The following prototypes are for the file: LIB.CPP								*/
/*=========================================================================*/
char *Find_Argv(char const *str);
void Mono_Mem_Dump(void const *databuf, int bytes, int y);
void Convert_RGB_To_HSV(uint32_t r, uint32_t g, uint32_t b, uint32_t *h, uint32_t *s, uint32_t *v);
void Convert_HSV_To_RGB(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b);


/*=========================================================================*/
/* The following prototypes are for the file: VERSION.CPP						*/
/*=========================================================================*/

char * Version(void);


/*=========================================================================*/
/* The following prototypes are for the file: IRANDOM.CPP						*/
/*=========================================================================*/
int IRandom(int minval, int maxval);


/*========================= Assembly Routines ==============================*/

#ifdef __cplusplus
extern "C" {
#endif

/*=========================================================================*/
/* The following prototypes are for the file: RANDOM.ASM							*/
/*=========================================================================*/

uint8_t Random(void);
int Get_Random_Mask(int maxval);

/*=========================================================================*/
/* The following prototype is for the file: SHAKESCR.ASM							*/
/*=========================================================================*/

void Shake_Screen(int shakes);

/*=========================================================================*/
/* The following prototypes are for the file: REVERSE.ASM						*/
/*=========================================================================*/

int32_t Reverse_LONG(int32_t number);
int16_t Reverse_WORD(int16_t number);
int32_t Swap_LONG(int32_t number);

/*=========================================================================*/
/* The following prototype is for the file: FACING8.ASM							*/
/*=========================================================================*/

int Desired_Facing8(int x1, int y1, int x2, int y2);

/*=========================================================================*/
/* The following prototype is for the file: FACING16.ASM							*/
/*=========================================================================*/

int Desired_Facing16(int x1, int y1, int x2, int y2);

/*=========================================================================*/
/* The following prototype is for the file: FACINGFF.ASM							*/
/*=========================================================================*/

int Desired_Facing256(int x1, int y1, int x2, int y2);

/*=========================================================================*/
/* The following prototype is for the file: FADING.ASM							*/
/*=========================================================================*/

void *Build_Fading_Table(void const *palette, void const *dest, int32_t color, int32_t frac);

/*=========================================================================*/
/* The following prototype is for the file: CRC.ASM							*/
/*=========================================================================*/

int32_t Calculate_CRC(void *buffer, int32_t length);

/*=========================================================================*/
/* The following prototypes are for the file: DETPROC.ASM						*/
/*=========================================================================*/

extern int16_t  Processor(void);
extern int16_t  Operating_System(void);
extern uint32_t random ( uint32_t mod )  ;
extern void  randomize ( void ) ;

extern int Clip_Rect ( int * x , int * y , int * dw , int * dh ,
	       	   			 int width , int height ) ;          			   
extern int Confine_Rect ( int * x , int * y , int dw , int dh , 
	      	      			 int width , int height ) ;          		   



/*=========================================================================*/
/* The following prototypes are for the file: OPSYS.ASM							*/
/*=========================================================================*/

extern int16_t OperationgSystem;
										  
#ifdef __cplusplus
}
#endif

/*=========================================================================*/

#endif // MISC_H
