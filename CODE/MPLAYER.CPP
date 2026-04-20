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

/* $Header: /CounterStrike/MPLAYER.CPP 3     3/13/97 2:06p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MPLAYER.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Bill Randolph                                                *
 *                                                                                             *
 *                   Start Date : April 14, 1995                                               *
 *                                                                                             *
 *                  Last Update : November 30, 1995 [BRR]                                      *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Select_MPlayer_Game -- prompts user for skirmish or network game                           *
 *   Clear_Listbox -- clears the given list box                                                *
 *   Clear_Vector -- clears the given NodeNameType vector                                      *
 *   Computer_Message -- "sends" a message from the computer                                   *
 *   Garble_Message -- "garbles" a message                                                     *
 *   Surrender_Dialog -- Prompts user for surrendering                                         *
 *   Abort_Dialog -- Prompts user for confirmation on aborting the mission							  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "FUNCTION.H"
extern bool Is_Mission_Counterstrike (char *file_name);

#ifdef WOLAPI_INTEGRATION
#include "WOLSTRNG.H"
#endif

/***********************************************************************************************
 * Select_MPlayer_Game -- prompts user for NULL-Modem, Modem, or Network game                  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      GAME_NORMAL, GAME_UDP, etc.                                                            *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
GameType Select_MPlayer_Game (void)
{
	const bool udp_enabled = Udp.Is_UDP() != 0;
#if RA_ENABLE_TCP
	const bool tcp_enabled = true;
#else
	const bool tcp_enabled = false;
#endif

	//------------------------------------------------------------------------
	//	Dialog & button dimensions
	//------------------------------------------------------------------------
	int d_dialog_w = 190 *RESFACTOR;
#ifdef WOLAPI_INTEGRATION
#if RA_ENABLE_TCP
	int d_dialog_h = 100 * RESFACTOR;			//	ajw
#else
	int d_dialog_h = 89 * RESFACTOR;			//	ajw
#endif
	int	d_dialog_y = (((255 * RESFACTOR) - d_dialog_h) / 2);
#else
#if RA_ENABLE_TCP
	int d_dialog_h = 89 *RESFACTOR;
	int d_dialog_y = 84 * RESFACTOR;
#else
	int d_dialog_h = 78 *RESFACTOR;
	int d_dialog_y = 90 * RESFACTOR;
#endif
#endif
	int d_dialog_x = (((320*RESFACTOR) - d_dialog_w) / 2);
	int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);

	int d_txt6_h = 7 *RESFACTOR;
	int d_margin = 7 *RESFACTOR;

	int d_skirmish_w = 80 *RESFACTOR;
	int d_skirmish_h = 9 *RESFACTOR;
	int d_skirmish_x = d_dialog_cx - d_skirmish_w / 2;
	int d_skirmish_y = d_dialog_y + d_margin + d_txt6_h + d_margin;

	int d_udp_w = 80 *RESFACTOR;
	int d_udp_h = 9 *RESFACTOR;
	int d_udp_x = d_dialog_cx - d_udp_w / 2;
	int d_udp_y = d_skirmish_y + d_skirmish_h + 2*RESFACTOR;

#if RA_ENABLE_TCP
	int d_tcp_w = 80 *RESFACTOR;
	int d_tcp_h = 9 *RESFACTOR;
	int d_tcp_x = d_dialog_cx - d_tcp_w / 2;
	int d_tcp_y = d_udp_y + d_udp_h + 2*RESFACTOR;
#endif

#if RA_ENABLE_WOL
	int d_wol_w = 80 * RESFACTOR;
	int d_wol_h = 9 * RESFACTOR;
	int d_wol_x = d_dialog_cx - d_wol_w / 2;
#if RA_ENABLE_TCP
	int d_wol_y = d_tcp_y + d_tcp_h + 2*RESFACTOR;
#else
	int d_wol_y = d_udp_y + d_udp_h + 2*RESFACTOR;
#endif
	// Enlarge the dialog to accommodate the extra button.
	d_dialog_h += d_wol_h + 2*RESFACTOR;
#endif

	int d_cancel_w = 60 *RESFACTOR;
	int d_cancel_h = 9 *RESFACTOR;
	int d_cancel_x = d_dialog_cx - d_cancel_w / 2;
#if RA_ENABLE_WOL
	int d_cancel_y = d_wol_y + d_wol_h + d_margin;
#else
#if RA_ENABLE_TCP
	int d_cancel_y = d_tcp_y + d_tcp_h + d_margin;
#else
	int d_cancel_y = d_udp_y + d_udp_h + d_margin;
#endif
#endif

	GraphicBufferClass seen_buff_save(VisiblePage.Get_Width(), VisiblePage.Get_Height(), (void*)NULL);

	//------------------------------------------------------------------------
	//	Button enumerations:
	//------------------------------------------------------------------------
	enum {
		BUTTON_SKIRMISH = 100,
		BUTTON_UDP,
#if RA_ENABLE_TCP
		BUTTON_TCP,
#endif
#if RA_ENABLE_WOL
		BUTTON_WOL,
#endif
		BUTTON_CANCEL,

		NUM_OF_BUTTONS = 2
#if RA_ENABLE_TCP
		+ 1
#endif
#if RA_ENABLE_WOL
		+ 1
#endif
		+ 1,
	};

	int num_of_buttons = NUM_OF_BUTTONS - (udp_enabled ? 0 : 1);
	//------------------------------------------------------------------------
	//	Redraw values: in order from "top" to "bottom" layer of the dialog
	//------------------------------------------------------------------------
	typedef enum {
		REDRAW_NONE = 0,
		REDRAW_BUTTONS,		// includes map interior & coord values
		REDRAW_BACKGROUND,	// includes box, map bord, key, coord labels, btns
		REDRAW_ALL = REDRAW_BACKGROUND
	} RedrawType;

	//------------------------------------------------------------------------
	//	Dialog variables:
	//------------------------------------------------------------------------
	int input;										// input from user
	bool process;									// loop while true
	RedrawType display;							// true = re-draw everything
	GameType retval;								// return value
	int selection;
	bool pressed;
	int curbutton;
	TextButtonClass * buttons[NUM_OF_BUTTONS];
	int button_ids[NUM_OF_BUTTONS];

	//------------------------------------------------------------------------
	//	Buttons
	//------------------------------------------------------------------------
	ControlClass * commands = NULL;				// the button list

	//------------------------------------------------------------------------
	// The legacy serial-only fallback was removed with the modem cleanup.
	//------------------------------------------------------------------------
//	if ( !Udp.Is_UDP() ) {
//		return( Select_Serial_Dialog() );
//	}

	TextButtonClass skirmishbtn (BUTTON_SKIRMISH, TXT_SKIRMISH, TPF_BUTTON,
		d_skirmish_x, d_skirmish_y, d_skirmish_w, d_skirmish_h);

	TextButtonClass udpbtn (BUTTON_UDP, TXT_NETWORK, TPF_BUTTON,
		d_udp_x, d_udp_y, d_udp_w, d_udp_h);

#if RA_ENABLE_TCP
	if (!udp_enabled) {
		d_tcp_y = d_udp_y;
#ifdef WOLAPI_INTEGRATION
		d_wol_y = d_tcp_y + d_tcp_h + 2*RESFACTOR;
		d_cancel_y = d_wol_y + d_wol_h + d_margin;
#else
		d_cancel_y = d_tcp_y + d_tcp_h + d_margin;
#endif
		d_dialog_h -= d_udp_h + 2*RESFACTOR;
	}

	TextButtonClass tcpbtn (BUTTON_TCP, TXT_INTERNET, TPF_BUTTON,
		d_tcp_x, d_tcp_y, d_tcp_w, d_tcp_h);
#endif

#if RA_ENABLE_WOL
	TextButtonClass wolbtn(BUTTON_WOL, TXT_INTERNET, TPF_BUTTON,
		d_wol_x, d_wol_y, d_wol_w, d_wol_h);
#endif

	if(!udp_enabled && !tcp_enabled) {
		d_cancel_y = d_udp_y;
		d_dialog_h -= d_cancel_h;
	}

	TextButtonClass cancelbtn (BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON,
		d_cancel_x, d_cancel_y, d_cancel_w, d_cancel_h);

	//------------------------------------------------------------------------
	//	Initialize
	//------------------------------------------------------------------------
	Set_Logic_Page(SeenBuff);
	VisiblePage.Blit(seen_buff_save);
	//------------------------------------------------------------------------
	//	Create the list
	//------------------------------------------------------------------------
	commands = &skirmishbtn;
	if(udp_enabled) {
		udpbtn.Add_Tail(*commands);
	}
#if RA_ENABLE_TCP
	if(tcp_enabled) {
		tcpbtn.Add_Tail(*commands);
	}
#endif
#if RA_ENABLE_WOL
	wolbtn.Add_Tail(*commands);
#endif
	cancelbtn.Add_Tail(*commands);

	//------------------------------------------------------------------------
	//	Fill array of button ptrs
	//------------------------------------------------------------------------
	curbutton = 0;
	int button_count = 0;
	buttons[button_count] = &skirmishbtn;
	button_ids[button_count++] = BUTTON_SKIRMISH;
	if(udp_enabled) {
		buttons[button_count] = &udpbtn;
		button_ids[button_count++] = BUTTON_UDP;
	}
#if RA_ENABLE_TCP
	if(tcp_enabled) {
		buttons[button_count] = &tcpbtn;
		button_ids[button_count++] = BUTTON_TCP;
	}
#endif
#if RA_ENABLE_WOL
	buttons[button_count] = &wolbtn;
	button_ids[button_count++] = BUTTON_WOL;
#endif
	buttons[button_count] = &cancelbtn;
	button_ids[button_count++] = BUTTON_CANCEL;
	buttons[curbutton]->Turn_On();

	Keyboard->Clear();

	Fancy_Text_Print(TXT_NONE, 0, 0, GadgetClass::Get_Color_Scheme(), TBLACK,
		TPF_CENTER | TPF_TEXT);

	//------------------------------------------------------------------------
	//	Main Processing Loop
	//------------------------------------------------------------------------
	display = REDRAW_ALL;
	process = true;
	pressed = false;
	while (process) {
			/*
			** If we have just received input focus again after running in the background then
			** we need to redraw.
			*/
			if (AllSurfaces.SurfacesRestored) {
				AllSurfaces.SurfacesRestored=FALSE;
				seen_buff_save.Blit(VisiblePage);
				display = REDRAW_ALL;
			}

		//.....................................................................
		//	Invoke game callback
		//.....................................................................
		Call_Back();

		//.....................................................................
		//	Refresh display if needed
		//.....................................................................
		if (display) {
			Hide_Mouse();
			if (display >= REDRAW_BACKGROUND) {

				//...............................................................
				//	Refresh the backdrop
				//...............................................................
				Load_Title_Page(true);
				CCPalette.Set();

				//...............................................................
				//	Draw the background
				//...............................................................
				Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
				Draw_Caption (TXT_SELECT_MPLAYER_GAME, d_dialog_x, d_dialog_y, d_dialog_w);
			}

			//..................................................................
			//	Redraw buttons
			//..................................................................
			if (display >= REDRAW_BUTTONS) {
				commands->Flag_List_To_Redraw();
			}
			Show_Mouse();
			display = REDRAW_NONE;
		}

		//.....................................................................
		//	Get user input
		//.....................................................................
		input = commands->Input();

		//.....................................................................
		//	Process input
		//.....................................................................
		switch (input) {
			case (BUTTON_SKIRMISH | KN_BUTTON):
				selection = BUTTON_SKIRMISH;
				pressed = true;
				break;

			case (BUTTON_UDP | KN_BUTTON):
				selection = BUTTON_UDP;
				pressed = true;
				break;

#if RA_ENABLE_TCP
			case (BUTTON_TCP | KN_BUTTON):
				selection = BUTTON_TCP;
				pressed = true;
				break;
#endif

#if RA_ENABLE_WOL
			case (BUTTON_WOL | KN_BUTTON):
				selection = BUTTON_WOL;
				pressed = true;
				break;
#endif

			case (KN_ESC):
			case (BUTTON_CANCEL | KN_BUTTON):
				selection = BUTTON_CANCEL;
				pressed = true;
				break;

			case KN_UP:
				buttons[curbutton]->Turn_Off();
				buttons[curbutton]->Flag_To_Redraw();
				curbutton--;
				if (curbutton < 0)
					curbutton = (num_of_buttons - 1);
				buttons[curbutton]->Turn_On();
				buttons[curbutton]->Flag_To_Redraw();
				break;

			case KN_DOWN:
				buttons[curbutton]->Turn_Off();
				buttons[curbutton]->Flag_To_Redraw();
				curbutton++;
				if (curbutton > (num_of_buttons - 1) )
					curbutton = 0;
				buttons[curbutton]->Turn_On();
				buttons[curbutton]->Flag_To_Redraw();
				break;

			case KN_RETURN:
				selection = button_ids[curbutton];
				pressed = true;
				break;

			default:
				break;
		}

		if (pressed) {

			//..................................................................
			// to make sure the selection is correct in case they used the mouse
			//..................................................................
			buttons[curbutton]->Turn_Off();
			buttons[curbutton]->Flag_To_Redraw();
			for (curbutton = 0; curbutton < num_of_buttons; curbutton++) {
				if (button_ids[curbutton] == selection) {
					break;
				}
			}
			if (curbutton >= num_of_buttons) {
				curbutton = 0;
			}
			buttons[curbutton]->Turn_On();
			buttons[curbutton]->IsPressed = true;
			buttons[curbutton]->Draw_Me(true);

			switch (selection) {
				case (BUTTON_SKIRMISH):
					Session.Type = GAME_SKIRMISH;
					if (Com_Scenario_Dialog(true)) {
						retval = GAME_SKIRMISH;
						process = false;
#ifdef FIXIT_VERSION_3
						bAftermathMultiplayer = Is_Aftermath_Installed();
						//	ajw I'll bet this was needed before also...
						Session.ScenarioIsOfficial = Session.Scenarios[Session.Options.ScenarioIndex]->Get_Official();
#endif
					} else {
						buttons[curbutton]->IsPressed = false;
						Session.Type = GAME_NORMAL;
						display = REDRAW_ALL;
					}
					break;

				case (BUTTON_UDP):
					retval = GAME_UDP;
					process = false;
					break;

#if RA_ENABLE_TCP
				case (BUTTON_TCP):
					retval = GAME_INTERNET;
					process = false;
					break;
#endif

#if RA_ENABLE_WOL
				case (BUTTON_WOL):
					retval = GAME_WOL;
					process = false;
					break;
#endif

				case (BUTTON_CANCEL):
					retval = GAME_NORMAL;
					process = false;
					break;
			}

			pressed = false;
		}
		if (process) {
			SDL_Delay(16);
		}
	}
	return(retval);

}	/* end of Select_MPlayer_Game */

/***************************************************************************
 * Clear_Listbox -- clears the given list box                              *
 *                                                                         *
 * This routine assumes the items in the given list box are character		*
 * buffers; it deletes each item in the list, then clears the list.			*
 *                                                                         *
 * INPUT:                                                                  *
 *		list			ptr to listbox															*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   11/29/1995 BRR : Created.                                             *
 *=========================================================================*/
void Clear_Listbox(ListClass * list)
{
	char * item;

	//------------------------------------------------------------------------
	//	Clear the list box
	//------------------------------------------------------------------------
	while (list->Count()) {
		item = (char *)(list->Get_Item(0));
		list->Remove_Item(item);
		delete [] item;
	}
	list->Flag_To_Redraw();

}	// end of Clear_Listbox

/***************************************************************************
 * Clear_Vector -- clears the given NodeNameType vector                    *
 *                                                                         *
 * INPUT:                                                                  *
 *		vector		ptr to vector to clear												*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   11/29/1995 BRR : Created.                                             *
 *=========================================================================*/
void Clear_Vector(DynamicVectorClass <NodeNameType *> * vector)
{
	int i;

	//------------------------------------------------------------------------
	//	Clear the 'Players' Vector
	//------------------------------------------------------------------------
	for (i = 0; i < vector->Count(); i++) {
		delete (*vector)[i];
	}
	vector->Clear();

}	// end of Clear_Vector

/***************************************************************************
 * Computer_Message -- "sends" a message from the computer                 *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   06/06/1995 BRR : Created.                                             *
 *=========================================================================*/
void Computer_Message(void)
{
}	/* end of Computer_Message */


/***************************************************************************
 * Surrender_Dialog -- Prompts user for surrendering                       *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      0 = user cancels, 1 = user wants to surrender.                     *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/05/1995 BRR : Created.                                             *
 *=========================================================================*/
#ifdef FIXIT_VERSION_3		//	Stalemate games.
int Surrender_Dialog(int text)
{
	return Surrender_Dialog( Text_String( text ) );
}
#endif

#ifdef FIXIT_VERSION_3		//	Stalemate games.
int Surrender_Dialog(const char* text)
#else
int Surrender_Dialog(int text)
#endif
{
	//------------------------------------------------------------------------
	//	Dialog & button dimensions
	//------------------------------------------------------------------------
	enum {
		D_DIALOG_W = 240*RESFACTOR,							// dialog width
		D_DIALOG_H = 63*RESFACTOR,								// dialog height
		D_DIALOG_X = ((320*RESFACTOR - D_DIALOG_W) / 2),// centered x-coord
		D_DIALOG_Y = ((200*RESFACTOR - D_DIALOG_H) / 2),// centered y-coord
		D_DIALOG_CX = D_DIALOG_X + (D_DIALOG_W / 2),		// coord of x-center

		D_TXT6_H = 7*RESFACTOR,									// ht of 6-pt text
		D_MARGIN = 5*RESFACTOR,									// margin width/height
		D_TOPMARGIN = 20*RESFACTOR,							// top margin

		D_OK_W = 45*RESFACTOR,									// OK width
		D_OK_H = 9*RESFACTOR,									// OK height
		D_OK_X = D_DIALOG_CX - D_OK_W - 5*RESFACTOR,		// OK x
		D_OK_Y = D_DIALOG_Y + D_DIALOG_H - D_OK_H - D_MARGIN*2,	// OK y

		D_CANCEL_W = 45*RESFACTOR,								// Cancel width
		D_CANCEL_H = 9*RESFACTOR,								// Cancel height
		D_CANCEL_X = D_DIALOG_CX + 5*RESFACTOR,			// Cancel x
		D_CANCEL_Y = D_DIALOG_Y + D_DIALOG_H - D_CANCEL_H - D_MARGIN*2, // Cancel y
	};

	//------------------------------------------------------------------------
	//	Button enumerations
	//------------------------------------------------------------------------
	enum {
		BUTTON_OK = 100,
		BUTTON_CANCEL,
	};

	//------------------------------------------------------------------------
	//	Buttons
	//------------------------------------------------------------------------
	ControlClass * commands = NULL;		// the button list

	TextButtonClass okbtn(BUTTON_OK, TXT_OK, TPF_BUTTON, D_OK_X, D_OK_Y, D_OK_W, D_OK_H);

	TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL, TPF_BUTTON, D_CANCEL_X, D_CANCEL_Y, D_CANCEL_W, D_CANCEL_H);

	int curbutton;
	TextButtonClass * buttons[2];
	curbutton = 0;

	//------------------------------------------------------------------------
	//	Initialize
	//------------------------------------------------------------------------
	Set_Logic_Page(SeenBuff);

	//------------------------------------------------------------------------
	//	Create the button list
	//------------------------------------------------------------------------
	commands = &okbtn;
	cancelbtn.Add_Tail(*commands);

	buttons[0] = &okbtn;
	buttons[1] = &cancelbtn;
	buttons[curbutton]->Turn_On();

	//------------------------------------------------------------------------
	//	Main Processing Loop
	//------------------------------------------------------------------------
	int retcode = 0;
	bool display = true;
	bool process = true;
	while (process) {

		//.....................................................................
		//	Invoke game callback
		//.....................................................................
		if (Session.Type != GAME_SKIRMISH) {
			if (Main_Loop()) {
				retcode = 0;
				process = false;
			}
		}

		//.....................................................................
		//	Refresh display if needed
		//.....................................................................
		if (display) {
			display = false;

			//..................................................................
			//	Display the dialog box
			//..................................................................
			Hide_Mouse();
			Dialog_Box(D_DIALOG_X, D_DIALOG_Y, D_DIALOG_W, D_DIALOG_H);
			Draw_Caption(TXT_NONE, D_DIALOG_X, D_DIALOG_Y, D_DIALOG_W);

			//...............................................................
			//	Draw the captions
			//...............................................................
#ifdef FIXIT_VERSION_3		//	Stalemate games.
			Fancy_Text_Print(text,
				D_DIALOG_CX, D_DIALOG_Y + D_TOPMARGIN,
				GadgetClass::Get_Color_Scheme(), TBLACK,
				TPF_CENTER | TPF_TEXT);
#else
			Fancy_Text_Print(Text_String(text),
				D_DIALOG_CX, D_DIALOG_Y + D_TOPMARGIN,
				GadgetClass::Get_Color_Scheme(), TBLACK,
				TPF_CENTER | TPF_TEXT);
#endif

			//..................................................................
			//	Redraw the buttons
			//..................................................................
			commands->Flag_List_To_Redraw();
			Show_Mouse();
		}

		//.....................................................................
		//	Get user input
		//.....................................................................
		int input = commands->Input();

		//.....................................................................
		//	Process input
		//.....................................................................
		switch (input) {
			case (BUTTON_OK | KN_BUTTON):
				retcode = 1;
				process = false;
				break;

			case (BUTTON_CANCEL | KN_BUTTON):
				retcode = 0;
				process = false;
				break;

			case (KN_RETURN):
				if (curbutton == 0) {
					retcode = 1;
				} else {
					retcode = 0;
				}
				process = false;
				break;

			case (KN_ESC):
				retcode = 0;
				process = false;
				break;

			case (KN_RIGHT):
				buttons[curbutton]->Turn_Off();
				curbutton++;
				if (curbutton > 1) {
					curbutton = 0;
				}
				buttons[curbutton]->Turn_On();
				break;

			case (KN_LEFT):
				buttons[curbutton]->Turn_Off();
				curbutton--;
				if (curbutton < 0) {
					curbutton = 1;
				}
				buttons[curbutton]->Turn_On();
				break;

			default:
				break;
		}
		if (process) {
			SDL_Delay(16);
		}
	}

	//------------------------------------------------------------------------
	//	Redraw the display
	//------------------------------------------------------------------------
	HidPage.Clear();
	Map.Flag_To_Redraw(true);
	Map.Render();

	return (retcode);
}

/***************************************************************************
 * Abort_Dialog -- Prompts user for confirmation on aborting the mission	*
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      1 = user confirms abort, 0 = user cancels									*
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/05/1995 BRR : Created.                                             *
 *=========================================================================*/
int Abort_Dialog(void)
{
	//------------------------------------------------------------------------
	//	Dialog & button dimensions
	//------------------------------------------------------------------------
	enum {
		D_DIALOG_W = 170*RESFACTOR,							// dialog width
		D_DIALOG_H = 63*RESFACTOR,								// dialog height
		D_DIALOG_X = ((320*RESFACTOR - D_DIALOG_W) / 2),// centered x-coord
		D_DIALOG_Y = ((200*RESFACTOR - D_DIALOG_H) / 2),// centered y-coord
		D_DIALOG_CX = D_DIALOG_X + (D_DIALOG_W / 2),		// coord of x-center

		D_TXT6_H = 7*RESFACTOR,									// ht of 6-pt text
		D_MARGIN = 5*RESFACTOR,									// margin width/height
		D_TOPMARGIN = 20*RESFACTOR,							// top margin

		D_YES_W = 45*RESFACTOR,									// YES width
		D_YES_H = 9*RESFACTOR,									// YES height
		D_YES_X = D_DIALOG_CX - D_YES_W - 5*RESFACTOR,	// YES x
		D_YES_Y = D_DIALOG_Y + D_DIALOG_H - D_YES_H - D_MARGIN*2,	// YES y

		D_NO_W = 45*RESFACTOR,									// Cancel width
		D_NO_H = 9*RESFACTOR,									// Cancel height
		D_NO_X = D_DIALOG_CX + 5*RESFACTOR,					// Cancel x
		D_NO_Y = D_DIALOG_Y + D_DIALOG_H - D_NO_H - D_MARGIN*2, // Cancel y
	};

	//------------------------------------------------------------------------
	//	Button enumerations
	//------------------------------------------------------------------------
	enum {
		BUTTON_YES = 100,
		BUTTON_NO,
	};

	//------------------------------------------------------------------------
	//	Buttons
	//------------------------------------------------------------------------
	ControlClass * commands = NULL;		// the button list

	TextButtonClass yesbtn(BUTTON_YES, TXT_YES, TPF_BUTTON, D_YES_X, D_YES_Y, D_YES_W, D_YES_H);

	TextButtonClass nobtn(BUTTON_NO, TXT_NO, TPF_BUTTON, D_NO_X, D_NO_Y, D_NO_W, D_NO_H);

	int curbutton;
	TextButtonClass * buttons[2];
	curbutton = 0;

	//------------------------------------------------------------------------
	//	Initialize
	//------------------------------------------------------------------------
	Set_Logic_Page(SeenBuff);

	//------------------------------------------------------------------------
	//	Create the button list
	//------------------------------------------------------------------------
	commands = &yesbtn;
	nobtn.Add_Tail(*commands);

	buttons[0] = &yesbtn;
	buttons[1] = &nobtn;
	buttons[curbutton]->Turn_On();

	//------------------------------------------------------------------------
	//	Main Processing Loop
	//------------------------------------------------------------------------
	int retcode = 0;
	bool display = true;
	bool process = true;
	while (process) {

		//.....................................................................
		//	Invoke game callback
		//.....................................................................
		if (Session.Type != GAME_SKIRMISH) {
			if (Main_Loop()) {
				retcode = 0;
				process = false;
			}
		}

		//.....................................................................
		//	Refresh display if needed
		//.....................................................................
		if (display) {
			display = false;

			//..................................................................
			//	Display the dialog box
			//..................................................................
			Hide_Mouse();
			Dialog_Box(D_DIALOG_X, D_DIALOG_Y, D_DIALOG_W, D_DIALOG_H);
			Draw_Caption(TXT_NONE, D_DIALOG_X, D_DIALOG_Y, D_DIALOG_W);

			//...............................................................
			//	Draw the captions
			//...............................................................
			Fancy_Text_Print(Text_String(TXT_CONFIRM_EXIT),
				D_DIALOG_CX, D_DIALOG_Y + D_TOPMARGIN,
				GadgetClass::Get_Color_Scheme(), TBLACK,
				TPF_CENTER | TPF_TEXT);

			//..................................................................
			//	Redraw the buttons
			//..................................................................
			commands->Flag_List_To_Redraw();
			Show_Mouse();
		}

		//.....................................................................
		//	Get user input
		//.....................................................................
		int input = commands->Input();

		//.....................................................................
		//	Process input
		//.....................................................................
		switch (input) {
			case (BUTTON_YES | KN_BUTTON):
				retcode = 1;
				process = false;
				break;

			case (BUTTON_NO | KN_BUTTON):
				retcode = 0;
				process = false;
				break;

			case (KN_RETURN):
				if (curbutton == 0) {
					retcode = 1;
				} else {
					retcode = 0;
				}
				process = false;
				break;

			case (KN_ESC):
				retcode = 0;
				process = false;
				break;

			case (KN_RIGHT):
				buttons[curbutton]->Turn_Off();
				curbutton++;
				if (curbutton > 1) {
					curbutton = 0;
				}
				buttons[curbutton]->Turn_On();
				break;

			case (KN_LEFT):
				buttons[curbutton]->Turn_Off();
				curbutton--;
				if (curbutton < 0) {
					curbutton = 1;
				}
				buttons[curbutton]->Turn_On();
				break;

			default:
				break;
		}
		if (process) {
			SDL_Delay(16);
		}
	}

	//------------------------------------------------------------------------
	//	Redraw the display
	//------------------------------------------------------------------------
	HidPage.Clear();
	Map.Flag_To_Redraw(true);
	Map.Render();

	return (retcode);
}

/************************** end of mplayer.cpp *****************************/
