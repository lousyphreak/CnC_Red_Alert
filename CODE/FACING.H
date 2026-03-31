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

/* $Header: /CounterStrike/FACING.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : FACING.H                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 03/21/95                                                     *
 *                                                                                             *
 *                  Last Update : March 21, 1995 [JLB]                                         *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef FACING_H
#define FACING_H

#include "FACE.H"

/*
**	This is a general facing handler class. It is used in those cases where facing needs to be
**	kept track of, but there could also be an associated desired facing. The current facing
**	is supposed to transition to the desired state over time. Using this class facilitates this
**	processing as well as isolating the rest of the code from the internals.
*/
class FacingClass
{
	public:
		FacingClass(void);
		FacingClass(DirType dir) : CurrentFacing(Normalize_Dir(dir)), DesiredFacing(Normalize_Dir(dir)) {};
		FacingClass(NoInitClass const & ) {};

		operator DirType (void) const {return(Current());};

		DirType Current(void) const {return(Normalize_Dir(CurrentFacing));};
		DirType Desired(void) const {return(Normalize_Dir(DesiredFacing));};

		int Set_Desired(DirType facing);
		int Set_Current(DirType facing);

		void Set(DirType facing) {
			Set_Current(facing);
			Set_Desired(facing);
		};

		DirType Get(void) const { return(Current()); }

		int Is_Rotating(void) const {return (Desired() != Current());};
		int Difference(void) const {return (int)(signed char)((int)Desired() - (int)Current());}
		int Difference(DirType facing) const {return (int)(signed char)((int)Normalize_Dir(facing) - (int)Current());}
		int Rotation_Adjust(int rate);

	private:
		DirType CurrentFacing;
		DirType DesiredFacing;
};


#endif
