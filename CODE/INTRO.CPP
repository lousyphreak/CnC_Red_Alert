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

/* $Header: /CounterStrike/INTRO.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : INTRO.H                                                      *
 *                                                                                             *
 *                   Programmer : Barry W. Green                                               *
 *                                                                                             *
 *                   Start Date : May 8, 1995                                                  *
 *                                                                                             *
 *                  Last Update : May 8, 1995  [BWG]                                           *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"

VQAHandle * Open_Movie(char * name);
VQAHandle * Open_Movie(char * name)
{
	if (!Debug_Quiet) {
		AnimControl.OptionFlags |= VQAOPTF_AUDIO;
	} else {
		AnimControl.OptionFlags &= ~VQAOPTF_AUDIO;
	}

	VQAHandle * vqa = VQA_Alloc();
	if (vqa) {
		VQA_Init(vqa, MixFileHandler);

		if (VQA_Open(vqa, name, &AnimControl) != 0) {
			VQA_Free(vqa);
			vqa = 0;
		}
	}
	return(vqa);
}


/***********************************************************************************************
 * Choose_Side -- play the introduction movies, select house                                   *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   5/08/1995 BWG : Created.                                                                  *
 *=============================================================================================*/
bool Choose_Side(void)
{
	Whom = HOUSE_GOOD;
	bool allied_campaign = true;

	if (Special.IsFromInstall) {
#if RA_ENABLE_DVD_MOVIES
		Hide_Mouse();
		Load_Title_Page();
		GamePalette = CCPalette;
		HidPage.Blit(SeenPage);
		CCPalette.Set();
		Set_Logic_Page(SeenBuff);
		Show_Mouse();

		if (WWMessageBox().Process(TXT_CHOOSE, TXT_ALLIES, TXT_SOVIET) == 1) {
			allied_campaign = false;
			Whom = HOUSE_BAD;
		}

		Hide_Mouse();
		BlackPalette.Set(FADE_PALETTE_SLOW);
		SeenPage.Clear();
#endif
		Play_Movie(VQ_INTRO_MOVIE, THEME_NONE, false);
	}
	return(allied_campaign);
}
