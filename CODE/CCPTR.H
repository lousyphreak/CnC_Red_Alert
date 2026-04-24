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

/* $Header: /CounterStrike/CCPTR.H 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : CCPTR.H                                                      *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 06/07/96                                                     *
 *                                                                                             *
 *                  Last Update : June 7, 1996 [JLB]                                           *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#ifndef CCPTR_H
#define CCPTR_H
#include <stdint.h>

#include	"NOINIT.H"
#include	"HEAP.H"

class AircraftClass;
class AircraftTypeClass;
class AnimClass;
class AnimTypeClass;
class BuildingClass;
class BuildingTypeClass;
class BulletClass;
class BulletTypeClass;
class FactoryClass;
class HouseClass;
class HouseTypeClass;
class InfantryClass;
class InfantryTypeClass;
class OverlayClass;
class OverlayTypeClass;
class SmudgeClass;
class SmudgeTypeClass;
class TeamClass;
class TeamTypeClass;
class TemplateClass;
class TemplateTypeClass;
class TerrainClass;
class TerrainTypeClass;
class TriggerClass;
class TriggerTypeClass;
class UnitTypeClass;
class VesselTypeClass;

/*
**	The CCPtr class is designed for a specific purpose. It functions like a pointer except that
**	it requires no fixups for saving and loading. If pointer fixups are not an issue, than using
**	regular pointers would be more efficient.
*/
template<class T>
class CCPtr
{
	public:
		CCPtr(void) : ID(-1) {};
		CCPtr(NoInitClass const & ) {};
		CCPtr(T * ptr);

		operator T * (void) const {
			if (ID == -1) return(NULL);
			assert(Heap != NULL && (unsigned)ID < Heap->Length());
			return((T*) (*Heap)[ID]);
		}
		T & operator * (void) const {
			assert(Heap != NULL && (unsigned)ID < Heap->Length());
			return(*(T*)(*Heap)[ID]);
		}
		T * operator -> (void) const {
			if (ID == -1) return(NULL);
			assert(Heap != NULL && (unsigned)ID < Heap->Length());
			return((T*) (*Heap)[ID]);
		}

		bool Is_Valid(void) const {return(ID != -1);}

		bool operator == (CCPtr<T> const & rvalue) const {return(ID == rvalue.ID);}
		bool operator != (CCPtr<T> const & rvalue) const {return(ID != rvalue.ID);}
		bool operator > (CCPtr<T> const & rvalue) const;
		bool operator <= (CCPtr<T> const & rvalue) const {return (rvalue > *this);}
		bool operator < (CCPtr<T> const & rvalue) const {return (*this != rvalue && rvalue > *this);}
		bool operator >= (CCPtr<T> const & rvalue) const {return (*this == rvalue || rvalue > *this);}

		int32_t Raw(void) const {return(ID);}
		void Set_Raw(int32_t value) {ID = value;}

	private:

		static FixedIHeapClass * Heap;

		/*
		**	This is the ID number of the object it refers to. By using an ID number, this class can
		**	be saved and loaded without pointer fixups.
		*/
		int ID;
};

template<> FixedIHeapClass * CCPtr<AircraftClass>::Heap;
template<> FixedIHeapClass * CCPtr<AircraftTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<AnimClass>::Heap;
template<> FixedIHeapClass * CCPtr<AnimTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<BuildingClass>::Heap;
template<> FixedIHeapClass * CCPtr<BuildingTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<BulletClass>::Heap;
template<> FixedIHeapClass * CCPtr<BulletTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<FactoryClass>::Heap;
template<> FixedIHeapClass * CCPtr<HouseClass>::Heap;
template<> FixedIHeapClass * CCPtr<HouseTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<InfantryClass>::Heap;
template<> FixedIHeapClass * CCPtr<InfantryTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<OverlayClass>::Heap;
template<> FixedIHeapClass * CCPtr<OverlayTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<SmudgeClass>::Heap;
template<> FixedIHeapClass * CCPtr<SmudgeTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<TeamClass>::Heap;
template<> FixedIHeapClass * CCPtr<TeamTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<TemplateClass>::Heap;
template<> FixedIHeapClass * CCPtr<TemplateTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<TerrainClass>::Heap;
template<> FixedIHeapClass * CCPtr<TerrainTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<TriggerClass>::Heap;
template<> FixedIHeapClass * CCPtr<TriggerTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<UnitTypeClass>::Heap;
template<> FixedIHeapClass * CCPtr<VesselTypeClass>::Heap;

/*
**	These template helper functions tell the compiler what to do in the
**	ambiguous case of a CCPtr on one side and a regular type pointer on the
**	other side. In such a case the compiler could create a temp CCPtr object
**	OR call the conversion operator on the existing CCPtr object. Either way
**	is technically valid, but the compiler doesn't know which is better so it
**	generates an error. These routines force the conversion operator rather than
**	creating a temporary object. This presumes that the conversion operator is
**	cheaper than constructing a temporary and that cheaper solutions are desirable.
*/
template<class T>
int operator == (CCPtr<T> & lvalue, T * rvalue)
{
	return((T*)lvalue == rvalue);
}

template<class T>
int operator == (T * lvalue, CCPtr<T> & rvalue)
{
	return(lvalue == (T*)rvalue);
}

#endif
