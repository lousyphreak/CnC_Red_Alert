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
 *                 Project Name : Timer Class Functions                    *
 *                                                                         *
 *                    File Name : TIMER.H                                  *
 *                                                                         *
 *                   Programmer : Scott K. Bowen                           *
 *                                                                         *
 *                   Start Date : July 6, 1994                             *
 *                                                                         *
 *                  Last Update : July 12, 1994   [SKB]                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef TIMER_H
#include <stdint.h>
#define TIMER_H

/*=========================================================================*/
/* The following prototypes are for the file: TIMERA.ASM							*/
/*=========================================================================*/

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// Externs /////////////////////////////////////////////
extern int32_t TimerSystemOn;


/*=========================================================================*/
typedef enum BaseTimerEnum {
	BT_SYSTEM,			// System timer (60 / second).
	BT_USER				// User controllable timer (? / second).
} BaseTimerEnum;

class TimerClass {
 	public:
		// Constructor.  Timers set before low level init has been done will not
		// be able to be 'Started' or 'on' until timer system is in place.	
		TimerClass(BaseTimerEnum timer=BT_SYSTEM, int32_t start=FALSE);

		// No destructor.
		~TimerClass(void){}

		// 
		int32_t Set(int32_t value, int32_t start=TRUE);	// Set initial timer value.
		int32_t Stop(void);				// Pause timer.
		int32_t Start(void);				// Resume timer.
		int32_t Reset(int32_t start=TRUE);	// Reset timer to zero.
		int32_t Time(void);				// Fetch current timer value.

	protected:
		int32_t Started;					// Time last started (0 == not paused).
		int32_t Accumulated;				//	Total accumulated ticks.

	private:
		int32_t (*Get_Ticks)(void);	// System timer fetch.
};


inline int32_t TimerClass::Reset(int32_t start)
{
	return(Set(0, start));
}


class CountDownTimerClass : private TimerClass {
	public:
		// Constructor.  Timers set before low level init has been done will not
		// be able to be 'Started' or 'on' until timer system is in place.	
		CountDownTimerClass(BaseTimerEnum timer, int32_t set, int on=FALSE);
		CountDownTimerClass(BaseTimerEnum timer=BT_SYSTEM, int on=FALSE);

		// No destructor.
		~CountDownTimerClass(void){}

		// Public functions
		int32_t Set(int32_t set, int32_t start=TRUE);	// Set count down value.
		int32_t Reset(int32_t start=TRUE);	// Reset timer to zero.
		int32_t Stop(void);			// Pause timer.
		int32_t Start(void);			// Resume timer.
		int32_t Time(void);			// Fetch current count down value.

	protected:
		int32_t DelayTime;			// Ticks remaining before countdown timer expires.
};

inline int32_t CountDownTimerClass::Stop(void)
{
	TimerClass::Stop();
	return(Time());
}

inline int32_t CountDownTimerClass::Start(void)
{
	TimerClass::Start();
	return(Time());
}

inline int32_t CountDownTimerClass::Reset(int32_t start)
{
	return (TimerClass::Reset(start));
}



//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// externs  //////////////////////////////////////////

extern TimerClass					TickCount;
extern CountDownTimerClass		CountDown;

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////// Prototypes //////////////////////////////////////////

extern "C" {
	int32_t Get_System_Tick_Count(void);
	int32_t Get_User_Tick_Count(void);
	void  Timer_Interrupt_Func(void);
//	int32_t Get_Num_Interrupts(uint32_t realmode);
}

/*=========================================================================*/
/* The following prototypes are for the file: TIMER.CPP							*/
/*=========================================================================*/
int32_t Init_Timer_System(uint32_t freq, int partial = FALSE);
int32_t Remove_Timer_System(void);


#endif // TIMER_H


