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

/* $Header: /CounterStrike/JSHELL.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : JSHELL.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 03/13/95                                                     *
 *                                                                                             *
 *                  Last Update : March 13, 1995 [JLB]                                         *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef JSHELL_H
#define JSHELL_H

#include	<stdint.h>

#include <SDL3/SDL_stdinc.h>

class SystemTimerClass
{
	public:
		long operator () (void) const;
		operator long (void) const;
};
struct NoInitClass;

#include	<assert.h>

template<class T>
class SmartPtr
{
	public:
		SmartPtr(NoInitClass const &) {}
		SmartPtr(T * realptr = 0) : Pointer(realptr) {}
		SmartPtr(SmartPtr const & rvalue) : Pointer(rvalue.Pointer) {}
		~SmartPtr(void) {Pointer = 0;}

		operator T * (void) const {return(Pointer);}

		operator long (void) const {return((long)Pointer);}

		SmartPtr<T> operator ++ (int) {assert(Pointer != 0);SmartPtr<T> temp = *this;++Pointer;return(temp);}
		SmartPtr<T> & operator ++ (void) {assert(Pointer != 0);++Pointer;return(*this);}
		SmartPtr<T> operator -- (int) {assert(Pointer != 0);SmartPtr<T> temp = *this;--Pointer;return(temp);}
		SmartPtr<T> & operator -- (void) {assert(Pointer != 0);--Pointer;return(*this);}

		SmartPtr & operator = (SmartPtr const & rvalue) {Pointer = rvalue.Pointer;return(*this);}
		T * operator -> (void) const {assert(Pointer != 0);return(Pointer);}
		T & operator * (void) const {assert(Pointer != 0);return(*Pointer);}

	private:
		T * Pointer;
};

#include	"KEY.H"
#include	"MOUSE.H"
#include	"SHAPE.H"
#include	"TIMER.H"

/*
**	Interface class to the keyboard. This insulates the game from library vagaries. Most
**	notable being the return values are declared as "int" in the library whereas C&C
**	expects it to be of KeyNumType.
*/
//lint -esym(1725,KeyboardClass::MouseQX,KeyboardClass::MouseQY)
struct KeyboardClass : public WWKeyboardClass
{

	/*
	**	This flag is retained for legacy callers, but the supported port
	**	always routes keyboard input through WWKeyboardClass.
	*/
	unsigned IsLibrary;

	KeyboardClass() : IsLibrary(true) {}
	KeyNumType Get(void) {return ((KeyNumType)WWKeyboardClass::Get());};
	KeyNumType Check(void) {return ((KeyNumType)WWKeyboardClass::Check());};
	KeyASCIIType To_ASCII(KeyNumType key) {return((KeyASCIIType)WWKeyboardClass::To_ASCII(key));};
	void Clear(void) {WWKeyboardClass::Clear();};
	int Down(KeyNumType key) {return(WWKeyboardClass::Down(key));};

	int Mouse_X(void) {return(Get_Global_Mouse_X());};
	int Mouse_Y(void) {return(Get_Global_Mouse_Y());};
};


/*
**	These templates allow enumeration types to have simple bitwise
**	arithmatic performed. The operators must be instatiated for the
**	enumerated types desired.
*/
template<class T> inline T operator ++(T & a)
{
	a = (T)((int)a + (int)1);
	return(a);
}
template<class T> inline T operator ++(T & a, int)
{
	T aa = a;
	a = (T)((int)a + (int)1);
	return(aa);
}
template<class T> inline T operator --(T & a)
{
	a = (T)((int)a - (int)1);
	return(a);
}
template<class T> inline T operator --(T & a, int)
{
	T aa = a;
	a = (T)((int)a - (int)1);
	return(aa);
}
template<class T> inline T operator |(T t1, T t2)
{
	return((T)((int)t1 | (int)t2));
}
template<class T> inline T operator &(T t1, T t2)
{
	return((T)((int)t1 & (int)t2));
}
template<class T> inline T operator ~(T t1)
{
	return((T)(~(int)t1));
}


template<class T> inline void swap(T &value1, T &value2)
{
	T temp = value1;
	value1 = value2;
	value2 = temp;
}
int swap(int, int);
long swap(long, long);

template<class T> inline
T Bound(T original, T minval, T maxval)
{
	if (original < minval) return(minval);
	if (original > maxval) return(maxval);
	return(original);
};
inline int Bound(signed int original, signed int minval, signed int maxval)
{
	return Bound<signed int>(original, minval, maxval);
}
inline unsigned Bound(unsigned original, unsigned minval, unsigned maxval)
{
	return Bound<unsigned>(original, minval, maxval);
}
inline long Bound(long original, long minval, long maxval)
{
	return Bound<long>(original, minval, maxval);
}

template<class T>
T _rotl(T X, int n)
{
	return((T)(( ( ( X ) << n ) | ( ( X ) >> ( (sizeof(T)*8) - n ) ) )));
}


/*
**	This macro serves as a general way to determine the number of elements
**	within an array.
*/
#define	ARRAY_LENGTH(x)	int(sizeof(x)/sizeof(x[0]))
#define	ARRAY_SIZE(x)		int(sizeof(x)/sizeof(x[0]))


/*
**	The shape flags are likely to be "or"ed together and other such bitwise
**	manipulations. These instatiated operator templates allow this.
*/
inline ShapeFlags_Type operator |(ShapeFlags_Type lhs, ShapeFlags_Type rhs)
{
	return (ShapeFlags_Type)((int)lhs | (int)rhs);
}
inline ShapeFlags_Type operator &(ShapeFlags_Type lhs, ShapeFlags_Type rhs)
{
	return (ShapeFlags_Type)((int)lhs & (int)rhs);
}
inline ShapeFlags_Type operator ~(ShapeFlags_Type value)
{
	return (ShapeFlags_Type)(~(int)value);
}


void Set_Bit(void * array, int bit, int value);
#pragma aux Set_Bit parm [esi] [ecx] [eax] \
	modify [esi ebx] = 			\
	"mov	ebx,ecx"					\
	"shr	ebx,5"					\
	"and	ecx,01Fh"				\
	"btr	[esi+ebx*4],ecx"		\
	"or	eax,eax"					\
	"jz	ok"						\
	"bts	[esi+ebx*4],ecx"		\
	"ok:"

int Get_Bit(void const * array, int bit);
#pragma aux Get_Bit parm [esi] [eax] \
	modify [esi ebx] \
	value [eax]		= 				\
	"mov	ebx,eax"					\
	"shr	ebx,5"					\
	"and	eax,01Fh"				\
	"bt	[esi+ebx*4],eax"		\
	"setc	al"

int First_True_Bit(void const * array);
#pragma aux First_True_Bit parm [esi] \
	modify [esi ebx] \
	value [eax]		= 				\
	"mov	eax,-32"					\
	"again:"							\
	"add	eax,32"					\
	"mov	ebx,[esi]"				\
	"add	esi,4"					\
	"bsf	ebx,ebx"					\
	"jz	again"					\
	"add	eax,ebx"

int First_False_Bit(void const * array);
#pragma aux First_False_Bit parm [esi] \
	modify [esi ebx] \
	value [eax]		= 				\
	"mov	eax,-32"					\
	"again:"							\
	"add	eax,32"					\
	"mov	ebx,[esi]"				\
	"not	ebx"						\
	"add	esi,4"					\
	"bsf	ebx,ebx"					\
	"jz	again"					\
	"add	eax,ebx"

#ifdef OBSOLETE
extern int Bound(int original, int min, int max);
#pragma aux Bound parm [eax] [ebx] [ecx] \
	modify [eax] \
	value [eax]		= 				\
	"cmp	ebx,ecx"					\
	"jl	okorder"					\
	"xchg	ebx,ecx"					\
	"okorder: cmp	eax,ebx"		\
	"jg	okmin"					\
	"mov	eax,ebx"					\
	"okmin: cmp	eax,ecx"			\
	"jl	okmax"					\
	"mov	eax,ecx"					\
	"okmax:"

extern unsigned Bound(unsigned original, unsigned min, unsigned max);
#pragma aux Bound parm [eax] [ebx] [ecx] \
	modify [eax] \
	value [eax]		= 				\
	"cmp	ebx,ecx"					\
	"jb	okorder"					\
	"xchg	ebx,ecx"					\
	"okorder: cmp	eax,ebx"		\
	"ja	okmin"					\
	"mov	eax,ebx"					\
	"okmin: cmp	eax,ecx"			\
	"jb	okmax"					\
	"mov	eax,ecx"					\
	"okmax:"
#endif


unsigned Fixed_To_Cardinal(unsigned base, unsigned fixed);
#pragma aux Fixed_To_Cardinal parm [eax] [edx] \
	modify [edx] \
	value [eax]		= 				\
	"mul	edx"						\
	"add	eax,080h"				\
	"test	eax,0FF000000h"		\
	"jz	ok"						\
	"mov	eax,000FFFFFFh"		\
	"ok:"								\
	"shr	eax,8"


unsigned Cardinal_To_Fixed(unsigned base, unsigned cardinal);
#pragma aux Cardinal_To_Fixed parm [ebx] [eax] \
	modify [edx] \
	value [eax]		= 				\
	"or	ebx,ebx"					\
	"jz	fini"						\
	"shl	eax,8"					\
	"xor	edx,edx"					\
	"div	ebx"						\
	"fini:"


#ifndef OUTPORTB
#define OUTPORTB
extern void outportb(int port, unsigned char data);
#pragma aux outportb parm [edx] [al] =		\
		"out	dx,al"

extern void outport(int port, unsigned short data);
#pragma aux outport parm [edx] [ax] =		\
		"out	dx,al"		\
		"inc	dx"			\
		"mov	al,ah"		\
		"out	dx,al"
#endif


/*
**	Timer objects that fetch the appropriate timer value according to
**	the type of timer they are.
*/
extern int32_t Frame;
#ifndef FRAME_TIMER_CLASS_DEFINED
#define FRAME_TIMER_CLASS_DEFINED
class FrameTimerClass
{
	public:
		int32_t operator () (void) const {return(Frame);};
		operator int32_t (void) const {return(Frame);};
};
#endif


extern WinTimerClass	*		WindowsTimer;

inline long SystemTimerClass::operator () (void) const {if (!WindowsTimer) return(0);return(WindowsTimer->Get_System_Tick_Count());}
inline SystemTimerClass::operator long (void) const {if (!WindowsTimer) return(0);return(WindowsTimer->Get_System_Tick_Count());}


class UserTimerClass
{
	public:
			long operator () (void) const {if (!WindowsTimer) return(0);return(WindowsTimer->Get_User_Tick_Count());};
			operator long (void) const {if (!WindowsTimer) return(0);return(WindowsTimer->Get_User_Tick_Count());};
};


template<class T>
void Bubble_Sort(T * array, int count)
{
	if (array != NULL && count > 1) {
		bool swapflag;

		do {
			swapflag = false;
			for (int index = 0; index < count-1; index++) {
				if (array[index] > array[index+1]) {
					T temp = array[index];
					array[index] = array[index+1];
					array[index+1] = temp;
					swapflag = true;
				}
			}
		} while (swapflag);
	}
}

template<class T>
void PBubble_Sort(T * array, int count)
{
	if (array != NULL && count > 1) {
		bool swapflag;

		do {
			swapflag = false;
			for (int index = 0; index < count-1; index++) {
				if (*array[index] > *array[index+1]) {
					T temp = array[index];
					array[index] = array[index+1];
					array[index+1] = temp;
					swapflag = true;
				}
			}
		} while (swapflag);
	}
}

template<class T>
void PNBubble_Sort(T * array, int count)
{
	if (array != NULL && count > 1) {
		bool swapflag;

		do {
			swapflag = false;
			for (int index = 0; index < count-1; index++) {
				if (SDL_strcasecmp(array[index]->Name(), array[index+1]->Name()) > 0) {
					T temp = array[index];
					array[index] = array[index+1];
					array[index+1] = temp;
					swapflag = true;
				}
			}
		} while (swapflag);
	}
}

#endif
