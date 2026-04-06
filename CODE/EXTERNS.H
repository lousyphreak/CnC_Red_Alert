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

/* $Header: /counterstrike/EXTERNS.H 2     3/10/97 6:23p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : EXTERNS.H                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : May 27, 1994                                                 *
 *                                                                                             *
 *                  Last Update : May 27, 1994   [JLB]                                         *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef EXTERNS_H
#define EXTERNS_H
#include <stdint.h>

#include	<SDLAUDIOBACKEND.H>
#include	"PALETTEC.H"
#include	"../WIN32LIB/INCLUDE/AUDIO.H"
#include	"CELL.H"
#include	"KEY.H"
#include	"MOUSE.H"

#ifdef SCENARIO_EDITOR
#include	"MAPEDIT.H"
#endif
#include	"TECHNO.H"
#include	"TYPE.H"
#include	"BUILDING.H"
#include	"UNIT.H"
#include	"CREDITS.H"
#include	"GOPTIONS.H"
#include	"OPTIONS.H"
#include	"INFANTRY.H"

extern char _staging_buffer[32000];
void _PRO(void);

/*
**	Convenient alias for MixFileClass<CDFileClass> object. This allows
**	easier entry into the code and less clutter.
*/
typedef MixFileClass<CCFileClass>	MFCD;

extern bool IsVQ640;
extern uint32_t GameVersion;
extern bool Debug_MotionCapture;
extern bool Debug_Rotate;
extern bool Debug_Quiet;
extern bool Debug_Cheat;
extern bool Debug_Remap;
extern bool Debug_Flag;
extern bool Debug_Lose;
extern bool Debug_Map;
extern bool Debug_Win;
extern bool Debug_Icon;
extern bool Debug_Passable;
extern bool Debug_Unshroud;
extern bool Debug_Threat;
extern bool Debug_Find_Path;
extern bool Debug_Check_Map;
extern bool Debug_Playtest;

extern bool Debug_Heap_Dump;
extern bool Debug_Smart_Print;
extern bool Debug_Trap_Check_Heap;
extern bool Debug_Print_Events;

extern void const *LightningShapes;

extern int NewINIFormat;

#ifdef FIXIT_ANTS
extern bool AntsEnabled;
#endif

#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
extern bool NewUnitsEnabled;
extern bool SecretUnitsEnabled;
extern int MTankDistance;
#ifdef FIXIT_CARRIER	//	checked - ajw 9/28/98
extern int CarrierLaunchDelay;
#endif
#endif

#ifdef FIXIT_NAME_OVERRIDE
extern char const * NameOverride[25];
extern int NameIDOverride[25];
#endif

extern int32_t					GameInFocus;
extern uint8_t 	*InterpolatedPalettes[100];
extern int32_t					PalettesRead;
extern unsigned			PaletteCounter;
extern int 					AllDone;
extern bool 				InMovie;
extern WinTimerClass	*		WindowsTimer;
extern WWMouseClass *		WWMouse;
extern GraphicBufferClass	HiddenPage;
#define	SeenPage		SeenBuff
extern GraphicBufferClass		VisiblePage;
extern GraphicViewPortClass	SeenBuff;
extern GraphicBufferClass		SysMemPage;
extern AudioBackendDevice *				SoundObject;
extern AudioBackendBuffer *		PrimaryBufferPtr;
extern int							ScreenWidth;
extern int							ScreenHeight;

/*
**	Dynamic global variables (these change or are initialized at run time).
*/
extern MissionControlClass		MissionControl[MISSION_COUNT];
extern char const *				TutorialText[225];
extern Buffer *					TheaterBuffer;
extern GetCDClass					CDList;
extern CCINIClass					RuleINI;
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
extern CCINIClass					AftermathINI;
#endif
extern Benchmark *				Benches;
extern int							MapTriggerID;
extern int							LogicTriggerID;
extern PKey							FastKey;
extern PKey							SlowKey;
extern RulesClass					Rule;
extern KeyboardClass *			Keyboard;
extern RandomStraw				CryptRandom;
extern RandomClass				NonCriticalRandomNumber;
extern CarryoverClass *			Carryover;
extern ScenarioClass				Scen;
extern RemapControlType			ColorRemaps[PCOLOR_COUNT];
extern RemapControlType			MetalScheme;
extern RemapControlType			GreyScheme;
extern VersionClass				VerNum;
extern bool							SlowPalette;
extern bool							ScoresPresent;
extern bool							AllowVoice;
extern NewConfigType				NewConfig;
extern VoxType						SpeakQueue;
extern bool							PlayerWins;
extern bool							PlayerLoses;
extern bool							PlayerRestarts;
extern int32_t						Frame;
extern VoxType						SpeechRecord[2];
extern void *						SpeechBuffer[2];
extern int							PreserveVQAScreen;
extern bool							BreakoutAllowed;
extern bool							Brokeout;

extern GameOptionsClass 		Options;

extern LogicClass 				Logic;
#ifdef SCENARIO_EDITOR
extern MapEditClass 				Map;
#else
extern MouseClass 				Map;
#endif
extern ScoreClass 				Score;
extern MonoClass 					MonoArray[DMONO_COUNT];
extern MFCD *						TheaterData;
extern MFCD *						MoviesMix;
extern MFCD *						GeneralMix;
extern MFCD *						ScoreMix;
extern MFCD *						MainMix;
extern MFCD *						ConquerMix;
extern ThemeClass 				Theme;
extern SpecialClass 				Special;

/*
**	Game object allocation and tracking classes.
*/
extern TFixedIHeapClass<AircraftClass>						Aircraft;
extern TFixedIHeapClass<AnimClass>							Anims;
extern TFixedIHeapClass<BuildingClass>						Buildings;
extern TFixedIHeapClass<BulletClass>						Bullets;
extern TFixedIHeapClass<FactoryClass>						Factories;
extern TFixedIHeapClass<HouseClass>							Houses;
extern TFixedIHeapClass<InfantryClass>						Infantry;
extern TFixedIHeapClass<OverlayClass>						Overlays;
extern TFixedIHeapClass<SmudgeClass>						Smudges;
extern TFixedIHeapClass<TeamClass>							Teams;
extern TFixedIHeapClass<TeamTypeClass>						TeamTypes;
extern TFixedIHeapClass<TemplateClass>						Templates;
extern TFixedIHeapClass<TerrainClass>						Terrains;
extern TFixedIHeapClass<TriggerClass>						Triggers;
extern TFixedIHeapClass<UnitClass>							Units;
extern TFixedIHeapClass<VesselClass>						Vessels;
extern TFixedIHeapClass<TriggerTypeClass>					TriggerTypes;

extern TFixedIHeapClass<HouseTypeClass>					HouseTypes;
extern TFixedIHeapClass<BuildingTypeClass>				BuildingTypes;
extern TFixedIHeapClass<AircraftTypeClass>				AircraftTypes;
extern TFixedIHeapClass<InfantryTypeClass>				InfantryTypes;
extern TFixedIHeapClass<BulletTypeClass>					BulletTypes;
extern TFixedIHeapClass<AnimTypeClass>						AnimTypes;
extern TFixedIHeapClass<UnitTypeClass>						UnitTypes;
extern TFixedIHeapClass<VesselTypeClass>					VesselTypes;
extern TFixedIHeapClass<TemplateTypeClass>				TemplateTypes;
extern TFixedIHeapClass<TerrainTypeClass>					TerrainTypes;
extern TFixedIHeapClass<OverlayTypeClass>					OverlayTypes;
extern TFixedIHeapClass<SmudgeTypeClass>					SmudgeTypes;

extern FixedIHeapClass *										HeapPointers[RTTI_COUNT];

extern TFixedIHeapClass<WeaponTypeClass>					Weapons;
extern TFixedIHeapClass<WarheadTypeClass>					Warheads;

extern QueueClass<EventClass, MAX_EVENTS>					OutList;
extern QueueClass<EventClass, (MAX_EVENTS * 64)>		DoList;

#ifdef MIRROR_QUEUE
extern QueueClass<EventClass, (MAX_EVENTS * 64)>		MirrorList;
#endif

extern DynamicVectorClass<ObjectClass *>					CurrentObject;
extern DynamicVectorClass<TriggerClass *>					LogicTriggers;
extern DynamicVectorClass<TriggerClass *>					MapTriggers;
extern DynamicVectorClass<TriggerClass *> 				HouseTriggers[HOUSE_COUNT];

extern BaseClass													Base;

/* These variables are used to keep track of the slowest speed of a team */
extern MPHType TeamMaxSpeed[10];
extern SpeedType TeamSpeed[10];
extern bool FormMove;
extern SpeedType FormSpeed;
extern MPHType FormMaxSpeed;

extern bool IsTanyaDead;
extern bool SaveTanya;

extern bool TimeQuake;

#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
extern bool PendingTimeQuake;
extern TARGET TimeQuakeCenter;
extern fixed QuakeUnitDamage;
extern fixed QuakeBuildingDamage;
extern int QuakeInfantryDamage;
extern int QuakeDelay;
extern fixed ChronoTankDuration;		// chrono override for chrono tanks
#ifdef FIXIT_ENGINEER	//	checked - ajw 9/28/98
extern fixed EngineerDamage;			// Amount of damage an engineer does
extern fixed EngineerCaptureLevel;	// Building damage level before engineer can capture
#endif
#endif

/*
**	Loaded data file pointers.
*/
extern void const *		Metal12FontPtr;
extern void const *		MapFontPtr;
extern void const *		VCRFontPtr;
extern void const *		TypeFontPtr;
extern void const *		Font3Ptr;
extern void const *		Font6Ptr;
extern void const *		EditorFont;
extern void const *		Font8Ptr;
extern void const *		FontLEDPtr;
extern void const *		ScoreFontPtr;
extern void const *		GradFont6Ptr;
extern char const *		SystemStrings;
extern char const *		DebugStrings;

/*
**	Miscellaneous globals.
*/
extern ChronalVortexClass		ChronalVortex;
extern TTimerClass<SystemTimerClass> TickCount;
extern bool							PassedProximity;	// used in display.cpp
extern HousesType					Whom;
extern _VQAConfig					AnimControl;
extern int32_t						SpareTicks;
extern int32_t						PathCount;
extern int32_t						CellCount;
extern int32_t						TargetScan;
extern int32_t						SidebarRedraws;
extern DMonoType					MonoPage;
extern bool							GameActive;
extern bool							SpecialFlag;
extern int							ScenarioInit;
extern HouseClass *				PlayerPtr;
extern PaletteClass 				CCPalette;
extern PaletteClass 				BlackPalette;
extern PaletteClass 				WhitePalette;
extern PaletteClass 				GamePalette;
//extern PaletteClass 				InGamePalette;
#define InGamePalette GamePalette
extern PaletteClass 				OriginalPalette;
extern PaletteClass				ScorePalette;
extern int							BuildLevel;
extern uint32_t				ScenarioCRC;

#ifdef FIXIT_VERSION_3
extern bool							bAftermathMultiplayer;		//	Is multiplayer game being played with Aftermath rules?
#else
extern uint32_t 				PlayingAgainstVersion;		// Negotiated version number
extern bool							Version107InMix;				// Is there a v1.07 in the game
#endif

#ifdef FIXIT_VERSION_3			//	For endgame auto-sonar pulse.
extern bool							bAutoSonarPulse;
#endif

#ifdef SCENARIO_EDITOR
extern CELL 						CurrentCell;
#endif

extern SessionClass				Session;
extern UDPManagerClass 	 		Udp;

#if(TIMING_FIX)
extern int										NewMaxAheadFrame1;
extern int										NewMaxAheadFrame2;
#endif

extern int							Seed;
extern int							CustomSeed;
extern GroundType  				Ground[LAND_COUNT];

/*
**	Constant externs (data is not modified during game play).
*/
extern char const *							Missions[MISSION_COUNT];
extern char const								Keys[];
extern char const * const					VQName[VQ_COUNT];
extern int										CrateData[CRATE_COUNT];
extern char const * const					CrateNames[CRATE_COUNT];
extern int										CrateShares[CRATE_COUNT];
extern AnimType								CrateAnims[CRATE_COUNT];
extern char const * const					SpecialWeaponName[SPC_COUNT];
extern int const								SpecialWeaponHelp[SPC_COUNT];
extern char const * const					SpecialWeaponFile[SPC_COUNT];
extern char const * const					ArmorName[ARMOR_COUNT];
extern char const * const					QuarryName[QUARRY_COUNT];
extern char const * const					FormationName[FORMATION_COUNT];
extern int32_t const								PlayCodes[];
extern int32_t const								CheatCodes[];
//extern char const * const					ProjectileNames[];
extern int32_t const								EditorCodes[];
extern char const * const					SourceName[SOURCE_COUNT];
extern int const								GroundColor[LAND_COUNT];
extern int const								SnowColor[LAND_COUNT];
extern TheaterDataType const 				Theaters[THEATER_COUNT];
extern uint8_t const 				Facing32[256];
extern uint8_t const					Facing16[256];
extern int8_t const					Rotation16[256];
extern uint8_t const 				Facing8[256];
extern uint8_t const					Pixel2Lepton[24];
extern COORDINATE const 					StoppingCoordAbs[5];
extern CELL const 							AdjacentCell[FACING_COUNT];
extern COORDINATE const 					AdjacentCoord[FACING_COUNT];
extern uint8_t const					RemapCiv2[];
extern uint8_t const					RemapCiv4[];
extern uint8_t const					RemapCiv5[];
extern uint8_t const					RemapCiv6[];
extern uint8_t const					RemapCiv7[];
extern uint8_t const					RemapCiv8[];
extern uint8_t const					RemapCiv9[];
extern uint8_t const					RemapCiv10[];
extern uint8_t const					RemapEmber[];

extern GraphicViewPortClass	HidPage;
extern int 						MenuList[][8];
extern CDTimerClass<SystemTimerClass> FrameTimer;
extern CDTimerClass<SystemTimerClass> CountDownTimer;

extern SpecialDialogType	SpecialDialog;

extern int RequiredCD;
extern int CurrentCD;
extern int MouseInstalled;

extern int LogLevel;
extern uint32_t LogLevelTime[ MAX_LOG_LEVEL ];
extern uint32_t LogLastTime;

extern class DynamicVectorClass<EventChoiceClass> test2;
extern class DynamicVectorClass<ActionChoiceClass> test3;

extern bool LogDump_Print;

extern bool IsTheaterShape;

extern void Reset_Theater_Shapes(void);
extern TheaterType LastTheater;
void Coordinate_Remap ( GraphicViewPortClass *inbuffer, int x, int y, int width, int height, uint8_t *remap_table);
void Do_Vortex (int x, int y, int frame);

/************************************************************
** Win32 specific externs
*/
extern int ReadyToQuit;							//Are we about to exit cleanly
extern bool InDebugger;							//Are we being run from a debugger
void Memory_Error_Handler(void);				//Memory error handler function
void WWDebugString (char const *string);

/*************************************************************
** Internet specific externs
*/

extern char PlanetWestwoodIPAddress[];		//IP of server or other player
extern uint16_t PlanetWestwoodPortNumber;		//Port number to send to
extern bool PlanetWestwoodIsHost;				//Flag true if player has control of game options
extern uint32_t PlanetWestwoodGameID;	//Game ID
extern bool	GameStatisticsPacketSent;
extern bool	ConnectionLost;
extern void *PacketLater;
void	Register_Game_Start_Time(void);
void	Register_Game_End_Time(void);
void	Send_Statistics_Packet(void);
bool 	Server_Remote_Connect(void);
bool 	Client_Remote_Connect(void);
extern	int UnitBuildPenalty;

/*
** From SENDFILE.CPP - externs for scenario file transfers
*/
bool Receive_Remote_File ( char *file_name, uint32_t file_length, uint32_t crc, int gametype);
bool Send_Remote_File ( char *file_name, int gametype );
bool Get_Scenario_File_From_Host(char *return_name, int gametype);
bool Find_Local_Scenario (char *description, char *filename, uint32_t length, char *digest, bool official);

#endif
