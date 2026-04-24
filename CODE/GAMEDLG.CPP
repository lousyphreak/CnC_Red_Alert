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

/* $Header: /CounterStrike/GAMEDLG.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : GAMEDLG.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Maria del Mar McCready Legg, Joe L. Bostic                   *
 *                                                                                             *
 *                   Start Date : Jan 8, 1995                                                  *
 *                                                                                             *
 *                  Last Update : Jan 18, 1995   [MML]                                         *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   OptionsClass::Process -- Handles all the options graphic interface.                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include	"GAMEDLG.H"
#include "SOUNDDLG.H"
#include "VISUDLG.H"
#define GERMAN_OFFSET_Y   4      //VG

#ifdef WOLAPI_INTEGRATION
#include "WOLSTRNG.H"
extern WolapiObject* pWolapi;
bool WOL_Options_Dialog( WolapiObject* pWO, bool bCalledFromGame );
#endif


/***********************************************************************************************
 * OptionsClass::Process -- Handles all the options graphic interface.                         *
 *                                                                                             *
 *    This routine is the main control for the visual representation of the options            *
 *    screen. It handles the visual overlay and the player input.                              *
 *                                                                                             *
 * INPUT:   none                                                                               *
 * OUTPUT:  none                                                                               *
 * WARNINGS:   none                                                                            *
 * HISTORY:                                                                                    *
 *   12/31/1994 MML : Created.                                                                 *
 *=============================================================================================*/
void GameControlsClass::Process(bool use_title_page)
{
	/*
	**	Dialog & button dimensions
	*/
	int	d_dialog_w = 232 * RESFACTOR;											// dialog width
	int	d_dialog_h = 164 * RESFACTOR;											// dialog height
	int	d_dialog_x = ((SeenBuff.Get_Width() - d_dialog_w) / 2);				// dialog x-coord
	int	d_dialog_y = ((SeenBuff.Get_Height() - d_dialog_h) / 2);				// centered y-coord
	int	d_dialog_cx = d_dialog_x + (d_dialog_w / 2);		// center x-coord
	int	d_top_margin = 25 * RESFACTOR;

	int	d_txt6_h = (6 * RESFACTOR) + 1;												// ht of 6-pt text
	int	d_margin1 = (5 * RESFACTOR);												// large margin

	int	d_speed_w = d_dialog_w - (34 * RESFACTOR);
	int	d_speed_h = 6 * RESFACTOR;
	int	d_speed_x = d_dialog_x + (17 * RESFACTOR);
#ifdef GERMAN
	int	d_speed_y = d_dialog_y + d_top_margin + d_margin1 + d_txt6_h - GERMAN_OFFSET_Y;
#else
	int	d_speed_y = d_dialog_y + d_top_margin + d_margin1 + d_txt6_h;
#endif

	int	d_scroll_w = d_dialog_w - (34 * RESFACTOR);
	int	d_scroll_h = 6 * RESFACTOR;
	int	d_scroll_x = d_dialog_x + (17 * RESFACTOR);
#ifdef GERMAN
	int	d_scroll_y = d_speed_y + d_speed_h + d_txt6_h + (d_margin1 * 2) + d_txt6_h - GERMAN_OFFSET_Y;
#else
	int	d_scroll_y = d_speed_y + d_speed_h + d_txt6_h + (d_margin1 * 2) + d_txt6_h;
#endif

	int	d_mouse_w = d_dialog_w - (34 * RESFACTOR);
	int	d_mouse_h = 6 * RESFACTOR;
	int	d_mouse_x = d_dialog_x + (17 * RESFACTOR);
	int	d_mouse_y = d_scroll_y + d_scroll_h + d_txt6_h + (d_margin1 * 2) + d_txt6_h;

	int	d_visual_w = d_dialog_w - (40 * RESFACTOR);
	int	d_visual_h = 9 * RESFACTOR;
	int	d_visual_x = d_dialog_x + (20 * RESFACTOR);
	int	d_visual_y = d_mouse_y + d_mouse_h + d_txt6_h + (d_margin1 * 2);

	int	d_sound_w = d_dialog_w - (40 * RESFACTOR);
	int	d_sound_h = (9 * RESFACTOR);
	int	d_sound_x = d_dialog_x + (20 * RESFACTOR);
	int	d_sound_y = d_visual_y + d_visual_h + d_margin1;

	int	d_ok_w = 20 * RESFACTOR;
	int	d_ok_h = 9 * RESFACTOR;
	int	d_ok_x = d_dialog_cx - (d_ok_w / 2);
	int	d_ok_y = d_dialog_y + d_dialog_h - d_ok_h - d_margin1 - (4 * RESFACTOR);

#ifdef WOLAPI_INTEGRATION
	int d_wol_x = d_sound_x;
	int d_wol_y = d_sound_y + d_sound_h + d_margin1;
	int d_wol_w = d_sound_w;
	int d_wol_h = d_sound_h;

	bool bShowWolapi = ( pWolapi && !pWolapi->bConnectionDown );
	if( bShowWolapi )
	{
		//	Enlarge dialog and shift ok button down.
		d_dialog_h += d_wol_h + d_margin1;
		d_dialog_y = ((SeenBuff.Get_Height() - d_dialog_h) / 2);				// centered y-coord
		//d_ok_y += d_wol_h + d_margin1;
		d_ok_y = d_dialog_y + d_dialog_h - d_ok_h - d_margin1 - (4 * RESFACTOR);
	}
#endif

	/*
	**	Button Enumerations
	*/
#ifdef WOLAPI_INTEGRATION
	enum {
		BUTTON_SPEED = 100,
		BUTTON_SCROLLRATE,
		BUTTON_MOUSE_ACCEL,
		BUTTON_VISUAL,
		BUTTON_SOUND,
		BUTTON_WOLAPI,
		BUTTON_OK,
		BUTTON_COUNT,
		BUTTON_FIRST = BUTTON_SPEED,
	};
#else
	enum {
		BUTTON_SPEED = 100,
		BUTTON_SCROLLRATE,
		BUTTON_MOUSE_ACCEL,
		BUTTON_VISUAL,
		BUTTON_SOUND,
		BUTTON_OK,
		BUTTON_COUNT,
		BUTTON_FIRST = BUTTON_SPEED,
	};
#endif

	/*
	**	Dialog variables
	*/
	int input;

	int gamespeed = Options.GameSpeed;
	int scrollrate = Options.ScrollRate;
	int mouseaccel = (Options.Get_Mouse_Acceleration() * 2) - 1;
	int selection;
	bool pressed = false;
	int curbutton = 0;
	TextButtonClass *buttons[BUTTON_COUNT - BUTTON_FIRST];
	TextPrintType style;

	RemapControlType * scheme = GadgetClass::Get_Color_Scheme();

	/*
	**	Buttons
	*/
	GadgetClass * commands;										// button list

	SliderClass gspeed_btn(BUTTON_SPEED, d_speed_x, d_speed_y, d_speed_w, d_speed_h, true);
	SliderClass scrate_btn(BUTTON_SCROLLRATE, d_scroll_x, d_scroll_y, d_scroll_w, d_scroll_h, true);
	SliderClass mouseaccel_btn(BUTTON_MOUSE_ACCEL, d_mouse_x, d_mouse_y, d_mouse_w, d_mouse_h, true);
	TextButtonClass visual_btn(BUTTON_VISUAL, TXT_VISUAL_CONTROLS, TPF_BUTTON, d_visual_x, d_visual_y, d_visual_w, d_visual_h);
	TextButtonClass sound_btn(BUTTON_SOUND, TXT_SOUND_CONTROLS, TPF_BUTTON, d_sound_x, d_sound_y, d_sound_w, d_sound_h);
	TextButtonClass okbtn(BUTTON_OK, TXT_OPTIONS_MENU, TPF_BUTTON, d_ok_x, d_ok_y);
	okbtn.X = (SeenBuff.Get_Width()-okbtn.Width)/2;

#ifdef WOLAPI_INTEGRATION
	TextButtonClass wol_btn( BUTTON_WOLAPI, TXT_WOL_OPTTITLE, TPF_BUTTON, d_wol_x, d_wol_y, d_wol_w, d_wol_h );
#endif

	/*
	**	Various Inits.
	*/
	Set_Logic_Page(SeenBuff);

	/*
	**	Build button list
	*/
	commands = &okbtn;
	gspeed_btn.Add_Tail(*commands);
	scrate_btn.Add_Tail(*commands);
	mouseaccel_btn.Add_Tail(*commands);
	visual_btn.Add_Tail(*commands);
	sound_btn.Add_Tail(*commands);
#ifdef WOLAPI_INTEGRATION
	if( bShowWolapi )
		wol_btn.Add_Tail(*commands);
#endif
	/*
	**	Init button states
	**	For sliders, the thumb ranges from 0 - (maxval-1), so to convert the
	**	thumb value to a real-world value:
	**		val = (MAX - slider.Get_Value()) - 1;
	**	and,
	**		slider.Set_Value(-(val + 1 - MAX));
	*/
	gspeed_btn.Set_Maximum(OptionsClass::MAX_SPEED_SETTING);	// varies from 0 - 7
	gspeed_btn.Set_Thumb_Size(1);
	gspeed_btn.Set_Value((OptionsClass::MAX_SPEED_SETTING-1) - gamespeed);

	scrate_btn.Set_Maximum(OptionsClass::MAX_SCROLL_SETTING);	// varies from 0 - 7
	scrate_btn.Set_Thumb_Size(1);
	scrate_btn.Set_Value((OptionsClass::MAX_SCROLL_SETTING-1) - scrollrate);

	if (mouseaccel < 0) {
		mouseaccel = 0;
	}
	if (mouseaccel >= OptionsClass::MAX_MOUSE_ACCEL_SETTING) {
		mouseaccel = OptionsClass::MAX_MOUSE_ACCEL_SETTING - 1;
	}
	mouseaccel_btn.Set_Maximum(OptionsClass::MAX_MOUSE_ACCEL_SETTING);	// varies from 0 - 19
	mouseaccel_btn.Set_Thumb_Size(1);
	mouseaccel_btn.Set_Value(mouseaccel);

	/*
	**	Fill array of button ptrs.
	*/
	buttons[0] = NULL;
	buttons[1] = NULL;
	buttons[2] = NULL;
	buttons[3] = &visual_btn;
	buttons[4] = &sound_btn;
#ifdef WOLAPI_INTEGRATION
	buttons[5] = &wol_btn;
	buttons[6] = &okbtn;
#else
	buttons[5] = &okbtn;
#endif
	/*
	**	Processing loop.
	*/
	bool process = true;
	bool display = true;
	bool refresh = true;
	while (process) {

		/*
		**	Invoke game callback.
		*/
		if (Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH) {
			Call_Back();
		} else {
			if (Main_Loop()) {
				process = false;
			}
		}

		/*
		** If we have just received input focus again after running in the background then
		** we need to redraw.
		*/
		if (AllSurfaces.SurfacesRestored) {
			AllSurfaces.SurfacesRestored=FALSE;
			display = true;
		}
		/*
		**	Refresh display if needed.
		*/
		if (display) {
			Hide_Mouse();

			if (use_title_page) {
				Load_Title_Page();
				CCPalette.Set();
			} else {
				Map.Flag_To_Redraw(true);
				Map.Render();
			}

			Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
			Draw_Caption(TXT_GAME_CONTROLS, d_dialog_x, d_dialog_y, d_dialog_w);
			Show_Mouse();
			display = false;
			refresh = true;
		}

		if (refresh) {
			Hide_Mouse();

			/*
			**	Label the game speed slider
			*/
			style = TPF_TEXT;
			if (curbutton == (BUTTON_SPEED - BUTTON_FIRST)) {
				style = (TextPrintType)(style | TPF_BRIGHT_COLOR);
			}
			Fancy_Text_Print(TXT_SPEED, d_speed_x, d_speed_y - d_txt6_h, scheme, TBLACK, style);

			Fancy_Text_Print(TXT_SLOWER, d_speed_x, d_speed_y + d_speed_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT);
			Fancy_Text_Print(TXT_FASTER, d_speed_x + d_speed_w, d_speed_y + d_speed_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT|TPF_RIGHT);

			/*
			**	Label the scroll rate slider
			*/
			style = TPF_TEXT;
			if (curbutton == (BUTTON_SCROLLRATE - BUTTON_FIRST)) {
				style = (TextPrintType)(style | TPF_BRIGHT_COLOR);
			}
			Fancy_Text_Print(TXT_SCROLLRATE, d_scroll_x, d_scroll_y - d_txt6_h, scheme, TBLACK, style);

			Fancy_Text_Print (TXT_SLOWER, d_scroll_x, d_scroll_y + d_scroll_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT);
			Fancy_Text_Print (TXT_FASTER, d_scroll_x + d_scroll_w, d_scroll_y + d_scroll_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT|TPF_RIGHT);

			/*
			**	Label the mouse acceleration slider.
			*/
			style = TPF_TEXT;
			if (curbutton == (BUTTON_MOUSE_ACCEL - BUTTON_FIRST)) {
				style = (TextPrintType)(style | TPF_BRIGHT_COLOR);
			}
			Fancy_Text_Print(TXT_MOUSE_ACCELERATION, d_mouse_x, d_mouse_y - d_txt6_h, scheme, TBLACK, style);
			Fancy_Text_Print(TXT_SLOWER, d_mouse_x, d_mouse_y + d_mouse_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT);
			Fancy_Text_Print(TXT_FASTER, d_mouse_x + d_mouse_w, d_mouse_y + d_mouse_h + (1 * RESFACTOR), scheme, TBLACK, TPF_TEXT|TPF_RIGHT);
			{
				char mouse_accel_text[32];
				const int32_t mouse_accel_text_width = String_Pixel_Width("x10");
				LogicPage->Fill_Rect(d_mouse_x + d_mouse_w - mouse_accel_text_width - (2 * RESFACTOR),
					d_mouse_y - d_txt6_h,
					d_mouse_x + d_mouse_w,
					d_mouse_y - 1,
					TBLACK);
				sprintf(mouse_accel_text, "x%s", fixed(mouseaccel_btn.Get_Value() + 1, 2).As_ASCII());
				Fancy_Text_Print(mouse_accel_text, d_mouse_x + d_mouse_w, d_mouse_y - d_txt6_h, scheme, TBLACK, TPF_TEXT|TPF_RIGHT);
			}

			commands->Draw_All();

			Show_Mouse();
			refresh = false;
		}

		/*
		**	Get user input.
		*/
		input = commands->Input();

		/*
		**	Process input.
		*/
		switch (input) {
			case (BUTTON_SPEED | KN_BUTTON):
				curbutton = (BUTTON_SPEED - BUTTON_FIRST);
				refresh = true;
				break;

			case (BUTTON_SCROLLRATE | KN_BUTTON):
				curbutton = (BUTTON_SCROLLRATE - BUTTON_FIRST);
				refresh = true;
				break;

			case (BUTTON_MOUSE_ACCEL | KN_BUTTON):
				curbutton = (BUTTON_MOUSE_ACCEL - BUTTON_FIRST);
				refresh = true;
				break;

			case (BUTTON_VISUAL | KN_BUTTON):
				selection = BUTTON_VISUAL;
				pressed = true;
				break;

			case (BUTTON_SOUND | KN_BUTTON):
				selection = BUTTON_SOUND;
				pressed = true;
				break;

			case (BUTTON_OK | KN_BUTTON):
				selection = BUTTON_OK;
				pressed = true;
				break;

#ifdef WOLAPI_INTEGRATION
			case (BUTTON_WOLAPI | KN_BUTTON):
				selection = BUTTON_WOLAPI;
				pressed = true;
				break;
#endif

			case (KN_ESC):
				process = false;
				break;

			case (KN_LEFT):
				if (curbutton == (BUTTON_SPEED - BUTTON_FIRST) ) {
					gspeed_btn.Bump(1);
				} else
					if (curbutton == (BUTTON_SCROLLRATE - BUTTON_FIRST) ) {
						scrate_btn.Bump(1);
					} else
						if (curbutton == (BUTTON_MOUSE_ACCEL - BUTTON_FIRST) ) {
							mouseaccel_btn.Bump(1);
							refresh = true;
						}
				break;

			case (KN_RIGHT):
				if (curbutton == (BUTTON_SPEED - BUTTON_FIRST) ) {
					gspeed_btn.Bump(0);
				} else
					if (curbutton == (BUTTON_SCROLLRATE - BUTTON_FIRST) ) {
						scrate_btn.Bump(0);
					} else
						if (curbutton == (BUTTON_MOUSE_ACCEL - BUTTON_FIRST) ) {
							mouseaccel_btn.Bump(0);
							refresh = true;
						}
				break;

			case (KN_UP):
				if (buttons[curbutton]) {
					buttons[curbutton]->Turn_Off();
					buttons[curbutton]->Flag_To_Redraw();
				}

				curbutton--;
#ifdef WOLAPI_INTEGRATION
				if( !bShowWolapi )
				{
					if( curbutton == BUTTON_WOLAPI - BUTTON_FIRST )
						curbutton--;		//	Skip over missing button.
				}
#endif
				if (curbutton < 0) {
					curbutton = (BUTTON_COUNT - BUTTON_FIRST - 1);
				}

				if (buttons[curbutton]) {
					buttons[curbutton]->Turn_On();
					buttons[curbutton]->Flag_To_Redraw();
				}
				refresh = true;
				break;

			case (KN_DOWN):
				if (buttons[curbutton]) {
					buttons[curbutton]->Turn_Off();
					buttons[curbutton]->Flag_To_Redraw();
				}

				curbutton++;
#ifdef WOLAPI_INTEGRATION
				if( !bShowWolapi )
				{
					if( curbutton == BUTTON_WOLAPI - BUTTON_FIRST )
						curbutton++;		//	Skip over missing button.
				}
#endif
				if (curbutton > (BUTTON_COUNT - BUTTON_FIRST - 1) ) {
					curbutton = 0;
				}

				if (buttons[curbutton]) {
					buttons[curbutton]->Turn_On();
					buttons[curbutton]->Flag_To_Redraw();
				}
				refresh = true;
				break;

			case (KN_RETURN):
				selection = curbutton + BUTTON_FIRST;
				pressed = true;
				break;

			default:
				break;
		}

		/*
		**	Perform some action. Either to exit the dialog or bring up another.
		*/
		if (pressed) {

			/*
			**	Record the new options slider settings.
			** The GameSpeed data member MUST NOT BE SET HERE!  It will cause multiplayer
			** games to go out of sync.  It's set by virtue of the event being executed.
			*/
			if (gamespeed != ((OptionsClass::MAX_SPEED_SETTING-1) - gspeed_btn.Get_Value()) ) {
				gamespeed = (OptionsClass::MAX_SPEED_SETTING-1) - gspeed_btn.Get_Value();
				if (use_title_page) {
					Options.GameSpeed = gamespeed;
				} else {
					OutList.Add(EventClass(EventClass::GAMESPEED, gamespeed));
				}
			}

			if (scrollrate != ((OptionsClass::MAX_SCROLL_SETTING-1) - scrate_btn.Get_Value()) ) {
				scrollrate = (OptionsClass::MAX_SCROLL_SETTING-1) - scrate_btn.Get_Value();
				Options.ScrollRate = scrollrate;
			}
			mouseaccel = mouseaccel_btn.Get_Value();
			Options.Set_Mouse_Acceleration(fixed(mouseaccel + 1, 2));
			process = false;

			/*
			** Save the settings in such a way that the GameSpeed is only set during
			** the save process; restore it when we're done, so multiplayer games don't
			** go out of sync.
			*/
			if (Session.Type == GAME_NORMAL) {
				Options.GameSpeed = gamespeed;
				Options.Save_Settings();			// save new value
			} else {
				int old = Options.GameSpeed;		// save orig value
				Options.GameSpeed = gamespeed;
				Options.Save_Settings();			// save new value
				Options.GameSpeed = old;			// restore old value
			}

			/*
			**	Possibly launch into another dialog if so directed.
			*/
			switch (selection) {
				case (BUTTON_VISUAL):
					VisualControlsClass().Process();
					process = true;
					display = true;
					refresh = true;
					break;

				case (BUTTON_SOUND):
					SoundControlsClass().Process();
					process = true;
					display = true;
					refresh = true;
					break;

#ifdef WOLAPI_INTEGRATION
				case BUTTON_WOLAPI:
					if( WOL_Options_Dialog( pWolapi, true ) )
					{
						//	The game ended while in this dialog.
						process = false;
					}
					else
					{
						process = true;
						display = true;
						refresh = true;
					}
					break;
#endif

				case (BUTTON_OK):
					break;
			}

			pressed = false;
		}
		if (process && (Session.Type == GAME_NORMAL || Session.Type == GAME_SKIRMISH)) {
			SDL_Delay(16);
		}
	}
}
