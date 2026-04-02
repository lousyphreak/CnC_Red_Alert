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
 *                 Project Name : Temp timer for 32bit lib                 *
 *                                                                         *
 *                    File Name : TIMERINI.CPP                             *
 *                                                                         *
 *                   Programmer : Scott K. Bowen                           *
 *                                                                         *
 *                   Start Date : July 6, 1994                             *
 *                                                                         *
 *                  Last Update : July 6, 1994   [SKB]                     *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   Init_Timer_System -- Initialize the WW timer system.                  *
 *   Remove_Timer_System -- Removes the timer system.                      *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include <stdint.h>

#include <SDL3/SDL_timer.h>

#include <WWSTD.H>
#include "TIMER.H"
#include <PROFILE.H>
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Defines /////////////////////////////////////

#define COPY_FROM_MEM	TRUE

/////////////////////////////////////////////////////////////////////////////////
////////////////////////////// timera.asm functions//////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

extern BOOL Install_Timer_Interrupt(VOID *bin_ptr, UINT rm_size, UINT freq, BOOL partial);
extern BOOL Remove_Timer_Interrupt(VOID);

#ifdef __cplusplus
}
#endif

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Global Data /////////////////////////////////////

BOOL	TimerSystemOn	= FALSE;

// Global timers that the library or user can count on existing.
TimerClass					WWTickCount(BT_SYSTEM);
CountDownTimerClass		CountDown(BT_SYSTEM, 0, FALSE);


// Prototype for timer callback
void CALLBACK Timer_Callback ( UINT event_id, UINT res1 , DWORD user, DWORD  res2, DWORD  res3 );

HANDLE	TimerThreadHandle = 0;		// Legacy callback thread handle (unused on SDL path)
int		InTimerCallback	= 0;		//Flag to say if we are currently in a callback

static unsigned Timer_Ticks_From_Nanos(uint64_t elapsed_nanos, unsigned frequency)
{
	if (frequency == 0) {
		return 0;
	}

	uint64_t ticks = (elapsed_nanos * static_cast<uint64_t>(frequency)) / SDL_NS_PER_SECOND;
	if (ticks > static_cast<uint64_t>(0xffffffffu)) {
		ticks = static_cast<uint64_t>(0xffffffffu);
	}

	return static_cast<unsigned>(ticks);
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Code ////////////////////////////////////////





/***************************************************************************
 * WinTimerClass::WinTimerClass -- Initialize the WW timer system.         *
 *                                                                         *
 *                                                                         *
 * INPUT: UINT : user timer frequency.												  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/5/95 3:47PM : ST Created.                                          *
 *=========================================================================*/
WinTimerClass::WinTimerClass (UINT freq, BOOL partial)
{
	BOOL		success;

#ifdef __SW_EP
	Frequency = PROFILE_RATE;
#else
	Frequency = freq;
#endif
	TrueRate = Frequency;
	UserRate = Frequency;
	SysTicks = 0;
	UserTicks = 0;
	StartTickNanos = SDL_GetTicksNS();
	TimerHandle = 1;
	TimerSystemOn = success = (Frequency != 0);

	if (success)  {
		if (!partial) {
			WindowsTimer=this;
			WWTickCount.Start();
		}
	}else{
		char error_str [128];
		sprintf (error_str, "Error - timer system failed to start. Error code %d\n", GetLastError());
		OutputDebugString(error_str);
	}
}



/***************************************************************************
 * WinTimerClass::~WinTimerClass -- Removes the timer system.              *
 *                                                                         *
 *                                                                         *
 * INPUT:   NONE.                                                          *
 *                                                                         *
 * OUTPUT:  BOOL was it removed successfuly                                *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/5/95 3:47PM : ST Created.                                          *
 *=========================================================================*/
WinTimerClass::~WinTimerClass( void )
{
	if (WindowsTimer == this) {
		WindowsTimer = NULL;
	}

	TimerHandle = 0;
	TimerSystemOn = FALSE;
	TimerThreadHandle = 0;
	InTimerCallback = 0;
}





/***********************************************************************************************
 * Timer_Callback -- Main timer callback. Equivalent to a timer interrupt handler              *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    uint timer ID                                                                     *
 *           uint reserved                                                                     *
 *           long 0 (application defined)                                                      *
 *           long reserved                                                                     *
 *           long reserved                                                                     *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/5/95 3:19PM ST : Created                                                              *
 *=============================================================================================*/


void CALLBACK Timer_Callback (UINT , UINT , DWORD , DWORD , DWORD)
{
	InTimerCallback++;
	if (WindowsTimer) {
		WindowsTimer->Update_Tick_Count();
	}
	InTimerCallback--;
}






/***********************************************************************************************
 * WinTimerClass::Update_Tick_Count -- update westwood timers                                  *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/5/95 3:58PM ST : Created                                                              *
 *=============================================================================================*/

void WinTimerClass::Update_Tick_Count ( void )
{
	SysTicks = Get_System_Tick_Count();
	UserTicks = Get_User_Tick_Count();
}








/*
;***************************************************************************
;* GET_NUM_INTERRUPTS -- Returns the number of interrupts that have occured*
;*                                                                         *
;* INPUT: TRUE - returns num RM ints.                                      *
;*        FALSE - return num PM ints.                                      *
;*                                                                         *
;* OUTPUT:                                                                 *
;*                                                                         *
;* WARNINGS:                                                               *
;*                                                                         *
;* HISTORY:                                                                *
;*   07/12/1994 SKB : Created.                                             *
;*=========================================================================*
	PROC	Get_Num_Interrupts C Near
	USES	esi
	ARG	realmode:DWORD

	mov	esi,[RealModePtr]
	cmp	[realmode],0
	je	??prot_mode
	mov	eax,[(TimerType PTR esi).NumRMInts]
	ret
??prot_mode:
	mov	eax,[(TimerType PTR esi).NumPMInts]
	ret

	ENDP
  */




/***********************************************************************************************
 * WinTimerClass::Get_System_Tick_Count -- returns the system tick count                       *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   tick count                                                                        *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/5/95 4:02PM ST : Created                                                              *
 *=============================================================================================*/

unsigned WinTimerClass::Get_System_Tick_Count ( void )
{
	SysTicks = Timer_Ticks_From_Nanos(SDL_GetTicksNS() - StartTickNanos, Frequency);
	return ( SysTicks );
}



/***********************************************************************************************
 * WinTimerClass::Get_User_Tick_Count -- returns the user tick count                           *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   tick count                                                                        *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/5/95 4:02PM ST : Created                                                              *
 *=============================================================================================*/

unsigned WinTimerClass::Get_User_Tick_Count ( void )
{
	UserTicks = Timer_Ticks_From_Nanos(SDL_GetTicksNS() - StartTickNanos, UserRate);
	return ( UserTicks );
}
