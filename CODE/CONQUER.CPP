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

/* $Header: /CounterStrike/CONQUER.CPP 6     3/13/97 2:05p Steve_tall $ */
#include <cstdlib>
#include <SDL3/SDL_storage.h>

// disable leak detection
#ifdef __cplusplus
extern "C"
#endif
const char* __asan_default_options() { return "detect_leaks=0"; }

void WWDraw_Flush_Present(void);
bool WWDraw_Has_Pending_Present(void);
void WWDraw_Request_Present(void);
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : CONQUER.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : April 3, 1991                                                *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   CC_Draw_Shape -- Custom draw shape handler.                                               *
 *   Call_Back -- Main game maintenance callback routine.                                      *
 *   Color_Cycle -- Handle the general palette color cycling.                                  *
 *   Crate_From_Name -- Given a crate name convert it to a crate type.                         *
 *   Disk_Space_Available -- returns bytes of free disk space                                  *
 *   Do_Record_Playback -- handles saving/loading map pos & current object                     *
 *   Fading_Table_Name -- Builds a theater specific fading table name.                         *
 *   Fetch_Techno_Type -- Convert type and ID into TechnoTypeClass pointer.                    *
 *   Get_Radar_Icon -- Builds and alloc a radar icon from a shape file                         *
 *   Handle_Team -- Processes team selection command.                                          *
 *   Handle_View -- Either records or restores the tactical view.                              *
 *   KN_To_Facing -- Converts a keyboard input number into a facing value.                     *
 *   Keyboard_Process -- Processes the tactical map input codes.                               *
 *   Language_Name -- Build filename for current language.                                     *
 *   List_Copy -- Makes a copy of a cell offset list.                                          *
 *   Main_Game -- Main game startup routine.                                                   *
 *   Main_Loop -- This is the main game loop (as a single loop).                               *
 *   Main_Loop_Frame_Interval -- Returns the target interval between gameplay frames.          *
 *   Main_Loop_Frame_Ready -- Determines whether the next gameplay frame should run.           *
 *   Map_Edit_Loop -- a mini-main loop for map edit mode only                                  *
 *   Message_Input -- allows inter-player message input processing                             *
 *   MixFileHandler -- Handles VQ file access.                                                 *
 *   Name_From_Source -- retrieves the name for the given SourceType                           *
 *   Owner_From_Name -- Convert an owner name into a bitfield.                                 *
 *   Play_Movie -- Plays a VQ movie.                                                           *
 *   Shake_The_Screen -- Dispatcher that shakes the screen.                                    *
 *   Shape_Dimensions -- Determine the minimum rectangle for the shape.                        *
 *   Source_From_Name -- Converts ASCII name into SourceType.                                  *
 *   Theater_From_Name -- Converts ASCII name into a theater number.                           *
 *   Reset_Main_Loop_Frame_State -- Resets main-loop frame scheduling state.                   *
 *   Run_Game_Frame -- Runs one gameplay simulation frame.                                     *
 *   Unselect_All -- Causes all selected objects to become unselected.                         *
 *   VQ_Call_Back -- Maintenance callback used for VQ movies.                                  *
 *   Game_Config_Section -- Returns pointer to string containing the game config section.
 *   Is_Counterstrike_Installed -- Function to determine the availability of the CS expansion.
 *   Is_Aftermath_Installed -- Function to determine the availability of the AM expansion.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef TESTCODE
class A {
	public:
		enum {VAR=1};
};

template<class T>
class B {
	public:
		enum {VAR2=T::VAR};  // this is the line in question.
};

B<A> test;
#endif

#include	"FUNCTION.H"
#if RA_ENABLE_WOL
#include	"WSMGR.H"
#endif
#include	"SDLINPUT.H"
#ifdef NETWORK_UDP
#include	"WSPROTO.H"
#else	//NETWORK_UDP
#include	"TCPIP.H"
#endif	//NETWORK_UDP
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include "sdl_fs.h"
#include	"VORTEX.H"

#ifdef WOLAPI_INTEGRATION
//#include "WOLDEBUG.H"
#include "WOLSTRNG.H"
extern WolapiObject* pWolapi;
#define PAGE_RESPOND_KEY	KN_RETURN	//KN_COMMA
#endif

static bool Conquer_Trace_Enabled(void)
{
	static int enabled = -1;
	if (enabled == -1) {
		enabled = (getenv("RA_TRACE_STARTUP") != NULL) ? 1 : 0;
	}
	return(enabled != 0);
}

static bool Conquer_Callback_Trace_Enabled(void)
{
	static int enabled = -1;
	if (enabled == -1) {
		enabled = (getenv("RA_TRACE_CALLBACK") != NULL) ? 1 : 0;
	}
	return(enabled != 0);
}

static void Conquer_Trace(char const * format, ...)
{
	if (!Conquer_Trace_Enabled()) {
		return;
	}

	va_list args;
	va_start(args, format);
	fputs("[conquer] ", stderr);
	vfprintf(stderr, format, args);
	fputc('\n', stderr);
	fflush(stderr);
	va_end(args);
}

static int32_t Movie_Playback_Volume(void)
{
	const fixed louder = (Options.ScoreVolume > Options.Volume) ? Options.ScoreVolume : Options.Volume;
	return(louder * 256);
}

#define SHAPE_TRANS		0x40

void * Get_Shape_Header_Data(void * ptr);

extern bool Is_Mission_Aftermath (char *file_name);
extern bool Is_Mission_Counterstrike (char *file_name);

#ifdef FIXIT_VERSION_3		//	Stalemate games.
extern void Do_Draw(void);
#endif

#ifdef CHEAT_KEYS
bool	bNoMovies = false;
#endif

/****************************************
**	Function prototypes for this module **
*****************************************/
bool Main_Loop(void);
void Keyboard_Process(KeyNumType & input);
static void Message_Input(KeyNumType &input);
static void Color_Cycle(void);
static void Reset_Main_Loop_Frame_State(void);
bool Map_Edit_Loop(void);

bool UseOldShapeDraw = false;

#ifdef CHEAT_KEYS
void Dump_Heap_Pointers( void );
void Error_In_Heap_Pointers( char * string );
#endif
static void Do_Record_Playback(void);

void Toggle_Formation(void);

extern char * __nheapbeg;

//
// Special module globals for recording and playback
//
char TeamEvent = 0;			// 0 = no event, 1,2,3 = team event type
char TeamNumber = 0;			// which team was selected? (1-9)
char FormationEvent = 0;	// 0 = no event, 1 = formation was toggled

static uint32_t MainLoopLastServiceTick = 0;

	/* -----------------10/14/96 7:29PM------------------

	 --------------------------------------------------*/

/***********************************************************************************************
 * Main_Game -- Main game startup routine.                                                     *
 *                                                                                             *
 *    This is the first official routine of the game. It handles game initialization and       *
 *    the main game loop control.                                                              *
 *                                                                                             *
 *    Initialization:                                                                          *
 *    - Init_Game handles one-time-only inits                                                  *
 *    - Select_Game is responsible for initializations required for each new game played       *
 *      (these may be different depending on whether a multiplayer game is selected, and       *
 *      other parameters)                                                                      *
 *    - This routine performs any un-inits required, both for each game played, and one-time   *
 *                                                                                             *
 * INPUT:   argc  -- Number of command line arguments (including program name itself).         *
 *                                                                                             *
 *          argv  -- Array of command line argument pointers.                                  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/01/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void Main_Game(int argc, char * argv[])
{
	static bool fade = true;

	/*
	**	Perform one-time-only initializations
	*/
	if (!Init_Game(argc, argv)) {
		return;
	}

	/*
	**	Game processing loop:
	**	1) Select which game to play, or whether to exit (don't fade the palette
	**		on the first game selection, but fade it in on subsequent calls)
	**	2) Invoke either the main-loop routine, or the editor-loop routine,
	**		until they indicate that the user wants to exit the scenario.
	*/
	while (Select_Game(fade)) {
		fade = false;
		ScenarioInit = 0;		// Kludge.

		fade = true;

		/*
		** Initialise the color lookup tables for the chronal vortex
		*/
		ChronalVortex.Stop();
		ChronalVortex.Setup_Remap_Tables(Scen.Theater);

		/*
		**	Make the game screen visible, clear the keyboard buffer of spurious
		**	values, and then show the mouse.  This PRESUMES that Select_Game() has
		**	told the map to draw itself.
		*/
		GamePalette.Set(FADE_PALETTE_MEDIUM);
		Keyboard->Clear();
		/*
		** Only show the mouse if we're not playing back a recording.
		*/
		if (Session.Play) {
			Hide_Mouse();
			TeamEvent = 0;
			TeamNumber = 0;
			FormationEvent = 0;
		} else {
			Show_Mouse();
		}

		Reset_Main_Loop_Frame_State();

		if (Session.Type == GAME_INTERNET) {
			Register_Game_Start_Time();
			GameStatisticsPacketSent = false;
			PacketLater = NULL;
			ConnectionLost = false;
		}

#ifdef SCENARIO_EDITOR
		/*
		**	Scenario-editor version of main-loop processing
		*/
		for (;;) {
			/*
			**	Non-scenario-editor-mode: call the game's main loop
			*/
			if (!Debug_Map) {
				SDL_GameInput_SetMissionMouseCapture(!Session.Play);
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
				TimeQuake = PendingTimeQuake;
				PendingTimeQuake = false;
#else
				TimeQuake = false;
#endif
				if (Main_Loop()) {
					break;
				}

				if (SpecialDialog != SDLG_NONE) {
					SDL_GameInput_PushMouseCaptureSuppression();
					switch (SpecialDialog) {
						case SDLG_SPECIAL:
							Map.Help_Text(TXT_NONE);
							Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
							Special_Dialog();
							Map.Revert_Mouse_Shape();
							SpecialDialog = SDLG_NONE;
							break;

						case SDLG_OPTIONS:
							Map.Help_Text(TXT_NONE);
							Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
							Options.Process();
							Map.Revert_Mouse_Shape();
							SpecialDialog = SDLG_NONE;
							break;

						case SDLG_SURRENDER:
							Map.Help_Text(TXT_NONE);
							Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
							if (Surrender_Dialog(TXT_SURRENDER)) {
								PlayerPtr->Flag_To_Lose();
							}
							SpecialDialog = SDLG_NONE;
							Map.Revert_Mouse_Shape();
							break;

						default:
							break;
					}
					SDL_GameInput_PopMouseCaptureSuppression();
				}
			} else {
				SDL_GameInput_SetMissionMouseCapture(false);

				/*
				**	Scenario-editor-mode: call the editor's main loop
				*/
				if (Map_Edit_Loop()) {
					break;
				}
			}
		}
#else
		/*
		**	Non-editor version of main-loop processing
		*/
		for (;;) {
			SDL_GameInput_SetMissionMouseCapture(!Session.Play);
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
			TimeQuake = PendingTimeQuake;
			PendingTimeQuake = false;
#else
			TimeQuake = false;
#endif
			/*
			**	Call the game's main loop
			*/
			if (Main_Loop()) {
				break;
			}

			/*
			**	If the SpecialDialog flag is set, invoke the given special dialog.
			**	This must be done outside the main loop, since the dialog will call
			**	Main_Loop(), allowing the game to run in the background.
			*/
			if (SpecialDialog != SDLG_NONE) {
				SDL_GameInput_PushMouseCaptureSuppression();
				switch (SpecialDialog) {
					case SDLG_SPECIAL:
						Map.Help_Text(TXT_NONE);
						Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
						Special_Dialog();
						Map.Revert_Mouse_Shape();
						SpecialDialog = SDLG_NONE;
						break;

					case SDLG_OPTIONS:
						Map.Help_Text(TXT_NONE);
						Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
						Options.Process();
						Map.Revert_Mouse_Shape();
						SpecialDialog = SDLG_NONE;
						break;

					case SDLG_SURRENDER:
						Map.Help_Text(TXT_NONE);
						Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
						if (Surrender_Dialog(TXT_SURRENDER)) {
							OutList.Add(EventClass(EventClass::DESTRUCT));
						}
						SpecialDialog = SDLG_NONE;
						Map.Revert_Mouse_Shape();
						break;

/*ifdef FIXIT_VERSION_3		//	Stalemate games.
					case SDLG_PROPOSE_DRAW:
						Map.Help_Text(TXT_NONE);
						Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
						if (Surrender_Dialog(TXT_WOL_PROPOSE_DRAW)) {
							OutList.Add(EventClass(EventClass::PROPOSE_DRAW));
						}
						SpecialDialog = SDLG_NONE;
						Map.Revert_Mouse_Shape();
						break;

					case SDLG_ACCEPT_DRAW:
						Map.Help_Text(TXT_NONE);
						Map.Override_Mouse_Shape(MOUSE_NORMAL, false);
						if (Surrender_Dialog(TXT_WOL_ACCEPT_DRAW)) {
							OutList.Add(EventClass(EventClass::ACCEPT_DRAW));
						}
						SpecialDialog = SDLG_NONE;
						Map.Revert_Mouse_Shape();
						break;
#endif
*/
					default:
						break;
				}
				SDL_GameInput_PopMouseCaptureSuppression();
			}
		}
#endif
		SDL_GameInput_SetMissionMouseCapture(false);

		/*
		** Send the game stats if we haven't already done so.
		*/
		if (!GameStatisticsPacketSent && PacketLater) {
			Send_Statistics_Packet();		//	After game sending if PacketLater set.
		}

		/*
		**	Scenario is done; fade palette to black
		*/
		BlackPalette.Set(FADE_PALETTE_SLOW);
		VisiblePage.Clear();

		/*
		**	Un-initialize whatever needs it, for each game played.
		**
		**	Shut down either the modem or network; they'll get re-initialized if
		**	the user selections those options again in Select_Game().  This
		**	"re-boots" the modem & network code, which I currently feel is safer
		**	than just letting it hang around.
		** (Skip this step if we're in playback mode; the modem or net won't have
		** been initialized in that case.)
		*/
		if (Session.Record || Session.Play) {
			Session.RecordFile.Close();
		}

		if (Session.Type == GAME_UDP
#if RA_ENABLE_WOL
			|| Session.Type == GAME_WOL
#endif
		) {
			if (!Session.Play) {
				if (Session.Type == GAME_WOL) {
#if RA_ENABLE_WOL
					Session.GameName[0] = 0;
					Wol.Return_To_Lobby();
#endif
				} else {
					Shutdown_Network();
				}
			}
		}

		/*
		**	If we're playing back, the mouse will be hidden; show it.
		** Also, set all variables back to normal, to return to the main menu.
		*/
		if (Session.Play) {
			Show_Mouse();
			Session.Type = GAME_NORMAL;
			Session.Play = 0;
		}
	}

	/*
	**	Free the scenario description buffers
	*/
	Session.Free_Scenario_Descriptions();
}

/***********************************************************************************************
 * Keyboard_Process -- Processes the tactical map input codes.                                 *
 *                                                                                             *
 *    This routine is used to process the input codes while the player                         *
 *    has the tactical map displayed. It handles all the keys that                             *
 *    are appropriate to that mode.                                                            *
 *                                                                                             *
 * INPUT:   input -- Input code as returned from Input_Num().                                  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/21/1992 JLB : Created.                                                                 *
 *   07/04/1995 JLB : Handles team and map control hotkeys.                                    *
 *=============================================================================================*/
void Keyboard_Process(KeyNumType & input)
{
	ObjectClass * obj;
	int index;

	/*
	**	Don't do anything if there is not keyboard event.
	*/
	if (input == KN_NONE) {
		return;
	}
	/*
	**	For network & modem, process user input for inter-player messages.
	*/
	Message_Input(input);

	/*
	**	The VK_BIT must be stripped from the "plain" value of the key so that a comparison to
	**	KN_1, for example, will yield TRUE if in fact the "1" key was pressed.
	*/

	KeyNumType plain = KeyNumType(input & ~(WWKEY_SHIFT_BIT|WWKEY_ALT_BIT|WWKEY_CTRL_BIT|WWKEY_VK_BIT));
	KeyNumType key = KeyNumType(input & ~WWKEY_VK_BIT);

#ifdef CHEAT_KEYS

	if (Debug_Flag) {
		HousesType h;

		switch (int(input)) {
			case int(int(KN_M) | int(KN_SHIFT_BIT)):
			case int(int(KN_M) | int(KN_ALT_BIT)):
			case int(int(KN_M) | int(KN_CTRL_BIT)):
				for (h = HOUSE_FIRST; h < HOUSE_COUNT; h++) {
					Houses.Ptr(h)->Refund_Money(10000);
				}
				break;

			default:
				break;
		}
	}
#endif

#ifdef VIRGIN_CHEAT_KEYS
	if (Debug_Playtest && input == (KN_W|KN_ALT_BIT)) {
		PlayerPtr->Blockage = false;
		PlayerPtr->Flag_To_Win();
	}
#endif

#ifdef CHEAT_KEYS
	if (Debug_Playtest && input == (KA_W|KN_ALT_BIT)) {
		PlayerPtr->Blockage = false;
		PlayerPtr->Flag_To_Win();
	}

	if ((Debug_Flag || Debug_Playtest) && plain == KN_F4) {
		if (Session.Type == GAME_NORMAL) {
			Debug_Unshroud = (Debug_Unshroud == false);
			Map.Flag_To_Redraw(true);
		}
	}

	if (Debug_Flag && input == KN_SLASH) {
		if (Session.Type != GAME_NORMAL) {
			SpecialDialog = SDLG_SPECIAL;
			input = KN_NONE;
		} else {
			Special_Dialog();
		}
	}
#endif

	/*
	**	Process prerecorded team selection. This will be an additive select
	**	if the SHIFT key is held down. It will create the team if the
	**	CTRL or ALT key is held down.
	*/
	int action = 0;
	if (input & WWKEY_SHIFT_BIT) action = 1;
	if (input & WWKEY_ALT_BIT) action = 3;
	if (input & WWKEY_CTRL_BIT) action = 2;

	/*
	**	If the "N" key is pressed, then select the next object.
	*/
	if (key != 0 && key == Options.KeyNext) {
		if (action) {
			obj = Map.Prev_Object(CurrentObject.Count() ? CurrentObject[0] : NULL);
		} else {
			obj = Map.Next_Object(CurrentObject.Count() ? CurrentObject[0] : NULL);
		}
		if (obj != NULL) {
			Unselect_All();
		 	obj->Select();
			Map.Center_Map();
			Map.Flag_To_Redraw(true);
		}
		input = KN_NONE;
	}
	if (key != 0 && key == Options.KeyPrevious) {
		if (action) {
			obj = Map.Next_Object(CurrentObject.Count() ? CurrentObject[0] : NULL);
		} else {
			obj = Map.Prev_Object(CurrentObject.Count() ? CurrentObject[0] : NULL);
		}
		if (obj != NULL) {
			Unselect_All();
		 	obj->Select();
			Map.Center_Map();
			Map.Flag_To_Redraw(true);
		}
		input = KN_NONE;
	}

	/*
	**	All selected units will go into idle mode.
	*/
	if (key != 0 && key == Options.KeyStop) {
		if (CurrentObject.Count()) {
			for (index = 0; index < CurrentObject.Count(); index++) {
				ObjectClass const * tech = CurrentObject[index];

				if (tech != NULL && (tech->Can_Player_Move() || (tech->Can_Player_Fire() && tech->What_Am_I() != RTTI_BUILDING))) {
					OutList.Add(EventClass(EventClass::IDLE, TargetClass(tech)));
				}
			}
		}
		input = KN_NONE;
	}

	/*
	**	All selected units will attempt to go into guard area mode.
	*/
	if (key != 0 && key == Options.KeyGuard) {
		if (CurrentObject.Count()) {
			for (index = 0; index < CurrentObject.Count(); index++) {
				ObjectClass const * tech = CurrentObject[index];

				if (tech != NULL && tech->Can_Player_Move() && tech->Can_Player_Fire()) {
					OutList.Add(EventClass(TargetClass(tech), MISSION_GUARD_AREA));
				}
			}
		}
		input = KN_NONE;
	}

	/*
	**	All selected units will attempt to scatter.
	*/
	if (key != 0 && key == Options.KeyScatter) {
		if (CurrentObject.Count()) {
			for (index = 0; index < CurrentObject.Count(); index++) {
				ObjectClass const * tech = CurrentObject[index];

				if (tech != NULL && tech->Can_Player_Move()) {
					OutList.Add(EventClass(EventClass::SCATTER, TargetClass(tech)));
				}
			}
		}
		input = KN_NONE;
	}

	/*
	**	Center the map around the currently selected objects. If no
	**	objects are selected, then fall into the home case.
	*/
	if (key != 0 && (key == Options.KeyHome1 || key == Options.KeyHome2)) {
		if (CurrentObject.Count()) {
			Map.Center_Map();
			Map.Flag_To_Redraw(true);
			input = KN_NONE;
		} else {
			input = Options.KeyBase;
		}
	}

	/*
	**	Center the map about the construction yard or construction vehicle
	**	if one is present.
	*/
	if (key != 0 && key == Options.KeyBase) {
		Unselect_All();
		if (PlayerPtr->CurBuildings) {
			for (index = 0; index < Buildings.Count(); index++) {
				BuildingClass * building = Buildings.Ptr(index);

				if (building != NULL && !building->IsInLimbo && building->House == PlayerPtr && *building == STRUCT_CONST) {
					Unselect_All();
					building->Select();
					if (building->IsLeader) break;
				}
			}
		}
		if (CurrentObject.Count() == 0 && PlayerPtr->CurUnits) {
			for (index = 0; index < Units.Count(); index++) {
				UnitClass * unit = Units.Ptr(index);

				if (unit != NULL && !unit->IsInLimbo && unit->House == PlayerPtr && *unit == UNIT_MCV) {
					Unselect_All();
					unit->Select();
					break;
				}
			}
		}
		if (CurrentObject.Count()) {
			Map.Center_Map();
		} else {
			if (PlayerPtr->Center != 0) {
				Map.Center_Map(PlayerPtr->Center);
			}
		}
		Map.Flag_To_Redraw(true);
		input = KN_NONE;
	}

	/*
	** Toggle the status of formation for the current team
	*/
	if (key != 0 && key == Options.KeyFormation) {
		Toggle_Formation();
		input = KN_NONE;
	}

#ifdef TOFIX
	/*
	** For multiplayer, 'R' pops up the surrender dialog.
	*/
	if (input != 0 && input == Options.KeyResign) {
		if (!PlayerLoses && /*Session.Type != GAME_NORMAL &&*/ !PlayerPtr->IsDefeated) {
			SpecialDialog = SDLG_SURRENDER;
			input = KN_NONE;
		}
		input = KN_NONE;
	}
#endif

	/*
	**	Handle making and breaking alliances.
	*/
	if (key != 0 && key == Options.KeyAlliance) {
		if (Session.Type != GAME_NORMAL || Debug_Flag) {
			if (CurrentObject.Count() && !PlayerPtr->IsDefeated) {
				if (CurrentObject[0]->Owner() != PlayerPtr->Class->House) {
					OutList.Add(EventClass(EventClass::ALLY, CurrentObject[0]->Owner()));
				}
			}
		}
		input = KN_NONE;
	}

	/*
	**	Select all the units on the current display. This is equivalent to
	**	drag selecting the whole view.
	*/
	if (key != 0 && key == Options.KeySelectView) {
		Map.Select_These(0x00000000, XY_Coord(Map.TacLeptonWidth, Map.TacLeptonHeight));
		input = KN_NONE;
	}

	/*
	**	Toggles the repair state similarly to pressing the repair button.
	*/
	if (key != 0 && key == Options.KeyRepair) {
		Map.Repair_Mode_Control(-1);
		input = KN_NONE;
	}

	/*
	**	Toggles the sell state similarly to pressing the sell button.
	*/
	if (key != 0 && key == Options.KeySell) {
		Map.Sell_Mode_Control(-1);
		input = KN_NONE;
	}

	/*
	**	Toggles the map zoom mode similarly to pressing the map button.
	*/
	if (key != 0 && key == Options.KeyMap) {
		Map.Zoom_Mode_Control();
		input = KN_NONE;
	}

	/*
	**	Scrolls the sidebar up one slot.
	*/
	if (key != 0 && key == Options.KeySidebarUp) {
		Map.SidebarClass::Scroll(true, -1);
		input = KN_NONE;
	}

	/*
	**	Scrolls the sidebar down one slot.
	*/
	if (key != 0 && key == Options.KeySidebarDown) {
		Map.SidebarClass::Scroll(false, -1);
		input = KN_NONE;
	}

	/*
	**	Brings up the options dialog box.
	*/
	if (key != 0 && (key == Options.KeyOption1 || key == Options.KeyOption2)) {
		Map.Help_Text(TXT_NONE);			// Turns off help text.
		Queue_Options();
		input = KN_NONE;
	}

	/*
	**	Scrolls the tactical map in the direction specified.
	*/
	int distance = CELL_LEPTON_W;
	if (key != 0 && key == Options.KeyScrollLeft) {
		Map.Scroll_Map(DIR_W, distance, true);
		input = KN_NONE;
	}
	if (key != 0 && key == Options.KeyScrollRight) {
		Map.Scroll_Map(DIR_E, distance, true);
		input = KN_NONE;
	}
	if (key != 0 && key == Options.KeyScrollUp) {
		Map.Scroll_Map(DIR_N, distance, true);
		input = KN_NONE;
	}
	if (key != 0 && key == Options.KeyScrollDown) {
		Map.Scroll_Map(DIR_S, distance, true);
		input = KN_NONE;
	}

	/*
	**	Teams are handled by the 10 special team keys. The manual comparison
	**	to the KN numbers is because the Windows keyboard driver can vary
	**	the base code number for the key depending on the shift or alt key
	**	state!
	*/
	if (input != 0 && (plain == Options.KeyTeam1 || plain == KN_1)) {
		Handle_Team(0, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam2 || plain == KN_2)) {
		Handle_Team(1, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam3 || plain == KN_3)) {
		Handle_Team(2, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam4 || plain == KN_4)) {
		Handle_Team(3, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam5 || plain == KN_5)) {
		Handle_Team(4, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam6 || plain == KN_6)) {
		Handle_Team(5, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam7 || plain == KN_7)) {
		Handle_Team(6, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam8 || plain == KN_8)) {
		Handle_Team(7, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam9 || plain == KN_9)) {
		Handle_Team(8, action);
		input = KN_NONE;
	}
	if (input != 0 && (plain == Options.KeyTeam10 || plain == KN_0)) {
		Handle_Team(9, action);
		input = KN_NONE;
	}

	/*
	**	Handle the bookmark hotkeys.
	*/
	if (input != 0 && plain == Options.KeyBookmark1 && !Debug_Map) {
		Handle_View(0, action);
		input = KN_NONE;
	}
	if (input != 0 && plain == Options.KeyBookmark2 && !Debug_Map) {
		Handle_View(1, action);
		input = KN_NONE;
	}
	if (input != 0 && plain == Options.KeyBookmark3 && !Debug_Map) {
		Handle_View(2, action);
		input = KN_NONE;
	}
	if (input != 0 && plain == Options.KeyBookmark4 && !Debug_Map) {
		Handle_View(3, action);
		input = KN_NONE;
	}

#ifdef CHEAT_KEYS
	if (input != 0 && Debug_Flag && input && (input & KN_RLSE_BIT) == 0) {
		Debug_Key(input);
	}
#endif
}

void Toggle_Formation(void) {
	int team = -1;
	int32_t minx = 0x7FFFFFFFL, miny = 0x7FFFFFFFL;
	int32_t maxx = 0, maxy = 0;
	int index;
	bool setform = 0;

	//
	// Recording support
	//
	if (Session.Record) {
		FormationEvent = 1;
	}

	/*
	** Find the first selected object that is a member of a team, and
	** register his group as the team we're using.  Once we find the team
	** number, update the 'setform' flag to know whether we should be setting
	** the formation's offsets, or clearing them.  If they currently have
	** illegal offsets (as in 0x80000000), then we're setting.
	*/
	for (index = 0; index < Units.Count(); index++) {
		UnitClass * obj = Units.Ptr(index);
		if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->IsSelected) {
			team = obj->Group;
			if (team != -1) {
				setform = obj->XFormOffset == (int)0x80000000;
				TeamSpeed[team] = SPEED_WHEEL;
				TeamMaxSpeed[team] = MPH_LIGHT_SPEED;
				break;
			}
		}
	}
	if (team == -1) {
		for (index = 0; index < Infantry.Count(); index++) {
			InfantryClass * obj = Infantry.Ptr(index);
			if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->IsSelected) {
				team = obj->Group;
				if (team != -1) {
					setform = obj->XFormOffset == (int)0x80000000;
					TeamSpeed[team] = SPEED_WHEEL;
					TeamMaxSpeed[team] = MPH_LIGHT_SPEED;
					break;
				}
			}
		}
	}

	if (team == -1) {
		for (index = 0; index < Vessels.Count(); index++) {
			VesselClass * obj = Vessels.Ptr(index);
			if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->IsSelected) {
				team = obj->Group;
				if (team != -1) {
					setform = obj->XFormOffset == 0x80000000UL;
					TeamSpeed[team] = SPEED_WHEEL;
					TeamMaxSpeed[team] = MPH_LIGHT_SPEED;
					break;
				}
			}
		}
	}

	if (team == -1) return;
	/*
	** Now that we have a team, let's go set (or clear) the formation offsets.
	*/
	for (index = 0; index < Units.Count(); index++) {
		UnitClass * obj = Units.Ptr(index);
		if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team) {
			obj->Mark(MARK_CHANGE);
			if (setform) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
				if (xc < minx) minx = xc;
				if (xc > maxx) maxx = xc;
				if (yc < miny) miny = yc;
				if (yc > maxy) maxy = yc;
				if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
					TeamMaxSpeed[team] = obj->Class->MaxSpeed;
					TeamSpeed[team] = obj->Class->Speed;
				}
			} else {
				obj->XFormOffset = obj->YFormOffset = (int)0x80000000;
			}
		}
	}

	for (index = 0; index < Infantry.Count(); index++) {
		InfantryClass * obj = Infantry.Ptr(index);
		if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team) {
			obj->Mark(MARK_CHANGE);
			if (setform) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
				if (xc < minx) minx = xc;
				if (xc > maxx) maxx = xc;
				if (yc < miny) miny = yc;
				if (yc > maxy) maxy = yc;
				if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
					TeamMaxSpeed[team] = obj->Class->MaxSpeed;
				}
			} else {
				obj->XFormOffset = obj->YFormOffset = (int)0x80000000;
			}
		}
	}

	for (index = 0; index < Vessels.Count(); index++) {
		VesselClass * obj = Vessels.Ptr(index);
		if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team) {
			obj->Mark(MARK_CHANGE);
			if (setform) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
				if (xc < minx) minx = xc;
				if (xc > maxx) maxx = xc;
				if (yc < miny) miny = yc;
				if (yc > maxy) maxy = yc;
				if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
					TeamMaxSpeed[team] = obj->Class->MaxSpeed;
				}
			} else {
				obj->XFormOffset = obj->YFormOffset = 0x80000000UL;
			}
		}
	}

	/*
	** All the units have been counted to find the bounding rectangle and
	** center of the formation, or to clear their offsets.  Now, if we're to
	** set them into formation, proceed to do so.  Otherwise, bail.
	*/
	if (setform) {
		int centerx = (int)((maxx - minx)/2)+minx;
		int centery = (int)((maxy - miny)/2)+miny;

		for (index = 0; index < Units.Count(); index++) {
			UnitClass * obj = Units.Ptr(index);
			if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));

				obj->XFormOffset = xc - centerx;
				obj->YFormOffset = yc - centery;
			}
		}

		for (index = 0; index < Infantry.Count(); index++) {
			InfantryClass * obj = Infantry.Ptr(index);
			if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team ) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));

				obj->XFormOffset = xc - centerx;
				obj->YFormOffset = yc - centery;
			}
		}

		for (index = 0; index < Vessels.Count(); index++) {
			VesselClass * obj = Vessels.Ptr(index);
			if (obj && !obj->IsInLimbo && obj->House == PlayerPtr && obj->Group == team ) {
				int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
				int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));

				obj->XFormOffset = xc - centerx;
				obj->YFormOffset = yc - centery;
			}
		}
	}
}

/***********************************************************************************************
 * Message_Input -- allows inter-player message input processing                               *
 *                                                                                             *
 * INPUT:                                                                                      *
 *		input		key value																							  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *		none.																												  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *		none.																												  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/22/1995 BRR : Created.                                                                 *
 *=============================================================================================*/

static void Message_Input(KeyNumType &input)
{
	int rc;
	char txt[MAX_MESSAGE_LENGTH+32];
	int id;
	int i;
	KeyNumType copy_input;
	//char *msg;

	/*
	**	Check keyboard input for a request to send a message.
	**	The 'to' argument for Add_Edit is prefixed to the message buffer; the
	**	message buffer is big enough for the 'to' field plus MAX_MESSAGE_LENGTH.
	**	To send the message, calling Get_Edit_Buf retrieves the buffer minus the
	**	'to' portion.  At the other end, the buffer allocated to display the
	**	message must be MAX_MESSAGE_LENGTH plus the size of "From: xxx (house)".
	*/
#ifdef WOLAPI_INTEGRATION
	if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH && 
		( ( input >= KN_F1 && input < (KN_F1 + Session.MaxPlayers) ) || input == PAGE_RESPOND_KEY ) && 
		!Session.Messages.Is_Edit()) {
#else
	if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH && input >= KN_F1 && input < (KN_F1 + Session.MaxPlayers) && !Session.Messages.Is_Edit()) {
#endif
		memset (txt, 0, 40);

		/*
		**	For a network game:
		**	F1-F7 = "To <name> (house):" (only allowed if we're not in ObiWan mode)
		**	F8 = "To All:"
		*/
		if ((Session.Type == GAME_UDP || Session.Type == GAME_INTERNET
#if RA_ENABLE_WOL
			|| Session.Type == GAME_WOL
#endif
		) && !Session.Messages.Is_Edit()) {
		/*
		**	Network game message addressing
		*/
			if (input==(KN_F1 + Session.MaxPlayers - 1)) {

				Session.MessageAddress = UDPAddressClass();		// set to broadcast
				strcpy(txt, Text_String(TXT_TO_ALL));	// "To All:"

				Session.Messages.Add_Edit(Session.ColorIdx,
					TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, txt, 0, 232 * RESFACTOR);

				Map.Flag_To_Redraw(false);

#ifdef WOLAPI_INTEGRATION
			} else if ((input - KN_F1) < Active_Net_Mgr()->Num_Connections() && !Session.ObiWan && input != PAGE_RESPOND_KEY ) {
#else
			} else if ((input - KN_F1) < Active_Net_Mgr()->Num_Connections() && !Session.ObiWan) {
#endif
				id = Active_Net_Mgr()->Connection_ID(input - KN_F1);
				Session.MessageAddress = (*(Active_Net_Mgr()->Connection_Address (id)));
				sprintf(txt, Text_String(TXT_TO), Active_Net_Mgr()->Connection_Name(id));

				Session.Messages.Add_Edit(Session.ColorIdx,
					TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, txt, 0, 232 * RESFACTOR);

				Map.Flag_To_Redraw(false);
			}
#ifdef WOLAPI_INTEGRATION
			else if( Session.Type == GAME_INTERNET && pWolapi && !pWolapi->bConnectionDown && input == PAGE_RESPOND_KEY )
			{
				if( *pWolapi->szExternalPager )
				{
					//	Respond to a page from external ww online user that paged me.
					//	Set MessageAddress to all zeroes, as a flag to ourselves later on.
					NetNumType blip;
					NetNodeType blop;
					memset( blip, 0, 4 );
					memset( blop, 0, 6 );
					Session.MessageAddress = UDPAddressClass( blip, blop );

					//	Tell pWolapi not to reset szExternalPager for the time being.
					pWolapi->bFreezeExternalPager = true;

					sprintf( txt, Text_String( TXT_TO ), pWolapi->szExternalPager );

					Session.Messages.Add_Edit(Session.ColorIdx,
						TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, txt, 0, 232 * RESFACTOR);

					Map.Flag_To_Redraw(false);

					Keyboard->Clear();
				}
				else
				{
					Session.Messages.Add_Message( NULL, 0, TXT_WOL_NOTPAGED, PCOLOR_GOLD, TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_FULLSHADOW, Rule.MessageDelay * TICKS_PER_MINUTE );
					Sound_Effect( VOC_SYS_ERROR );
				}
			}
#endif
		}
	}

	/*
	**	Process message-system input; send the message out if RETURN is hit.
	*/
	copy_input = input;
	rc = Session.Messages.Input(input);

	/*
	**	If a single character has been added to an edit buffer, update the display.
	*/
	if (rc == 1 && Session.Type != GAME_NORMAL) {
		Map.Flag_To_Redraw(false);
	}

	/*
	**	If backspace was hit, redraw the map.  If the edit message was removed,
	** the map must be force-drawn, since it won't be able to compute the
	** cells to redraw; otherwise, let the map compute the cells to redraw,
	** by not force-drawing it, but just setting the IsToRedraw bit.
	*/
	if (rc==2 && Session.Type != GAME_NORMAL) {
		if (copy_input==KN_ESC) {
			Map.Flag_To_Redraw(true);
#ifdef WOLAPI_INTEGRATION
			if( pWolapi )
				//	Just in case user was responding to a page from outside the game, and we had frozen the "szExternalPager".
				pWolapi->bFreezeExternalPager = false;
#endif
		} else {
			Map.Flag_To_Redraw(false);
		}
		Map.DisplayClass::IsToRedraw = true;
	}

	/*
	**	Send a message
	*/
	if ((rc==3 || rc==4) && Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH) {
		if (Session.Type == GAME_UDP || Session.Type == GAME_INTERNET
#if RA_ENABLE_WOL
			|| Session.Type == GAME_WOL
#endif
		) {
#ifdef WOLAPI_INTEGRATION
			NetNumType blip;
			NetNodeType blop;
			Session.MessageAddress.Get_Address( blip, blop );
			if(	blip[0] + blip[1] + blip[2] + blip[3] + blop[0] + blop[1] + blop[2] + blop[3] + blop[4] + blop[5] == 0 )
			{
				//	This message is a response to the last person that paged me.
				if( pWolapi && !pWolapi->bConnectionDown )		//	(As connection may have gone down.)
				{
					pWolapi->Page( pWolapi->szExternalPager, Session.Messages.Get_Edit_Buf(), false );
					pWolapi->bFreezeExternalPager = false;
				}
			}
			else
#endif
			{

			/*
			**	Network game: fill in a GlobalPacketType & send it.
			*/
				Session.GPacket.Command = NET_MESSAGE;
				strcpy (Session.GPacket.Name, Session.Players[0]->Name);
				Session.GPacket.Message.Color = Session.ColorIdx;
				Session.GPacket.Message.NameCRC = Compute_Name_CRC(Session.GameName);

				if (rc==3) {
					strcpy (Session.GPacket.Message.Buf, Session.Messages.Get_Edit_Buf());
				} else {
					strcpy (Session.GPacket.Message.Buf,
						Session.Messages.Get_Overflow_Buf());
					Session.Messages.Clear_Overflow_Buf();
				}

				/*
				**	If 'F4' was hit, MessageAddress will be a broadcast address; send
				**	the message to every player we have a connection with.
				*/
				if (Session.MessageAddress.Is_Broadcast()) {
					for (i = 0; i < Active_Net_Mgr()->Num_Connections(); i++) {
						Active_Net_Mgr()->Send_Global_Message(&Session.GPacket,
							sizeof(GlobalPacketType), 1,
							Active_Net_Mgr()->Connection_Address(Active_Net_Mgr()->Connection_ID(i)));
						Active_Net_Mgr()->Service();
					}
				} else {

					/*
					**	Otherwise, MessageAddress contains the exact address to send to.
					**	Send to that address only.
					*/
					Active_Net_Mgr()->Send_Global_Message(&Session.GPacket,
						sizeof(GlobalPacketType), 1,
						&Session.MessageAddress);
					Active_Net_Mgr()->Service();

				}

				/*
				**	Store this message in our LastMessage buffer; the computer may send
				**	us a version of it later.
				*/
				strcpy(Session.LastMessage, Session.GPacket.Message.Buf);
			}
		}

		/*
		**	Tell the map to completely update itself, since a message is now missing.
		*/
		Map.Flag_To_Redraw(true);
	}
}

/***********************************************************************************************
 * Color_Cycle -- Handle the general palette color cycling.                                    *
 *                                                                                             *
 *    This is a maintenance routine that handles the color cycling. It should be called as     *
 *    often as necessary to achieve smooth color cycling effects -- at least 8 times a second. *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/31/1994 JLB : Created.                                                                 *
 *   06/10/1994 JLB : Uses new cycle color values.                                             *
 *   12/21/1994 JLB : Handles text fade color.                                                 *
 *   07/16/1996 JLB : Faster pulsing of white color.                                           *
 *=============================================================================================*/
void Color_Cycle(void)
{
	static CDTimerClass<SystemTimerClass> _timer;
	static CDTimerClass<SystemTimerClass> _ftimer;
	static bool _up = false;
	static int val = 255;
	bool changed = false;

	if (Options.IsPaletteScroll) {
		/*
		**	Process the fading white color. It is used for the radar box and other glowing
		**	game interface elements.
		*/
		if (!_ftimer) {
			_ftimer = TIMER_SECOND/6;

			#define	STEP_RATE	20
			if (_up) {
				val += STEP_RATE;
				if (val > 150) {
					val = 150;
					_up = false;
				}
			} else {
				val -= STEP_RATE;
				if (val < 0x20) {
					val = 0x20;
					_up = true;
				}
			}

			/*
			**	Set the pulse color as the proportional value between white and the
			**	minimum value for pulsing.
			*/
			InGamePalette[CC_PULSE_COLOR] = GamePalette[WHITE];
			InGamePalette[CC_PULSE_COLOR].Adjust(val, BlackColor);

			/*
			**	Pulse the glowing embers between medium and dark red.
			*/
			InGamePalette[CC_EMBER_COLOR] = RGBClass(255, 80, 80);
			InGamePalette[CC_EMBER_COLOR].Adjust(val, BlackColor);

			changed = true;
		}

		/*
		**	Process the color cycling effects -- water.
		*/
		if (!_timer) {
			_timer = TIMER_SECOND/4;

			RGBClass first = InGamePalette[CYCLE_COLOR_START+CYCLE_COLOR_COUNT-1];
			for (int index = CYCLE_COLOR_START+CYCLE_COLOR_COUNT-1; index >= CYCLE_COLOR_START; index--) {
				InGamePalette[index] = InGamePalette[index-1];
			}
			InGamePalette[CYCLE_COLOR_START] = first;

			changed = true;
		}

		/*
		**	If any of the processing functions changed the palette, then this palette must be
		**	passed to the system.
		*/
		if (changed) {
			BStart(BENCH_PALETTE);
			InGamePalette.Set();
//			Set_Palette(InGamePalette);
			BEnd(BENCH_PALETTE);
		}
	}
}

/***********************************************************************************************
 * Call_Back -- Main game maintenance callback routine.                                        *
 *                                                                                             *
 *    This routine handles all the "real time" processing that needs to                        *
 *    occur. This includes palette fading and sound updating. It needs                         *
 *    to be called as often as possible.                                                       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/07/1992 JLB : Created.                                                                 *
 *=============================================================================================*/
void Call_Back(void)
{
	const bool trace_callback = Conquer_Callback_Trace_Enabled();
	if (trace_callback) {
		Conquer_Trace("Call_Back entry SessionType=%d", Session.Type);
	}

	SDL_GameInput_Pump();
	if (WWMouse) {
		/*
		** Keep the software cursor on the same main-thread SDL path as the rest
		** of the front-end input and presentation work.
		*/
		WWMouse->Process_Mouse();
	}

	/*
	**	Music and speech maintenance
	*/
	if (trace_callback) Conquer_Trace("Call_Back Sound_Callback start");
	Sound_Callback();
	if (trace_callback) Conquer_Trace("Call_Back Sound_Callback complete");
	if (trace_callback) Conquer_Trace("Call_Back Theme.AI start");
	Theme.AI();
	if (trace_callback) Conquer_Trace("Call_Back Theme.AI complete");
	if (trace_callback) Conquer_Trace("Call_Back Speak_AI start");
	Speak_AI();
	if (trace_callback) Conquer_Trace("Call_Back Speak_AI complete");

	/*
	**	Network maintenance.
	*/
	if (Session.Type == GAME_UDP || Session.Type == GAME_INTERNET
#if RA_ENABLE_WOL
		|| Session.Type == GAME_WOL
#endif
	) {
		if (trace_callback) Conquer_Trace("Call_Back UDP start");
		UDP_Call_Back();
		if (trace_callback) Conquer_Trace("Call_Back UDP complete");
	}

	/*
	**	Serial game maintenance.
	*/
#ifdef WOLAPI_INTEGRATION
	//	Wolapi maintenance.
	if( pWolapi )
	{
		if( pWolapi->bInGame )
		{
			if( !pWolapi->bConnectionDown && ::timeGetTime() > pWolapi->dwTimeNextWolapiPump )
			{
				pWolapi->pChat->PumpMessages();
				pWolapi->pNetUtil->PumpMessages();
				pWolapi->dwTimeNextWolapiPump = ::timeGetTime() + WOLAPIPUMPWAIT + 700;		//	Slower pump during games.
				if( pWolapi->bConnectionDown )
				{
					//	Connection to server lost.
					Session.Messages.Add_Message( NULL, 0, TXT_WOL_WOLAPIGONE, PCOLOR_GOLD, TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_FULLSHADOW, Rule.MessageDelay * TICKS_PER_MINUTE );
					Sound_Effect( WOLSOUND_LOGOUT );
//	ajw (Wolapi object is now left around, so we can try to send game results.)
//					//	Kill wolapi.
//					pWolapi->UnsetupCOMStuff();
//					delete pWolapi;
//					pWolapi = NULL;
				}
			}
		}
		else
		{
			//	When showing a modal dialog during chat, this pumping is turned on. It's turned off immediately following.
			if( pWolapi->bPump_In_Call_Back && ( ::timeGetTime() > pWolapi->dwTimeNextWolapiPump ) )
			{
				pWolapi->pChat->PumpMessages();
				pWolapi->pNetUtil->PumpMessages();
				pWolapi->dwTimeNextWolapiPump = ::timeGetTime() + WOLAPIPUMPWAIT;
			}
		}
	}
#endif

	if (trace_callback) {
		Conquer_Trace("Call_Back exit");
	}

	WWDraw_Flush_Present();
}

void Call_Back_Wait(int time)
{
	if (time < 0) {
		time = 0;
	}
	if (time > 60) {
		time = 60;
	}

	CDTimerClass<SystemTimerClass> timer = static_cast<uint32_t>(time);

	do {
		const bool had_pending_present = WWDraw_Has_Pending_Present();
		Call_Back();
		if (!had_pending_present && timer) {
			WWDraw_Request_Present();
		}
	} while (timer);
}

void UDP_Call_Back(void)
{
	Active_Net_Mgr()->Service();

	/*
	** Read packets only if the game is "closed", so we don't steal global
	** messages from the connection dialogs.
	*/
	if (!Session.NetOpen) {
		Session.GPacketlen = sizeof(Session.GPacket);
		if (Active_Net_Mgr()->Get_Global_Message (&Session.GPacket, &Session.GPacketlen, &Session.GAddress, &Session.GProductID)) {

			if (Session.GProductID == UDPGlobalConnClass::COMMAND_AND_CONQUER0) {

				/*
				**	If this is another player signing off, remove the connection &
				**	mark that player's house as non-human, so the computer will take
				**	it over.
				*/
				if (Session.GPacket.Command == NET_SIGN_OFF) {
					for (int i = 0; i < Active_Net_Mgr()->Num_Connections(); i++) {

						int id = Active_Net_Mgr()->Connection_ID(i);

						if (Session.GAddress == (*Active_Net_Mgr()->Connection_Address(id))) {
							Destroy_Connection(id, 0);
						}
					}
				} else {

					/*
					**	Process a message from another user.
					*/
					if (Session.GPacket.Command == NET_MESSAGE) {
						bool msg_ok = false;

						/*
						** If NetProtect is set, make sure this message came from within
						** this game.
						*/
						if (!Session.NetProtect) {
							msg_ok = true;
						} else {
							if (Session.GPacket.Message.NameCRC ==
								Compute_Name_CRC(Session.GameName)) {
								msg_ok = true;
							} else {
								msg_ok = false;
							}
						}

						if (msg_ok) {
							if (!Session.Messages.Concat_Message(Session.GPacket.Name,
								Session.GPacket.Message.Color,
								Session.GPacket.Message.Buf, Rule.MessageDelay * TICKS_PER_MINUTE)) {
								Session.Messages.Add_Message (Session.GPacket.Name,
									Session.GPacket.Message.Color,
									Session.GPacket.Message.Buf,
									Session.GPacket.Message.Color,
									TPF_6PT_GRAD | TPF_USE_GRAD_PAL |
									TPF_FULLSHADOW, Rule.MessageDelay * TICKS_PER_MINUTE);

								Sound_Effect(VOC_INCOMING_MESSAGE);
							}

							/*
							**	Tell the map to do a partial update (just to force the messages
							**	to redraw).
							*/
							Map.Flag_To_Redraw(true);

							/*
							**	Save this message in our last-message buffer
							*/
							strcpy(Session.LastMessage, Session.GPacket.Message.Buf);
						}
					} else {
						Process_Global_Packet(&Session.GPacket, &Session.GAddress);
					}
				}
			}
		}
	}

}

/***********************************************************************************************
 * Language_Name -- Build filename for current language.                                       *
 *                                                                                             *
 *    This routine attaches a language specific suffix to the base                             *
 *    filename provided. Typical use of this is when loading language                          *
 *    specific files at game initialization time.                                              *
 *                                                                                             *
 * INPUT:   basename -- Base name to append language specific                                  *
 *                      extension to.                                                          *
 *                                                                                             *
 * OUTPUT:  Returns with pointer to completed filename.                                        *
 *                                                                                             *
 * WARNINGS:   The return pointer value is valid only until the next time                      *
 *             this routine is called.                                                         *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/07/1992 JLB : Created.                                                                 *
 *=============================================================================================*/
char const * Language_Name(char const * basename)
{
	static char _fullname[_MAX_FNAME+_MAX_EXT];

	if (!basename) return(NULL);

	sprintf(_fullname, "%s.ENG", basename);
	return(_fullname);
}

/***********************************************************************************************
 * Source_From_Name -- Converts ASCII name into SourceType.                                    *
 *                                                                                             *
 *    This routine is used to convert an ASCII name representing a                             *
 *    SourceType into the actual SourceType value. Typically, this is                          *
 *    used when processing the scenario INI file.                                              *
 *                                                                                             *
 * INPUT:   name  -- The ASCII source name to process.                                         *
 *                                                                                             *
 * OUTPUT:  Returns with the SourceType represented by the name                                *
 *          specified.                                                                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   04/17/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
SourceType Source_From_Name(char const * name)
{
	if (name) {
		for (SourceType source = SOURCE_FIRST; source < SOURCE_COUNT; source++) {
			if (SDL_strcasecmp(SourceName[source], name) == 0) {
				return(source);
			}
		}
	}
	return(SOURCE_NONE);
}

/***********************************************************************************************
 * Name_From_Source -- retrieves the name for the given SourceType         						  *
 *                                                                         						  *
 * INPUT:                                                                  						  *
 *		source		SourceType to get the name for															  *
 *                                                                         						  *
 * OUTPUT:                                                                 						  *
 *		name of SourceType																							  *
 *                                                                         						  *
 * WARNINGS:                                                               						  *
 *		none.																												  *
 *                                                                         						  *
 * HISTORY:                                                                						  *
 *   11/15/1994 BR : Created.                                              						  *
 *=============================================================================================*/
char const * Name_From_Source(SourceType source)
{
	if ((unsigned)source < SOURCE_COUNT) {
		return(SourceName[source]);
	}
	return("None");
}

/***********************************************************************************************
 * Theater_From_Name -- Converts ASCII name into a theater number.                             *
 *                                                                                             *
 *    This routine converts an ASCII representation of a theater and converts it into a        *
 *    matching theater number. If no match was found, then THEATER_NONE is returned.           *
 *                                                                                             *
 * INPUT:   name  -- Pointer to ASCII name to convert.                                         *
 *                                                                                             *
 * OUTPUT:  Returns with the name converted into a theater number.                             *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/01/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
TheaterType Theater_From_Name(char const * name)
{
	TheaterType	index;

	if (name) {
		for (index = THEATER_FIRST; index < THEATER_COUNT; index++) {
			if (SDL_strcasecmp(name, Theaters[index].Name) == 0) {
				return(index);
			}
		}
	}
	return(THEATER_NONE);
}

/***********************************************************************************************
 * KN_To_Facing -- Converts a keyboard input number into a facing value.                       *
 *                                                                                             *
 *    This routine determine which compass direction is represented by the keyboard value      *
 *    provided. It is used for map scrolling and other directional control operations from     *
 *    the keyboard.                                                                            *
 *                                                                                             *
 * INPUT:   input -- The KN number to convert.                                                 *
 *                                                                                             *
 * OUTPUT:  Returns with the facing type that the keyboard number represents. If it could      *
 *          not be translated, then FACING_NONE is returned.                                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/28/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
FacingType KN_To_Facing(int input)
{
	input &= ~(KN_ALT_BIT|KN_SHIFT_BIT|KN_CTRL_BIT);
	switch (input) {
		case KN_LEFT:
			return(FACING_W);

		case KN_RIGHT:
			return(FACING_E);

		case KN_UP:
			return(FACING_N);

		case KN_DOWN:
			return(FACING_S);

		case KN_UPLEFT:
			return(FACING_NW);

		case KN_UPRIGHT:
			return(FACING_NE);

		case KN_DOWNLEFT:
			return(FACING_SW);

		case KN_DOWNRIGHT:
			return(FACING_SE);

		default:
			break;
	}
	return(FACING_NONE);
}

static void Reset_Main_Loop_Frame_State(void)
{
	FrameTimer = 0;
	MainLoopLastServiceTick = 0;
}

static int Main_Loop_Frame_Interval(void)
{
	int framedelay;

	if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH &&
		Session.CommProtocol == COMM_PROTOCOL_MULTI_E_COMP) {

		if (Session.Play) {
			return(0);
		}
#ifdef FIXIT_VERSION_3
		if (!Session.DesiredFrameRate) {
			Session.DesiredFrameRate = 60;
		}
#endif
		framedelay = TIMER_SECOND / Session.DesiredFrameRate;
	} else {
		if (Options.GameSpeed != 0) {
			framedelay = Options.GameSpeed +
				(PlayerPtr->Difficulty == DIFF_EASY ? 1 : 0) -
				(PlayerPtr->Difficulty == DIFF_HARD ? 1 : 0);
		} else {
			framedelay = Options.GameSpeed + (PlayerPtr->Difficulty == DIFF_EASY ? 1 : 0);
		}
	}

	return(MAX(framedelay, 0));
}

static bool Main_Loop_Frame_Ready(int frameinterval)
{
	const uint32_t now = TickCount;
	const uint32_t interval = static_cast<uint32_t>(MAX(frameinterval, 0));

	if (MainLoopLastServiceTick != 0 && now >= MainLoopLastServiceTick && FrameTimer) {
		SpareTicks += now - MainLoopLastServiceTick;
	}

	if (!FrameTimer) {
		FrameTimer = interval;
		MainLoopLastServiceTick = now;
		return(true);
	}

	if (now < MainLoopLastServiceTick) {
		MainLoopLastServiceTick = now;
		return(false);
	}

	MainLoopLastServiceTick = now;
	return(false);
}

static bool Run_Game_Frame(void)
{
	Session.ProcessTimer = TickCount;

	/*
	** Save map's position & selected objects, if we're recording the game.
	*/
	if (Session.Record || Session.Play) {
		Do_Record_Playback();
	}

#ifndef SORTDRAW
	/*
	** Sort the map's ground layer by y-coordinate value.  This is done
	** outside the IsToRedraw check, for the purposes of game sync'ing
	** between machines; this way, all machines will sort the Map's
	** layer in the same way, and any processing done that's based on
	** the order of this layer will remain in sync.
	*/
	DisplayClass::Layer[LAYER_GROUND].Sort();
#endif

	/*
	**	AI logic operations are performed here.
	*/
	Logic.AI();
	TimeQuake = false;
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
	if (!PendingTimeQuake) {
		TimeQuakeCenter = 0;
	}
#endif

	/*
	**	Manage the inter-player message list.  If Manage() returns true, it means
	**	a message has expired & been removed, and the entire map must be updated.
	*/
	if (Session.Messages.Manage()) {
		HiddenPage.Clear();
		Map.Flag_To_Redraw(true);
	}

	//
	// Measure how long it took to process the AI
	//
	Session.ProcessTicks += (TickCount - Session.ProcessTimer);
	Session.ProcessFrames++;

	/*
	**	Process all commands that are ready to be processed.
	*/
	Queue_AI();

	/*
	**	Keep track of elapsed time in the game.
	*/
	Score.ElapsedTime += TIMER_SECOND / TICKS_PER_SECOND;

	Call_Back();

	/*
	**	Check for player wins or loses according to global event flag.
	*/
	if (PlayerWins) {

		if (Session.Type == GAME_INTERNET && !GameStatisticsPacketSent) {
			Register_Game_End_Time();
			Send_Statistics_Packet();		//	Player just won.
		}

		WWMouse->Erase_Mouse(&HidPage, TRUE);
		PlayerLoses = false;
		PlayerWins = false;
		PlayerRestarts = false;
		Map.Help_Text(TXT_NONE);
		Do_Win();
		return(true);
	}
	if (PlayerLoses) {

		if (Session.Type == GAME_INTERNET && !GameStatisticsPacketSent) {
			Register_Game_End_Time();
			Send_Statistics_Packet();		//	Player just lost.
		}

		WWMouse->Erase_Mouse(&HidPage, TRUE);
		PlayerWins = false;
		PlayerLoses = false;
		PlayerRestarts = false;
		Map.Help_Text(TXT_NONE);
		Do_Lose();
		return(true);
	}
	if (PlayerRestarts) {
		WWMouse->Erase_Mouse(&HidPage, TRUE);
		PlayerWins = false;
		PlayerLoses = false;
		PlayerRestarts = false;
		Map.Help_Text(TXT_NONE);
		Do_Restart();
		return(true);
	}

#ifdef FIXIT_VERSION_3		//	Stalemate games.
	if( Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH && Session.Players.Count() == 2 &&
		Scen.bLocalProposesDraw && Scen.bOtherProposesDraw )
	{
		//	End game in a draw.
		if (Session.Type == GAME_INTERNET && !GameStatisticsPacketSent) {
			Register_Game_End_Time();
			Send_Statistics_Packet();
		}
		WWMouse->Erase_Mouse(&HidPage, TRUE);
		Map.Help_Text(TXT_NONE);
		Do_Draw();
		return(true);
	}
#endif

	/*
	**	The frame logic has been completed. Increment the frame
	**	counter.
	*/
	Frame++;

	/*
	** Is there a memory trasher altering the map??
	*/
	if (Debug_Check_Map) {
		if (!Map.Validate()) {
			if (WWMessageBox().Process (TEXT_MAP_ERROR, TEXT_STOP, TEXT_CONTINUE)==0) {
				GameActive = 0;
			}
			Map.Validate();		// give debugger a chance to catch it
		}
	}

	if (Debug_MotionCapture) {
		static void ** _array = 0;
		static int _sequence = 0;
		static int _seqsize = Rule.MovieTime * TICKS_PER_MINUTE;

		if (_array == NULL) {
			_array = new void * [_seqsize];
			memset(_array, '\0', _seqsize * sizeof(void*));
		}

		if (_array == NULL) {
			Debug_MotionCapture = false;
		}

		static GraphicBufferClass temp_page(	SeenBuff.Get_Width(),
													SeenBuff.Get_Height(),
													NULL,
													SeenBuff.Get_Width() * SeenBuff.Get_Height());

		int size = SeenBuff.Get_Width() * SeenBuff.Get_Height();

		if (_sequence < _seqsize) {
			if (_array[_sequence] == NULL) {
				_array[_sequence] = new char[size];
			}

			if (_array[_sequence] != NULL) {
				SeenBuff.Blit(temp_page);
				memmove(_array[_sequence], temp_page.Get_Buffer(), size);
			}
			_sequence++;

		} else {
			Debug_MotionCapture = false;

			CCFileClass file;
			file.Cache(200000);
			char filename[30];

			for (int index = 0; index < _sequence; index++) {
				memmove(temp_page.Get_Buffer(), _array[index], size);
				sprintf(filename, "cap%04d.pcx", index);
				file.Set_Name(filename);

				Write_PCX_File(file, temp_page, & GamePalette);
			}

			_sequence = 0;
		}
	}

	return(!GameActive);
}

/***********************************************************************************************
 * Main_Loop -- This is the main game loop (as a single loop).                                 *
 *                                                                                             *
 *    This function will perform one game loop.                                                *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Should the game end?                                                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/01/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
extern void Check_For_Focus_Loss(void);
void Reallocate_Big_Shape_Buffer(void);

bool Main_Loop()
{
	KeyNumType	input;					// Player input.
	int x;
	int y;

Mono_Set_Cursor(0,0);

	if (!GameActive) return(!GameActive);

	/*
	** Call the focus loss handler
	*/
	Check_For_Focus_Loss();

	/*
	** Allocate extra memory for uncompressed shapes as needed
	*/
	Reallocate_Big_Shape_Buffer();

	/*
	** Sync-bug trapping code
	*/
	if (Frame >= Session.TrapFrame) {
		Session.Trap_Object();
	}

	//
	// Initialize our AI processing timer
	//
	Session.ProcessTimer = TickCount;

#if 1
	if (Session.TrapCheckHeap) {
		Debug_Trap_Check_Heap = true;
	}
#endif

#ifdef CHEAT_KEYS

	/*
	**	Update the running status debug display.
	*/
	Self_Regulate();
#endif

	BStart(BENCH_GAME_FRAME);

	/*
	**	If there is no theme playing, but it looks like one is required, then start one
	**	playing. This is usually the symptom of there being no transition score.
	*/
	if (Theme.What_Is_Playing() == THEME_NONE) {
		Theme.Queue_Song(THEME_PICK_ANOTHER);
	}

	/*
	**	Update the display, unless we're inside a dialog.
	*/
	const bool frame_ready = Main_Loop_Frame_Ready(Main_Loop_Frame_Interval());
	const bool update_map = (SpecialDialog == SDLG_NONE && GameInFocus);

	if (update_map) {
		WWMouse->Erase_Mouse(&HidPage, TRUE);
		if (!Session.Play) {
			Map.Input(input, x, y);
			if (input) {
				Keyboard_Process(input);
			}
		}
	}

	if (frame_ready) {
		if (Run_Game_Frame()) {
			BEnd(BENCH_GAME_FRAME);
			return(!GameActive);
		}
	}

	Color_Cycle();

	if (update_map) {
		Map.Render();
	}

	if (frame_ready) {
		WWDraw_Flush_Present();
	} else {
		Call_Back();
	}

	BEnd(BENCH_GAME_FRAME);
	return(!GameActive);
}

#ifdef SCENARIO_EDITOR
/***************************************************************************
 * Map_Edit_Loop -- a mini-main loop for map edit mode only                *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   10/19/1994 BR : Created.                                              *
 *=========================================================================*/
bool Map_Edit_Loop(void)
{
	/*
	**	Redraw the map.
	*/
	Map.Render();

	/*
	**	Get user input (keys, mouse clicks).
	*/
	KeyNumType input;

	WWMouse->Erase_Mouse(&HidPage, TRUE);

	int x;
	int y;
	Map.Input(input, x, y);

	/*
	**	Process keypress.
	*/
	if (input) {
		Keyboard_Process(input);
	}

	Call_Back();								// maintains Theme.AI() for music
	Color_Cycle();

	return(!GameActive);
}

/***************************************************************************
 * Go_Editor -- Enables/disables the map editor										*
 *                                                                         *
 * INPUT:                                                                  *
 *		flag		true = go into editor mode; false = go into game mode			*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   10/19/1994 BR : Created.                                              *
 *=========================================================================*/
void Go_Editor(bool flag)
{
	/*
	**	Go into Scenario Editor mode
	*/
	if (flag) {
		Debug_Map = true;
		Debug_Unshroud = true;

		/*
		** Un-select any selected objects
		*/
		Unselect_All();

		/*
		** Turn off the sidebar if it's on
		*/
		Map.Activate(0);

		/*
		** Reset the map's Button list for the new mode
		*/
		Map.Init_IO();

		/*
		** Force a complete redraw of the screen
		*/
		HiddenPage.Clear();
		Map.Flag_To_Redraw(true);
		Map.Render();

	} else {

		/*
		**	Go into normal game mode
		*/
		Debug_Map = false;
		Debug_Unshroud = false;

		/*
		** Un-select any selected objects
		*/
		Unselect_All();

		/*
		** Reset the map's Button list for the new mode
		*/
		Map.Init_IO();

		/*
		** Force a complete redraw of the screen
		*/
		HidPage.Clear();
		Map.Flag_To_Redraw(true);
		Map.Render();
	}
}

#endif

/***********************************************************************************************
 * MixFileHandler -- Handles VQ file access.                                                   *
 *                                                                                             *
 *    This routine is called from the VQ player when it needs to access the source file. By    *
 *    using this routine it is possible to virtualize the file system.                         *
 *                                                                                             *
 * INPUT:   vqa   -- Pointer to the VQA handle for this animation.                             *
 *                                                                                             *
 *          action-- The requested action to perform.                                          *
 *                                                                                             *
 *          buffer-- Optional buffer pointer as needed by the type of action.                  *
 *                                                                                             *
 *          nbytes-- The number of bytes (if needed) for this operation.                       *
 *                                                                                             *
 * OUTPUT:  Returns a value consistent with the action requested.                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/04/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
int32_t MixFileHandler(VQAHandle * vqa, int32_t action, void * buffer, int32_t nbytes)
{
	CCFileClass * file;
	int32_t     error;

	file = (CCFileClass *)vqa->VQAio;

	/*
	**	Perform the action specified by the stream command.
	*/
	switch (action) {

		/*
		** VQACMD_READ means read NBytes from the stream and place it in the
		** memory pointed to by Buffer.
		**
		** Any error code returned will be remapped by VQA library into
		** VQAERR_READ.
		*/
		case VQACMD_READ:
			error = (file->Read(buffer, (uint16_t)nbytes) != (uint16_t)nbytes);
			break;

		/*
		**	VQACMD_WRITE is analogous to VQACMD_READ.
		**
		** Writing is not allowed to the VQA file, VQA library will remap the
		** error into VQAERR_WRITE.
		*/
		case VQACMD_WRITE:
			error = 1;
			break;

		/*
		**	VQACMD_SEEK asks that you perform a seek relative to the current
		** position. NBytes is a signed number, indicating seek direction
		** (positive for forward, negative for backward). Buffer has no meaning
		** here.
		**
		** Any error code returned will be remapped by VQA library into
		** VQAERR_SEEK.
		*/
		case VQACMD_SEEK:
			error = (file->Seek(nbytes, SEEK_CUR) == -1);
			break;

		/*
		**	VQACMD_OPEN asks that you open your stream for access.
		*/
		case VQACMD_OPEN:
			file = new CCFileClass((char *)buffer);

			if (file != NULL && file->Is_Available()) {
				error = file->Open((char *)buffer, READ);

				if (error != -1) {
					vqa->VQAio = reinterpret_cast<uintptr_t>(file);
					error = 0;
				} else {
					delete file;
					file = 0;
					error = 1;
				}
			} else {
				error = 1;
			}
			break;

		case VQACMD_CLOSE:
			file->Close();
			delete file;
			file = 0;
			vqa->VQAio = 0;
			error = 0;
			break;

		/*
		**	VQACMD_INIT means to prepare your stream for reading. This is used for
		** certain streams that can't be read immediately upon opening, and need
		** further preparation. This operation is allowed to fail; the error code
		** will be returned directly to the client.
		*/
		case VQACMD_INIT:

		/*
		**	IFFCMD_CLEANUP means to terminate the transaction with the associated
		** stream. This is used for streams that can't simply be closed. This
		** operation is not allowed to fail; any error returned will be ignored.
		*/
		case VQACMD_CLEANUP:
			error = 0;
			break;

		default:
			error = 0;
			break;
	}

	return(error);
}

void Rebuild_Interpolated_Palette(uint8_t * interpal)
{
	for (int y=0; y<255; y++) {
		for (int x=y+1; x<256; x++) {
			*(interpal + (y*256+x)) = *(interpal + (x*256+y));
		}
	}
}

uint8_t 	* InterpolatedPalettes[100];
int32_t				PalettesRead;
unsigned			PaletteCounter;

/***********************************************************************************************
 * Load_Interpolated_Palettes -- Loads in any precalculated palettes for hires VQs             *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Name of palette file                                                              *
 *                                                                                             *
 * OUTPUT:   Number of palettes loaded                                                         *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    5/7/96 9:49AM ST : Created                                                               *
 *=============================================================================================*/
int Load_Interpolated_Palettes(char const * filename, int32_t add)
{
	int	num_palettes=0;
	int	i;
	int 	start_palette;

	PalettesRead = FALSE;
	CCFileClass	file(filename);

	if (!add) {
		for (i = 0; i < static_cast<int>(ARRAY_SIZE(InterpolatedPalettes)); i++) {
			InterpolatedPalettes[i]=NULL;
		}
		start_palette=0;
	} else {
		for (start_palette = 0; start_palette < static_cast<int>(ARRAY_SIZE(InterpolatedPalettes)); start_palette++) {
			if (!InterpolatedPalettes[start_palette]) break;
		}
	}

	/*
	**	Hack another interpolated palette if the requested one is
	**	not present.
	*/
	if (!file.Is_Available()) {
		file.Set_Name("AAGUN.VQP");
	}

	if (file.Is_Available()) {

		file.Open(READ);
		file.Read(&num_palettes , 4);

		for (i=0; i < num_palettes; i++) {
			InterpolatedPalettes[i+start_palette] = (uint8_t *)malloc (65536);
			memset (InterpolatedPalettes[i+start_palette], 0, 65536);
			for (int y = 0; y < 256; y++) {
				file.Read (InterpolatedPalettes[i+start_palette] + y*256 , y+1);
			}

			Rebuild_Interpolated_Palette(InterpolatedPalettes[i+start_palette]);
		}

		PalettesRead = TRUE;
		file.Close();
	}
	PaletteCounter = 0;
	return (num_palettes);
}

void Free_Interpolated_Palettes(void)
{
	for (int i = 0; i < static_cast<int>(ARRAY_SIZE(InterpolatedPalettes)); i++) {
		if (InterpolatedPalettes[i]) {
			free(InterpolatedPalettes[i]);
			InterpolatedPalettes[i]=NULL;
		}
	}
}

/***********************************************************************************************
 * Play_Movie -- Plays a VQ movie.                                                             *
 *                                                                                             *
 *    Use this routine to play a VQ movie. It will dispatch the specified movie to the         *
 *    VQ player. The routine will not return until the movie has finished playing.             *
 *                                                                                             *
 * INPUT:   name  -- The name of the movie file (sans ".VQA").                                 *
 *                                                                                             *
 *          theme -- The identifier for an optional theme that should be played in the         *
 *                   background while this VQ plays.                                           *
 *                                                                                             *
 *          clrscrn -- 'true' if to clear the screen when the movie is over                    *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/19/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
#ifdef MOVIE640
extern GraphicBufferClass VQ640;
#endif
bool Try_Play_Movie(char const * name, ThemeType theme, bool clrscrn)
	{
	#ifdef CHEAT_KEYS
	//	Mono_Printf("Movie: %s\n", name);
	#endif	//CHEAT_KEYS
	/*
	** Don't play movies in editor mode
	*/
	if (Debug_Map) {
		return(false);
	}
	#ifdef CHEAT_KEYS
	//	Mono_Printf("A\n");
	#endif	//CHEAT_KEYS
	/*
	** Don't play movies in multiplayer mode
	*/
	if (Session.Type != GAME_NORMAL) {
		return(false);
	}
	#ifdef CHEAT_KEYS
	//Mono_Printf("b\n");
	#endif	//CHEAT_KEYS

	if (name) {
		SDL_GameInput_PushMouseCaptureSuppression();
		Conquer_Trace("Play_Movie begin name=%s theme=%d clrscrn=%d", name, theme, clrscrn ? 1 : 0);
		bool played = false;
		char fullname[_MAX_FNAME+_MAX_EXT];
		WWFS_MakePath(fullname, NULL, NULL, name, ".VQA");
		char palname [_MAX_FNAME+_MAX_EXT];
		WWFS_MakePath(palname , NULL, NULL, name, ".VQP");
		#ifdef CHEAT_KEYS
		//			Mono_Set_Cursor(0, 0);Mono_Printf("[%s]", fullname);
		#endif

		if (!CCFileClass(fullname).Is_Available()){
			Conquer_Trace("Play_Movie unavailable file=%s", fullname);
			#ifdef CHEAT_KEYS
			//		 Mono_Printf("fullname: %s\n", fullname);
			#endif	//CHEAT_KEYS
			SDL_GameInput_PopMouseCaptureSuppression();
		 return(false);
		}
		/*
		**	Reset the anim control structure.
		*/
		Anim_Init();

		/*
		**	Prepare to play a movie. First hide the mouse and stop any score that is playing.
		**	While the score (if any) is fading to silence, fade the palette to black as well.
		**	When the palette has finished fading, wait until the score has finished fading
		**	before launching the movie.
		*/
		Hide_Mouse();
		Theme.Queue_Song(theme);
		if (PreserveVQAScreen == 0 && !clrscrn) {
			BlackPalette.Set(FADE_PALETTE_MEDIUM);
			VisiblePage.Clear();
			BlackPalette.Adjust(0x08, WhitePalette);
			BlackPalette.Set();
			BlackPalette.Adjust(0xFF);
			BlackPalette.Set();
		}
		PreserveVQAScreen = 0;
		Keyboard->Clear();

		VQAHandle * vqa = NULL;

		#ifdef MOVIE640
		if(IsVQ640) {
			AnimControl.ImageWidth = 640;
			AnimControl.ImageHeight = 400;
			AnimControl.ImageBuf = (uint8_t *)VQ640.Get_Offset();
		} else {
			AnimControl.ImageWidth = 320;
			AnimControl.ImageHeight = 200;
			AnimControl.ImageBuf = (uint8_t *)SysMemPage.Get_Offset();
		}
		#endif

		if (!Debug_Quiet) {
			AnimControl.OptionFlags |= VQAOPTF_AUDIO;
		} else {
			AnimControl.OptionFlags &= ~VQAOPTF_AUDIO;
		}
		AnimControl.Volume = Movie_Playback_Volume();
		Conquer_Trace("Play_Movie prepared file=%s size=%ldx%ld audio=%d volume=%ld", fullname, AnimControl.ImageWidth, AnimControl.ImageHeight,
			(AnimControl.OptionFlags & VQAOPTF_AUDIO) ? 1 : 0, AnimControl.Volume);

		if ((vqa = VQA_Alloc()) != NULL) {
			VQA_Init(vqa, MixFileHandler);

			const int32_t open_result = VQA_Open(vqa, fullname, &AnimControl);
			if (open_result == 0) {
				Conquer_Trace("Play_Movie VQA_Open success file=%s", fullname);
				played = true;
				Brokeout = false;
				#ifdef MOVIE640
				if(!IsVQ640) {
					Load_Interpolated_Palettes(palname);
				}
				#else
				Load_Interpolated_Palettes(palname);
				#endif
				//Set_Palette(BlackPalette);
				SysMemPage.Clear();
				InMovie = true;
				Conquer_Trace("Play_Movie VQA_Play start file=%s", fullname);
				VQA_Play(vqa, VQAMODE_RUN);
				Conquer_Trace("Play_Movie VQA_Play complete file=%s brokeout=%d", fullname, Brokeout ? 1 : 0);
				VQA_Close(vqa);
				InMovie = FALSE;
				#ifdef MOVIE640
				if(!IsVQ640) {
					Free_Interpolated_Palettes();
				}
				#else
				Free_Interpolated_Palettes();
				#endif
				IsVQ640 = false;
				Set_Primary_Buffer_Format();

				/*
				**	Any movie that ends prematurely must have the screen
				**	cleared to avoid any unexpected palette glitches.
				*/
				if (Brokeout) {
					clrscrn = true;
					VisiblePage.Clear();
					Brokeout = false;
				}
			} else {
				Conquer_Trace("Play_Movie VQA_Open failed file=%s code=%ld", fullname, open_result);
				#ifndef NDEBUG
				bool error = true;
				assert(error);
				#endif
			}

			VQA_Free(vqa);
		} else {
			Conquer_Trace("Play_Movie VQA_Alloc failed file=%s", fullname);
			assert(vqa != NULL);
		}
		#ifdef CHEAT_KEYS
		//Mono_Printf("d");
		#endif	//CHEAT_KEYS
		/*
		**	Presume that the screen is left in a garbage state as well as the palette
		**	being in an unknown condition. Recover from this by clearing the screen and
		**	forcing the palette to black.
		*/
		if (clrscrn) {
			VisiblePage.Clear();
			BlackPalette.Adjust(0x08, WhitePalette);
			BlackPalette.Set();
			BlackPalette.Adjust(0xFF);
			BlackPalette.Set();
		}
		Show_Mouse();
		Conquer_Trace("Play_Movie end name=%s clrscrn=%d played=%d", name, clrscrn ? 1 : 0, played ? 1 : 0);
		SDL_GameInput_PopMouseCaptureSuppression();
		return(played);
	}
	return(false);
}

void Play_Movie(char const * name, ThemeType theme, bool clrscrn)
{
	(void)Try_Play_Movie(name, theme, clrscrn);
}

bool Try_Play_Movie(VQType name, ThemeType theme, bool clrscrn)
{
	Conquer_Trace("Play_Movie enum name=%d theme=%d clrscrn=%d session=%d", name, theme, clrscrn ? 1 : 0, Session.Type);
	if (name != VQ_NONE) {
		if (name == VQ_REDINTRO) {
			IsVQ640 = true;
		}
		const bool played = Try_Play_Movie(VQName[name], theme, clrscrn);
		IsVQ640 = false;
		return(played);
	}
	return(false);
}

void Play_Movie(VQType name, ThemeType theme, bool clrscrn)
{
	(void)Try_Play_Movie(name, theme, clrscrn);
}

/***********************************************************************************************
 * Unselect_All -- Causes all selected objects to become unselected.                           *
 *                                                                                             *
 *    This routine will unselect all objects that are currently selected.                      *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void Unselect_All(void)
{
	while (CurrentObject.Count()) {
		CurrentObject[0]->Unselect();
	}
}

/***********************************************************************************************
 * Fading_Table_Name -- Builds a theater specific fading table name.                           *
 *                                                                                             *
 *    This routine builds a standard fading table name. This name is dependant on the theater  *
 *    being played, since each theater has its own palette.                                    *
 *                                                                                             *
 * INPUT:   base  -- The base name of this fading table. The base name can be no longer than   *
 *                   seven characters.                                                         *
 *                                                                                             *
 *          theater  -- The theater that this fading table is specific to.                     *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the constructed fading table filename. This pointer is   *
 *          valid until this function is called again.                                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
char const * Fading_Table_Name(char const * base, TheaterType theater)
{
	static char _buffer[_MAX_FNAME+_MAX_EXT];
	char root[_MAX_FNAME];

	sprintf(root, "%1.1s%s", Theaters[theater].Root, base);
	WWFS_MakePath(_buffer, NULL, NULL, root, ".MRF");
	return(_buffer);
}

/***********************************************************************************************
 * Get_Radar_Icon -- Builds and alloc a radar icon from a shape file                           *
 *                                                                                             *
 * INPUT:      void const * shapefile - pointer to a key framed shapefile                      *
 *             int shapenum          - shape to extract from shapefile                         *
 *                                                                                             *
 * OUTPUT:     void const *           - 3/3 icon set of shape from file                        *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   04/12/1995 PWG : Created.                                                                 *
 *   05/10/1995 JLB : Handles a null shapefile pointer.                                        *
 *=============================================================================================*/
void const * Get_Radar_Icon(void const * shapefile, int shapenum, int frames, int zoomfactor)
{
	static int _offx[]={	0, 0,  -1,  1, 0, -1, 1, -1, 1};
	static int _offy[]={	0, 0,  -1,  1, 0, -1, 1, -1, 1};
	int lp,framelp;
	char pixel;

	char * retval = NULL;
	char * buffer = NULL;

	/*
	**	If there is no shape file, then there can be no radar icon imagery.
	*/
	if (!shapefile) return(NULL);

#if (0)
	CCPalette.Set();
	Set_Logic_Page(SeenBuff);
	CC_Draw_Shape(shapefile, shapenum, 64, 64, WINDOW_MAIN, SHAPE_WIN_REL);
#endif

	/*
	** Get the pixel width and height of the frame we built.  This will
	** be used to extract icons and build pixels.
	*/
	int pixel_width  = Get_Build_Frame_Width( shapefile );
	int pixel_height = Get_Build_Frame_Height( shapefile );

	/*
	** Find the width and height in icons, adjust these by half an
	** icon because the artists may be sloppy and miss the edge of an
	** icon one way or the other.
	*/
	int icon_width	 = (pixel_width + 12) / 24;
	int icon_height = (pixel_height + 12) / 24;

	/*
	** If we have been told to build as many frames as possible, then
	** find out how many frames there are to build.
	*/
	if (frames == -1) frames = Get_Build_Frame_Count( shapefile );

	/*
	** Allocate a position to store our icons.  If the alloc fails then
	** we don't add these icons to the set.
	**/
	buffer = new char[(icon_width * icon_height * 9 * frames)+2];
	if (!buffer) return(NULL);

	/*
	** Save off the return value so that we can return it to the calling
	** function.
	*/
	retval	 = (char *)buffer;
	*buffer++ = (char)icon_width;
	*buffer++ = (char)icon_height;
	int val = 24/zoomfactor;

	for (framelp = 0; framelp < frames; framelp ++) {
		/*
		** Build the current frame.  If the frame can not be built then we
		** just need to skip past this set of icons and try to build the
		** next frame.
		*/
		void * ptr;
		if ((ptr = Build_Frame(shapefile, shapenum + framelp, SysMemPage.Get_Buffer())) != NULL) {
			ptr = Get_Shape_Header_Data(ptr);

			/*
			** Loop through the icon width and the icon height building icons
			** into the buffer pointer.  When the getx or gety falls outside of
			** the width and height of the shape, just insert transparent pixels.
			*/
			for (int icony = 0; icony < icon_height; icony ++) {
				for (int iconx = 0; iconx < icon_width; iconx ++) {

					for (int y = 0; y < zoomfactor; y++) {
						for (int x = 0; x < zoomfactor; x++) {
							int getx = (iconx * 24) + (x * val) + (zoomfactor / 2);
							int gety = (icony * 24) + (y * val) + (zoomfactor / 2);
							if ((getx < pixel_width) && (gety < pixel_height)) {
								for (lp = 0; lp < 9; lp ++) {
									pixel	= *(char *)((char *)ptr + ((gety - _offy[lp]) * pixel_width) + getx-_offx[lp]);

									if (pixel == LTGREEN) pixel = 0;
									if (pixel) {
										break;
									}
								}
								*buffer++ = pixel;
							} else {
								*buffer++ = 0;
							}
						}
					}
				}
			}
		} else {
			buffer += icon_width * icon_height * 9;
		}
	}
	return(retval);
}

/***********************************************************************************************
 * CC_Draw_Shape -- Custom draw shape handler.                                                 *
 *                                                                                             *
 *    All draw shape calls will route through this function. It handles all draws for          *
 *    C&C. Such draws always occur to the logical page and assume certain things about         *
 *    the parameters passed.                                                                   *
 *                                                                                             *
 * INPUT:   shapefile   -- Pointer to the shape data file. This data file contains all the     *
 *                         embedded shapes.                                                    *
 *                                                                                             *
 *          shapenum    -- The shape number within the shapefile that will be drawn.           *
 *                                                                                             *
 *          x,y         -- The pixel coordinates to draw the shape.                            *
 *                                                                                             *
 *          window      -- The clipping window to use.                                         *
 *                                                                                             *
 *          flags       -- The custom draw shape flags. This controls how the parameters       *
 *                         are used (if any).                                                  *
 *                                                                                             *
 *          fadingdata  -- If SHAPE_FADING is desired, then this points to the fading          *
 *                         data table.                                                         *
 *                                                                                             *
 *          ghostdata   -- If SHAPE_GHOST is desired, then this points to the ghost remap      *
 *                         table.                                                              *
 *                                                                                             *
 *          rotation    -- Rotation to apply to the shape (DIR_N = no rotation at all).        *
 *                                                                                             *
 *          scale       -- 24.8 fixed point scale factor.                                      *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/21/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void CC_Draw_Shape(void const * shapefile, int shapenum, int x, int y, WindowNumberType window, ShapeFlags_Type flags, void const * fadingdata, void const * ghostdata, DirType rotation, int32_t scale)
{
	int predoffset;
	void *	shape_pointer;

	/*
	** Special kludge for E3 to prevent crashes
	*/
	if ((flags & SHAPE_GHOST) && (!ghostdata)) {
		ghostdata = DisplayClass::SpecialGhost;
	}
	if ((flags & SHAPE_FADING) && (!fadingdata)) {
		fadingdata = DisplayClass::FadingShade;
	}

	static uint8_t * _xbuffer = 0;

	if (!_xbuffer) {
		_xbuffer = new uint8_t[SHAPE_BUFFER_SIZE];
	}

	if (shapefile != NULL && shapenum != -1) {

		int width = Get_Build_Frame_Width(shapefile);
		int height = Get_Build_Frame_Height(shapefile);


		/*
		** In WIn95, build shape returns a pointer to the shape not its size
		*/
		shape_pointer = Build_Frame(shapefile, shapenum, _ShapeBuffer);
		if (shape_pointer) {
			GraphicViewPortClass draw_window(LogicPage->Get_Graphic_Buffer(),
														WindowList[window][WINDOWX] + LogicPage->Get_XPos(),
														WindowList[window][WINDOWY] + LogicPage->Get_YPos(),
														WindowList[window][WINDOWWIDTH],
														WindowList[window][WINDOWHEIGHT]);
			/*
			**	The Win32 build can return cached shapes wrapped in a small header.
			**	The software blitter needs the raw pixel payload, not that wrapper.
			*/
			uint8_t * buffer = (uint8_t *)Get_Shape_Header_Data((void*)shape_pointer);

			UseOldShapeDraw = false;
			/*
			**	Rotation and scale handler.
			*/
			if (rotation != DIR_N || scale != 0x0100) {

				/*
				** Get the raw shape data without the new header and flag to use the old shape drawing
				*/
				UseOldShapeDraw = true;
				buffer = (uint8_t *) Get_Shape_Header_Data((void*)shape_pointer);

				if (Debug_Rotate) {

					GraphicBufferClass src(width, height, buffer);
					width *= 2;
					height *= 2;
					memset(_xbuffer, '\0', SHAPE_BUFFER_SIZE);
					GraphicBufferClass dst(width, height, _xbuffer);
					Rotate_Bitmap(&src, &dst, rotation);
					buffer = _xbuffer;

				} else {

					BitmapClass bm(width, height, buffer);
					width *= 2;
					height *= 2;
					memset(_xbuffer, '\0', SHAPE_BUFFER_SIZE);
					GraphicBufferClass gb(width, height, _xbuffer);
					TPoint2D pt(width/2, height/2);

					gb.Scale_Rotate(bm, pt, scale, (256-(rotation-64)));
					buffer = _xbuffer;
				}
			}

			/*
			**	Special shadow drawing code (used for aircraft and bullets).
			*/
			if ((flags & (SHAPE_FADING|SHAPE_PREDATOR)) == (SHAPE_FADING|SHAPE_PREDATOR)) {
				flags = flags & ~(SHAPE_FADING|SHAPE_PREDATOR);
				flags = flags | SHAPE_GHOST;
				ghostdata = DisplayClass::SpecialGhost;
			}

			predoffset = Frame;

			if (x > ( WindowList[window][WINDOWWIDTH] << 2)) {
				predoffset = -predoffset;
			}

			if (draw_window.Lock()) {
				if ((flags & (SHAPE_GHOST|SHAPE_FADING)) == (SHAPE_GHOST|SHAPE_FADING)) {
					Buffer_Frame_To_Page(x, y, width, height, buffer, draw_window, flags | SHAPE_TRANS, ghostdata, fadingdata, 1, predoffset);
				} else {
					if (flags & SHAPE_FADING) {
						Buffer_Frame_To_Page(x, y, width, height, buffer, draw_window, flags | SHAPE_TRANS, fadingdata, 1, predoffset);
					} else {
						if (flags & SHAPE_PREDATOR) {
							Buffer_Frame_To_Page(x, y, width, height, buffer, draw_window, flags | SHAPE_TRANS, predoffset);
						} else {
							Buffer_Frame_To_Page(x, y, width, height, buffer, draw_window, flags | SHAPE_TRANS, ghostdata, predoffset);
						}
					}
				}
				draw_window.Unlock();
			}
		}
	}
}

/***********************************************************************************************
 * Shape_Dimensions -- Determine the minimum rectangle for the shape.                          *
 *                                                                                             *
 *    This routine will calculate (using brute forced) the minimum rectangle that will         *
 *    enclose the pixels of the shape. This rectangle will be relative to the upper left       *
 *    corner of the maximum shape size. By using this minimum rectangle, it is possible to     *
 *    greatly optimize the map 'dirty rectangle' logic.                                        *
 *                                                                                             *
 * INPUT:   shapedata   -- Pointer to the shape data block.                                    *
 *                                                                                             *
 *          shapenum    -- The shape number to examine. Each shape would have a different      *
 *                         dimension rectangle.                                                *
 *                                                                                             *
 * OUTPUT:  Returns with the rectangle that encloses the shape.                                *
 *                                                                                             *
 * WARNINGS:   This routine uses brute force and is slow. It is presumed that the results      *
 *             will be cached for subsiquent reuse.                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/22/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
Rect const Shape_Dimensions(void const * shapedata, int shapenum)
{
	Rect rect;

	if (shapedata == NULL || shapenum < 0 || shapenum > Get_Build_Frame_Count(shapedata)) {
		return(rect);
	}

	char * shape;
	void * sh = Build_Frame(shapedata, shapenum, _ShapeBuffer);
	if (sh == NULL) return(rect);
//	shape = (char *)sh;
	shape = (char *)Get_Shape_Header_Data(sh);

	int width = Get_Build_Frame_Width(shapedata);
	int height = Get_Build_Frame_Height(shapedata);

	rect.X = 0;
	rect.Y = 0;
	int xlimit = width-1;
	int ylimit = height-1;

	/*
	**	Find top edge of the shape.
	*/
	for (int y = 0; y <= ylimit; y++) {
		for (int x = 0; x <= xlimit; x++) {
			if (shape[y*width + x] != 0) {
				rect.Y = y;
				rect.X = x;
				y = ylimit+1;
				break;
			}
		}
	}

	/*
	**	Find bottom edge of the shape.
	*/
	for (int y = ylimit; y >= rect.Y; y--) {
		for (int x = xlimit; x >= 0; x--) {
			if (shape[y*width + x] != 0) {
				rect.Height = (y-rect.Y)+1;
				xlimit = x;
				y = rect.Y-1;
				break;
			}
		}
	}

	/*
	**	Find left edge of the shape.
	*/
	for (int x = 0; x < rect.X; x++) {
		for (int y = rect.Y; y < rect.Y+rect.Height; y++) {
			if (shape[y*width + x] != 0) {
				rect.X = x;
				x = rect.X;
				break;
			}
		}
	}

	/*
	**	Find the right edge of the shape.
	*/
	for (int x = width-1; x >= xlimit; x--) {
		for (int y = rect.Y; y < rect.Y+rect.Height; y++) {
			if (shape[y*width + x] != 0) {
				rect.Width = (x-rect.X)+1;
				x = xlimit-1;
				break;
			}
		}
	}

	/*
	**	Normalize the rectangle around the center of the shape.
	*/
	rect.X -= width / 2;
	rect.Y -= height / 2;

	/*
	**	Return with the minimum rectangle that encloses the shape.
	*/
	return(rect);
}

/***********************************************************************************************
 * Fetch_Techno_Type -- Convert type and ID into TechnoTypeClass pointer.                      *
 *                                                                                             *
 *    This routine will convert the supplied RTTI type number and the ID value into a valid    *
 *    TechnoTypeClass pointer. If there is an error in conversion, then NULL is returned.      *
 *                                                                                             *
 * INPUT:   type  -- RTTI type of the techno class object.                                     *
 *                                                                                             *
 *          id    -- Integer representation of the techno sub type number.                     *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the techno type class object specified or NULL if the    *
 *          conversion could not occur.                                                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
TechnoTypeClass const * Fetch_Techno_Type(RTTIType type, int id)
{
	switch (type) {
		case RTTI_UNITTYPE:
		case RTTI_UNIT:
			return(&UnitTypeClass::As_Reference(UnitType(id)));

		case RTTI_VESSELTYPE:
		case RTTI_VESSEL:
			return(&VesselTypeClass::As_Reference(VesselType(id)));

		case RTTI_BUILDINGTYPE:
		case RTTI_BUILDING:
			return(&BuildingTypeClass::As_Reference(StructType(id)));

		case RTTI_INFANTRYTYPE:
		case RTTI_INFANTRY:
			return(&InfantryTypeClass::As_Reference(InfantryType(id)));

		case RTTI_AIRCRAFTTYPE:
		case RTTI_AIRCRAFT:
			return(&AircraftTypeClass::As_Reference(AircraftType(id)));

		default:
			break;
	}
	return(NULL);
}

/***********************************************************************************************
 * VQ_Call_Back -- Maintenance callback used for VQ movies.                                    *
 *                                                                                             *
 *    This routine is called every frame of the VQ movie as it is being played. If this        *
 *    routine returns non-zero, then the movie will stop.                                      *
 *                                                                                             *
 * INPUT:   buffer   -- Pointer to the image buffer for the current frame.                     *
 *                                                                                             *
 *          frame    -- The frame number about to be displayed.                                *
 *                                                                                             *
 * OUTPUT:  Should the movie be stopped?                                                       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void VQA_PauseAudio(void);
void Check_VQ_Palette_Set(void);

extern GraphicBufferClass VQ640;
extern bool IsVQ640;

int32_t VQ_Call_Back(uint8_t *buffer, int32_t frame)
{
	static int frame_trace_count = 0;
	int key = 0;
	if (Keyboard->Check()) {
		key = Keyboard->Get();
		Keyboard->Clear();
	}
	Check_VQ_Palette_Set();
	if (std::getenv("RA_TRACE_STARTUP") != NULL && buffer != NULL && frame_trace_count < 8) {
		uint32_t width = IsVQ640 ? 640u : 320u;
		uint32_t height = IsVQ640 ? 400u : 200u;
		uint32_t total = width * height;
		uint32_t nonzero = 0;
		for (uint32_t index = 0; index < total; ++index) {
			if (buffer[index] != 0) {
				++nonzero;
			}
		}
		Conquer_Trace("Play_Movie callback frame=%ld size=%lux%lu pix_nonzero=%lu/%lu first=%u,%u,%u,%u",
			frame,
			width,
			height,
			nonzero,
			total,
			(unsigned)buffer[0],
			(unsigned)buffer[1],
			(unsigned)buffer[2],
			(unsigned)buffer[3]);
		++frame_trace_count;
	}
#ifdef MOVIE640
	if(IsVQ640) {
		VQ640.Blit(SeenBuff);
	} else {
		Interpolate_2X_Scale(&SysMemPage, &SeenBuff, NULL);
	}
#else
	Interpolate_2X_Scale(&SysMemPage, &SeenBuff, NULL);
#endif
	WWDraw_Flush_Present();
	if (std::getenv("RA_TRACE_STARTUP") != NULL && frame < 8 && SeenBuff.Lock()) {
		uint8_t* visible = static_cast<uint8_t*>(SeenBuff.Get_Graphic_Buffer()->Get_Buffer());
		uint32_t nonzero = 0;
		uint32_t width = (uint32_t)SeenBuff.Get_Width();
		uint32_t height = (uint32_t)SeenBuff.Get_Height();
		int pitch = SeenBuff.Get_Pitch();
		int32_t offset = SeenBuff.Get_Offset();
		if (visible != NULL && pitch > 0 && offset >= 0) {
			uint8_t* row = visible + offset;
			for (uint32_t y = 0; y < height; ++y) {
				for (uint32_t x = 0; x < width; ++x) {
					if (row[x] != 0) {
						++nonzero;
					}
				}
				row += pitch;
			}
			row = visible + offset;
			Conquer_Trace("Play_Movie dest frame=%ld size=%lux%lu pix_nonzero=%lu/%lu first=%u,%u,%u,%u pitch=%d offset=%ld",
				frame,
				width,
				height,
				nonzero,
				width * height,
				(unsigned)row[0],
				(unsigned)row[1],
				(unsigned)row[2],
				(unsigned)row[3],
				pitch,
				offset);
		}
		SeenBuff.Unlock();
	}
	//Call_Back();

	if ((BreakoutAllowed || Debug_Flag) && key == KN_ESC) {
		Keyboard->Clear();
		Brokeout = true;
		return(true);
	}

	if (!GameInFocus) {
		VQA_PauseAudio();
		while (!GameInFocus) {
			Check_For_Focus_Loss();
		}
	}
	return(false);
}

/***********************************************************************************************
 * Handle_Team -- Processes team selection command.                                            *
 *                                                                                             *
 *    This routine will handle creation and selection of pseudo teams that the player can      *
 *    create or control. A team in this sense is an arbitrary grouping of units such that      *
 *    rapid selection control is allowed.                                                      *
 *                                                                                             *
 * INPUT:   team  -- The logical team number to process.                                       *
 *                                                                                             *
 *          action-- The action to perform on this team:                                       *
 *                   0 - Toggle the select state for all members of this team.                 *
 *                   1 - Select the members of this team.                                      *
 *                   2 - Make all selected objects members of this team.                       *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/27/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void Handle_Team(int team, int action)
{
	int index;

	//
	// Recording support
	//
	if (Session.Record) {
		TeamNumber = (char)team;
		TeamEvent = (char)action + 1;
	}

	AllowVoice = true;
	switch (action) {

		/*
		**	Toggle the team selection. If the team is selected, then merely unselect it. If the
		**	team is not selected, then unselect all others before selecting this team.
		*/
		case 3:
		case 0:

			/*
			**	If a non team member is currently selected, then deselect all objects
			**	before selecting this team.
			*/
			if (CurrentObject.Count()) {
				if (CurrentObject[0]->Is_Foot() && ((FootClass *)CurrentObject[0])->Group != team) {
					Unselect_All();
				}
			}
			for (index = 0; index < Vessels.Count(); index++) {
				VesselClass * obj = Vessels.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Units.Count(); index++) {
				UnitClass * obj = Units.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Infantry.Count(); index++) {
				InfantryClass * obj = Infantry.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Aircraft.Count(); index++) {
				AircraftClass * obj = Aircraft.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}

			/*
			**	Center the map around the team if the ALT key was pressed too.
			*/
			if (action == 3) {
				Map.Center_Map();
				Map.Flag_To_Redraw(true);
			}
			break;

		/*
		**	Additive selection of team.
		*/
		case 1:
			for (index = 0; index < Units.Count(); index++) {
				UnitClass * obj = Units.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Vessels.Count(); index++) {
				VesselClass * obj = Vessels.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Infantry.Count(); index++) {
				InfantryClass * obj = Infantry.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			for (index = 0; index < Aircraft.Count(); index++) {
				AircraftClass * obj = Aircraft.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->Group == team && obj->House->IsPlayerControl) {
					if (!obj->IsSelected) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}
			break;

		/*
		**	Create the team.
		*/
		case 2: {
			int32_t minx = 0x7FFFFFFFL, miny = 0x7FFFFFFFL;
			int32_t maxx = 0, maxy = 0;
			TeamSpeed[team] = SPEED_WHEEL;
			TeamMaxSpeed[team] = MPH_LIGHT_SPEED;
			for (index = 0; index < Units.Count(); index++) {
				UnitClass * obj = Units.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl) {
				 	if (obj->Group == team) obj->Group = 0xFF;
					if (obj->IsSelected) {
						obj->Group = team;
						obj->Mark(MARK_CHANGE);
						int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
						int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
						if (xc < minx) minx = xc;
						if (xc > maxx) maxx = xc;
						if (yc < miny) miny = yc;
						if (yc > maxy) maxy = yc;
						if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
							TeamMaxSpeed[team] = obj->Class->MaxSpeed;
							TeamSpeed[team] = obj->Class->Speed;
						}
					}
				}
			}

			for (index = 0; index < Vessels.Count(); index++) {
				VesselClass * obj = Vessels.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl) {
				 	if (obj->Group == team) obj->Group = -1;
					if (obj->IsSelected) {
						obj->Group = team;
						obj->Mark(MARK_CHANGE);
						int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
						int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
						if (xc < minx) minx = xc;
						if (xc > maxx) maxx = xc;
						if (yc < miny) miny = yc;
						if (yc > maxy) maxy = yc;
						if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
							TeamMaxSpeed[team] = obj->Class->MaxSpeed;
							TeamSpeed[team] = obj->Class->Speed;
						}
					}
				}
			}

			for (index = 0; index < Infantry.Count(); index++) {
				InfantryClass * obj = Infantry.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl) {
				 	if (obj->Group == team) obj->Group = 0xFF;
					if (obj->IsSelected) {
						obj->Group = team;
						obj->Mark(MARK_CHANGE);
						int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
						int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));
						if (xc < minx) minx = xc;
						if (xc > maxx) maxx = xc;
						if (yc < miny) miny = yc;
						if (yc > maxy) maxy = yc;
						if (obj->Class->MaxSpeed < TeamMaxSpeed[team]) {
							TeamMaxSpeed[team] = obj->Class->MaxSpeed;
						}
					}
				}
			}
			for (index = 0; index < Aircraft.Count(); index++) {
				AircraftClass * obj = Aircraft.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl) {
				 	if (obj->Group == team) obj->Group = 0xFF;
					if (obj->IsSelected) {
						obj->Group = team;
						obj->Mark(MARK_CHANGE);
					}
				}
			}

			for (index = 0; index < Units.Count(); index++) {
				UnitClass * obj = Units.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl &&
				 	(obj->Group == team) && (obj->IsSelected) ) {

					/*
					** When a team is first created, they're created without a
					** formation offset, so they will not be created in
					** formation.  Later, if they're assigned a formation, the
					** XFormOffset & YFormOffset numbers will change to valid
					** offsets, and they'll be formationed.
					*/
#if(1)
					obj->XFormOffset = obj->YFormOffset = (int)0x80000000;
#else
#if(1)
// Old always-north formation stuff
						int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
						int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));

						obj->XFormOffset = xc - centerx;
						obj->YFormOffset = yc - centery;
#else
// New method: save direction and distance rather than x & y offset
						obj->XFormOffset = ::Direction(As_Coord(center), obj->Center_Coord());
						obj->YFormOffset = ::Distance (As_Coord(center), obj->Center_Coord());
#endif
#endif
				}
			}

			for (index = 0; index < Infantry.Count(); index++) {
				InfantryClass * obj = Infantry.Ptr(index);
				if (obj && !obj->IsInLimbo && obj->House->IsPlayerControl) {
				 	if (obj->Group == team) obj->Group = 0xFF;
					if (obj->IsSelected) obj->Group = team;
					if (obj->Group == team  && obj->IsSelected) {
#if(1)
						obj->XFormOffset = obj->YFormOffset = (int)0x80000000;
#else
#if(1)
// Old always-north formation stuff
						int32_t xc = Cell_X(Coord_Cell(obj->Center_Coord()));
						int32_t yc = Cell_Y(Coord_Cell(obj->Center_Coord()));

						obj->XFormOffset = xc - centerx;
						obj->YFormOffset = yc - centery;
#else
// New method: save direction and distance rather than x & y offset
						obj->XFormOffset = ::Direction(As_Coord(center), obj->Center_Coord());
						obj->YFormOffset = ::Distance (As_Coord(center), obj->Center_Coord());
#endif
#endif
					}
				}
			}
			break;
		}

		default:
			break;
	}
	AllowVoice = true;
}

/***********************************************************************************************
 * Handle_View -- Either records or restores the tactical view.                                *
 *                                                                                             *
 *    This routine is used to record or restore the current map tactical view.                 *
 *                                                                                             *
 * INPUT:   view  -- The view number to work with.                                             *
 *                                                                                             *
 *          action-- The action to perform with this view number.                              *
 *                   0  =  Restore the view to this previously remembered location.            *
 *                   1  =  Record the current view location.                                   *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/04/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void Handle_View(int view, int action)
{
	if ((unsigned)view < ARRAY_SIZE(Scen.Views)) {
		if (action == 0) {

			Map.Set_Tactical_Position(Coord_Whole(Cell_Coord(Scen.Views[view] - (MAP_CELL_W * 4 * RESFACTOR) - (5*RESFACTOR))));

			/*
			** Win95 scrolling logic cant handle just jumps in screen position so redraw the lot.
			*/
			Map.Flag_To_Redraw (true);
		} else {
			Scen.Views[view] = Coord_Cell(Map.TacticalCoord) + (MAP_CELL_W*4*RESFACTOR) + (5*RESFACTOR);
		}
	}
}

/***************************************************************************
 * DISK_SPACE_AVAILABLE -- returns bytes of free disk space                *
 *                                                                         *
 * INPUT:		none                                                        *
 *                                                                         *
 * OUTPUT:     returns amount of free disk space                           *
 *                                                                         *
 * HISTORY:                                                                *
 *   08/11/1995 PWG : Created.                                             *
 *=========================================================================*/
uint64_t Disk_Space_Available(void)
{
	SDL_Storage* storage = WWFS_OpenFileStorage(".");
	if (storage == NULL) {
		return 0;
	}

	uint64_t remaining = static_cast<uint64_t>(SDL_GetStorageSpaceRemaining(storage));
	SDL_CloseStorage(storage);
	return remaining;
}

/***********************************************************************************************
 * Do_Record_Playback -- handles saving/loading map pos & current object                       *
 *                                                                                             *
 * INPUT:                                                                                      *
 *		none.																												  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *		none.																												  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *		none.																												  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/15/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
static void Do_Record_Playback(void)
{
	int count;
	TARGET tgt;
	int i;
	COORDINATE coord;
	ObjectClass * obj;
	uint32_t sum;
	uint32_t sum2;
	uint32_t ltgt;

	/*
	**	Record a game
	*/
	if (Session.Record) {

		/*
		**	Save the map's location
		*/
		Session.RecordFile.Write(&Map.DesiredTacticalCoord,
			sizeof (Map.DesiredTacticalCoord));

		/*
		**	Save the current object list count
		*/
		count = CurrentObject.Count();
		Session.RecordFile.Write(&count, sizeof(count));

		/*
		**	Save a CRC of the selected-object list.
		*/
		sum = 0;
		for (i = 0; i < count; i++) {
			ltgt = (uint32_t)(CurrentObject[i]->As_Target());
			sum += ltgt;
		}
		Session.RecordFile.Write (&sum, sizeof(sum));

		/*
		**	Save all selected objects.
		*/
		for (i = 0; i < count; i++) {
			tgt = CurrentObject[i]->As_Target();
			Session.RecordFile.Write (&tgt, sizeof(tgt));
		}

		//
		// Save team-selection and formation events
		//
		Session.RecordFile.Write (&TeamEvent, sizeof(TeamEvent));
		Session.RecordFile.Write (&TeamNumber, sizeof(TeamNumber));
		Session.RecordFile.Write (&FormationEvent, sizeof(FormationEvent));
		Session.RecordFile.Write (TeamMaxSpeed, sizeof(TeamMaxSpeed));
		Session.RecordFile.Write (TeamSpeed, sizeof(TeamSpeed));
		Session.RecordFile.Write (&FormMove, sizeof(FormMove));
		Session.RecordFile.Write (&FormSpeed, sizeof(FormSpeed));
		Session.RecordFile.Write (&FormMaxSpeed, sizeof(FormMaxSpeed));
		TeamEvent = 0;
		TeamNumber = 0;
		FormationEvent = 0;
	}

	/*
	**	Play back a game ("attract" mode)
	*/
	if (Session.Play) {

		/*
		**	Read & set the map's location.
		*/
		if (Session.RecordFile.Read(&coord, sizeof(coord))==sizeof(coord)) {
			if (coord != Map.DesiredTacticalCoord) {
				Map.Set_Tactical_Position(coord);
			}
		}

		if (Session.RecordFile.Read(&count, sizeof(count))==sizeof(count)) {
			/*
			**	Compute a CRC of the current object-selection list.
			*/
			sum = 0;
			for (i = 0; i < CurrentObject.Count(); i++) {
				ltgt = (uint32_t)(CurrentObject[i]->As_Target());
				sum += ltgt;
			}

			/*
			**	Load the CRC of the objects on disk; if it doesn't match, select
			**	all objects as they're loaded.
			*/
			Session.RecordFile.Read (&sum2, sizeof(sum2));
			if (sum2 != sum) {
				Unselect_All();
			}

			AllowVoice = true;

			for (i = 0; i < count; i++) {
				if (Session.RecordFile.Read (&tgt, sizeof(tgt))==sizeof(tgt)) {
					obj = As_Object(tgt);
					if (obj && (sum2 != sum)) {
						obj->Select();
						AllowVoice = false;
					}
				}
			}

			AllowVoice = true;

		}

		//
		// Save team-selection and formation events
		//
		Session.RecordFile.Read (&TeamEvent, sizeof(TeamEvent));
		Session.RecordFile.Read (&TeamNumber, sizeof(TeamNumber));
		Session.RecordFile.Read (&FormationEvent, sizeof(FormationEvent));
		if (TeamEvent) {
			Handle_Team(TeamNumber, TeamEvent - 1);
		}
		if (FormationEvent) {
			Toggle_Formation();
		}

Session.RecordFile.Read (TeamMaxSpeed, sizeof(TeamMaxSpeed));
Session.RecordFile.Read (TeamSpeed, sizeof(TeamSpeed));
Session.RecordFile.Read (&FormMove, sizeof(FormMove));
Session.RecordFile.Read (&FormSpeed, sizeof(FormSpeed));
Session.RecordFile.Read (&FormMaxSpeed, sizeof(FormMaxSpeed));
		/*
		**	The map isn't drawn in playback mode, so draw it here.
		*/
		Map.Render();
	}
}

/***********************************************************************************************
 * Hires_Load -- Allocates memory for, and loads, a resolution dependant file.                 *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Name of file to load                                                              *
 *                                                                                             *
 * OUTPUT:   Ptr to loaded file                                                                *
 *                                                                                             *
 * WARNINGS: Caller is responsible for releasing the memory allocated                          *
 *                                                                                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    5/13/96 3:20PM ST : Created                                                              *
 *=============================================================================================*/
void * Hires_Load(char * name)
{
	char 	filename[30];
	int 	length;
	void * return_ptr;

	sprintf(filename, "H%s", name);
	CCFileClass file (filename);

	if (file.Is_Available()) {

		length = file.Size();
		return_ptr = new char[length];
		file.Read(return_ptr, length);
		return (return_ptr);

	} else {
		return (NULL);
	}
}

/***********************************************************************************************
 * Crate_From_Name -- Given a crate name convert it to a crate type.                           *
 *                                                                                             *
 *    Use this routine to convert an ASCII crate name into a crate type. If no match could     *
 *    be found, then CRATE_MONEY is assumed.                                                   *
 *                                                                                             *
 * INPUT:   name  -- Pointer to the crate name text to convert into a crate type.              *
 *                                                                                             *
 * OUTPUT:  Returns with the crate name converted into a crate type.                           *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/12/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
CrateType Crate_From_Name(char const * name)
{
	if (name != NULL) {
		for (int index = CRATE_FIRST; index < CRATE_COUNT; ++index) {
			CrateType crate = (CrateType)index;
			if (SDL_strcasecmp(name, CrateNames[crate]) == 0) return(crate);
		}
	}
	return(CRATE_MONEY);
}

/***********************************************************************************************
 * Owner_From_Name -- Convert an owner name into a bitfield.                                   *
 *                                                                                             *
 *    This will take an owner specification and convert it into a bitfield that represents     *
 *    it. Sometimes this will be just a single house bit, but other times it could be          *
 *    all the allies or soviet house bits combined.                                            *
 *                                                                                             *
 * INPUT:   text  -- Pointer to the text to convert into a house bitfield.                     *
 *                                                                                             *
 * OUTPUT:  Returns with the houses specified. The value is in the form of a bit field with    *
 *          one bit per house type.                                                            *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/12/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
int Owner_From_Name(char const * text)
{
	int ownable = 0;
	if (SDL_strcasecmp(text, "soviet") == 0) {
		ownable |= HOUSEF_SOVIET;
	} else {
		if (SDL_strcasecmp(text, "allies") == 0 || SDL_strcasecmp(text, "allied") == 0) {
			ownable |= HOUSEF_ALLIES;
		} else {
			HousesType h = HouseTypeClass::From_Name(text);
			if (h != HOUSE_NONE && (h < HOUSE_MULTI1 || h > HOUSE_MULTI8)) {
				ownable |= (1 << h);
			}
		}
	}
	return(ownable);
}

/***********************************************************************************************
 * Shake_The_Screen -- Dispatcher that shakes the screen.                                      *
 *                                                                                             *
 *    This routine will shake the game screen the number of shakes requested.                  *
 *                                                                                             *
 * INPUT:   shakes   -- The number of shakes to shake the screen.                              *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/04/1996 BWG : Created.                                                                 *
 *=============================================================================================*/
void Shake_The_Screen(int shakes)
{
	shakes += shakes;

	Hide_Mouse();
	SeenPage.Blit(HidPage);
	int oldyoff = 0;
	int newyoff = 0;
	while(shakes--) {
		uint32_t x = TickCount;
//		CountDownTimer = 1;
		do {
			newyoff = Sim_Random_Pick(0,2) - 1;
		} while (newyoff == oldyoff);
		switch (newyoff) {
			case -1:
				HidPage.Blit(SeenPage, 0,2, 0,0, 640,398);
				break;
			case 0:
				HidPage.Blit(SeenPage);
				break;
			case 1:
				HidPage.Blit(SeenPage, 0,0, 0,2, 640,398);
				break;
		} while (x == TickCount);
//		} while (CountDownTimer != 0) ;
	}
	HidPage.Blit(SeenPage);
	Show_Mouse();
}

/***********************************************************************************************
 * List_Copy -- Makes a copy of a cell offset list.                                            *
 *                                                                                             *
 *    This routine will make a copy of a cell offset list. It will only copy the significant   *
 *    elements of the list limited by the maximum length specified.                            *
 *                                                                                             *
 * INPUT:   source   -- Pointer to a cell offset list.                                         *
 *                                                                                             *
 *          len      -- The maximum number of cell offset elements to store in to the          *
 *                      destination list pointer.                                              *
 *                                                                                             *
 *          dest     -- Pointer to the destination list to store the copy into.                *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Ensure that the destination list is large enough to hold the list copy.         *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/04/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
void List_Copy(int16_t const * source, int len, int16_t * dest)
{
	if (source == NULL || dest == NULL || len <= 0) {
		return;
	}

	while (len > 1) {
		*dest = *source;
		if (*dest == REFRESH_EOL) break;
		dest++;
		source++;
		len--;
	}
	*dest = REFRESH_EOL;
}


/***********************************************************************************************
 * Game_Config_Section -- Returns the config section identifier for the game.
 * HISTORY:
 *    11/19/98 ajw : Created
 *=============================================================================================*/
const char* Game_Config_Section()
{
#ifdef ENGLISH
	static char szKey[] = "SOFTWARE\\Westwood\\Red Alert Windows 95 Edition";
#else
#ifdef GERMAN
	static char szKey[] = "SOFTWARE\\Westwood\\Alarmstufe Rot Windows 95 Edition";
#else
	static char szKey[] = "SOFTWARE\\Westwood\\Alerte Rouge version Windows 95";
#endif
#endif
	return szKey;
}

/***********************************************************************************************
 * Is_Counterstrike_Installed -- Function to determine the availability of the CS expansion    *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   true if Counterstrike is present                                                  *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    4/1/97 11:39PM ST : Created                                                              *
 *=============================================================================================*/
bool Is_Counterstrike_Installed (void)
{
	return RawFileClass("EXPAND.MIX").Is_Available();
}

#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
/***********************************************************************************************
 *=============================================================================================*/
bool Is_Aftermath_Installed (void)
{
	return RawFileClass("EXPAND2.MIX").Is_Available();
}
#endif

#ifdef FIXIT_VERSION_3
bool Force_Scenario_Available( const char* szName )
{
	return CCFileClass(szName).Is_Available();
}
#endif
