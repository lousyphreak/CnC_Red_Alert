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

/* $Header: /CounterStrike/INIT.CPP 8     3/14/97 5:15p Joe_b $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : INIT.CPP                                                     *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : January 20, 1992                                             *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Anim_Init -- Initialize the VQ animation control structure.                               *
 *   Bootstrap -- Perform the initial bootstrap procedure.                                     *
 *   Calculate_CRC -- Calculates a one-way hash from a data block.                             *
 *   Init_Authorization -- Verifies that the player is authorized to play the game.            *
 *   Init_Bootstrap_Mixfiles -- Registers and caches any mixfiles needed for bootstrapping.    *
 *   Init_Bulk_Data -- Initialize the time-consuming mixfile caching.                          *
 *   Init_Game_Data_Access -- Validate the fixed GameData asset layout.                        *
 *   Init_Color_Remaps -- Initialize the text remap tables.                                    *
 *   Init_Expansion_Files -- Fetch any override expansion mixfiles.                            *
 *   Init_Fonts -- Initialize all the game font pointers.                                      *
 *   Init_Game -- Main game initialization routine.                                            *
 *   Init_Heaps -- Initialize the game heaps and buffers.                                      *
 *   Init_Keys -- Initialize the cryptographic keys.                                           *
 *   Init_Mouse -- Initialize the mouse system.                                                *
 *   Init_One_Time_Systems -- Initialize internal pointers to the bulk data.                   *
 *   Init_Random -- Initializes the random-number generator                                    *
 *   Init_Secondary_Mixfiles -- Register and cache secondary mixfiles.                         *
 *   Load_Recording_Values -- Loads recording values from recording file                       *
 *   Load_Title_Page -- Load the background art for the title page.                            *
 *   Obfuscate -- Sufficiently transform parameter to thwart casual hackers.                   *
 *   Parse_Command_Line -- Parses the command line parameters.                                 *
 *   Parse_INI_File -- Parses CONQUER.INI for special options                                  *
 *   Play_Intro -- plays the introduction & logo movies                                        *
 *   Save_Recording_Values -- Saves recording values to a recording file                       *
 *   Select_Game -- The game's main menu                                                       *
 *   Load_Prolog_Page -- Loads the special pre-prolog "please wait" page.                      *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include	"LOADDLG.H"
#include "SDLINPUT.H"
#include <SDL3/SDL_time.h>
#include <cctype>
#include	<cstdarg>
#include	<cstdio>
#include	<cstdlib>
#include	<string>
#include	<stdint.h>
#ifdef NETWORK_UDP
#include	"WSPROTO.H"
#include	"WSPUDP.H"
#include	"INTERNET.H"
#else	//NETWORK_UDP
#include	"TCPIP.H"
#endif	//NETWORK_UDP
#if RA_ENABLE_TCP && !defined(NETWORK_UDP)
#include	"INTERNET.H"
#endif

extern int32_t BootstrapFocusSeen;

#include <time.h>

#ifdef DONGLE
#include "cbn_.h"
#endif

namespace
{
bool init_trace_enabled()
{
	static const bool enabled = std::getenv("RA_TRACE_STARTUP") != NULL;
	return enabled;
}

void init_trace(const char* format, ...)
{
	if (!init_trace_enabled()) {
		return;
	}

	va_list arguments;
	va_start(arguments, format);
	std::fputs("[init] ", stderr);
	std::vfprintf(stderr, format, arguments);
	std::fputc('\n', stderr);
	std::fflush(stderr);
	va_end(arguments);
}

const char* const kGameDataBaseMixfiles[] = {
	"MAIN1.MIX",
	"MAIN2.MIX",
	"MAIN3.MIX",
	"MAIN4.MIX"
};

const char* const kExpansionCampaignScenarioNames[] = {
	"SCG20EA",
	"SCG21EA",
	"SCG22EA",
	"SCG23EA",
	"SCG24EA",
	"SCG26EA",
	"SCG27EA",
	"SCG28EA",
	"SCU31EA",
	"SCU32EA",
	"SCU33EA",
	"SCU34EA",
	"SCU35EA",
	"SCU36EA",
	"SCU37EA",
	"SCU38EA",
#ifdef FIXIT_CSII
	"SCG43EA",
	"SCG41EA",
	"SCG40EA",
	"SCG42EA",
	"SCG47EA",
	"SCG45EA",
	"SCG44EA",
	"SCG48EA",
	"SCG46EA",
	"SCU43EA",
	"SCU40EA",
	"SCU42EA",
	"SCU41EA",
	"SCU45EA",
	"SCU44EA",
	"SCU46EA",
	"SCU47EA",
	"SCU48EA",
#endif
	NULL
};

bool g_game_data_base_mixfiles_registered = false;
bool g_game_data_main_mix_registered = false;
std::string g_game_data_main_mix_filename;
MFCD* g_active_main_mix = NULL;
MFCD* g_bootstrap_conquer_mix = NULL;

static void Register_GameData_Main_Mixfile(void);
static void Configure_Main_Mix_View(int requested_main_mix);
static void Configure_Default_Main_Mix_Internal(void);
static void Configure_Scenario_Main_Mix_Internal(char const* scenario_name);
static void Configure_Score_Presentation_Main_Mix_Internal(void);
static void Refresh_Frontend_Main_Mix_View(void);
static void Load_Movie_Mixfiles(void);
void Configure_Expansion_Main_Mix(bool counterstrike);
static void Register_Shadow_Main_Mix_View_Families(void);
static void Register_Expansion_Campaign_Scenario_Mixes(void);

static void Replace_Registered_Mix(MFCD*& mix, char const* filename)
{
	if (mix != NULL) {
		delete mix;
		mix = NULL;
	}

	if (!CCFileClass(filename).Is_Available()) {
		return;
	}

	mix = new MFCD(filename, &FastKey);
	assert(mix != NULL);
	mix->Promote();
}

enum
{
	kMainMixAllied = 1,
	kMainMixSoviet = 2,
	kMainMixCounterstrike = 3,
	kMainMixAftermath = 4
};

static int Base_Main_Mix_For_House(HousesType house)
{
	if (house == HOUSE_USSR || house == HOUSE_UKRAINE || house == HOUSE_BAD) {
		return kMainMixSoviet;
	}

	return kMainMixAllied;
}

static void Normalize_Scenario_Name(char* normalized_name, size_t normalized_name_size, char const* scenario_name)
{
	if (normalized_name_size == 0) {
		return;
	}

	normalized_name[0] = '\0';
	if (scenario_name == NULL || scenario_name[0] == '\0') {
		return;
	}

	std::snprintf(normalized_name, normalized_name_size, "%s", scenario_name);
	char* extension = std::strrchr(normalized_name, '.');
	if (extension != NULL) {
		*extension = '\0';
	}

	for (size_t index = 0; normalized_name[index] != '\0'; ++index) {
		normalized_name[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(normalized_name[index])));
	}
}

static int Main_Mix_View_For_Scenario(char const* scenario_name)
{
	char normalized_name[32];
	Normalize_Scenario_Name(normalized_name, sizeof(normalized_name), scenario_name);
	const size_t normalized_name_length = std::strlen(normalized_name);
	if (normalized_name[0] == '\0') {
		return kMainMixAllied;
	}

	if (normalized_name_length >= 3 &&
		normalized_name[0] == 'S' &&
		normalized_name[1] == 'C' &&
		normalized_name[2] == 'M') {
		if (Is_Mission_Aftermath(normalized_name)) {
			return kMainMixAftermath;
		}
		if (Is_Mission_Counterstrike(normalized_name)) {
			return kMainMixCounterstrike;
		}
		return kMainMixAllied;
	}

	if (normalized_name_length >= 3 &&
		normalized_name[0] == 'S' &&
		normalized_name[1] == 'C' &&
		normalized_name[2] == 'A') {
		return kMainMixCounterstrike;
	}

	if (normalized_name_length >= 7 &&
		normalized_name[0] == 'S' &&
		normalized_name[1] == 'C' &&
		(normalized_name[2] == 'G' || normalized_name[2] == 'U') &&
		std::isdigit(static_cast<unsigned char>(normalized_name[3])) &&
		std::isdigit(static_cast<unsigned char>(normalized_name[4])) &&
		normalized_name[5] == 'E' &&
		normalized_name[6] == 'A') {
		const int scenario_number = ((normalized_name[3] - '0') * 10) + (normalized_name[4] - '0');
		if (scenario_number >= 40) {
			return kMainMixAftermath;
		}
		if (scenario_number >= 20) {
			return kMainMixCounterstrike;
		}
		return (normalized_name[2] == 'U') ? kMainMixSoviet : kMainMixAllied;
	}

	return kMainMixAllied;
}

static bool Is_Local_GameData_File_Available(const char* filename)
{
	return RawFileClass(filename).Is_Available();
}

static void Register_GameData_Base_Mixfiles(void)
{
	if (g_game_data_base_mixfiles_registered) {
		return;
	}

	Register_GameData_Main_Mixfile();
	g_game_data_base_mixfiles_registered = true;
}

static void Register_GameData_Main_Mixfile(void)
{
	if (g_game_data_main_mix_registered || !CCFileClass("MAIN.MIX").Is_Available()) {
		return;
	}

	CCFileClass main_mix_file("MAIN.MIX");
	MFCD* main_mix = new MFCD("MAIN.MIX", &FastKey);
	assert(main_mix != NULL);
	g_active_main_mix = main_mix;
	if (main_mix_file.File_Name() != NULL) {
		char name[_MAX_FNAME];
		char ext[_MAX_EXT];
		char basename[_MAX_FNAME + _MAX_EXT];
		WWFS_SplitPath(main_mix_file.File_Name(), NULL, NULL, name, ext);
		WWFS_MakePath(basename, NULL, NULL, name, ext);
		g_game_data_main_mix_filename = basename;
	} else {
		g_game_data_main_mix_filename.clear();
	}
	g_game_data_main_mix_registered = true;
}

static void Refresh_Frontend_Main_Mix_View(void)
{
	if (g_active_main_mix != NULL) {
		g_active_main_mix->Promote();
	} else {
		Replace_Registered_Mix(g_active_main_mix, "MAIN.MIX");
	}

	Load_Movie_Mixfiles();
	Replace_Registered_Mix(GeneralMix, "GENERAL.MIX");
	Replace_Registered_Mix(ScoreMix, "SCORES.MIX");
	if (ScoreMix != NULL) {
		ThemeClass::Scan();
	}
}

static void Register_Shadow_Main_Mix_View_Families(void)
{
	if (CCFileClass("CONQUER.MIX").Is_Available()) {
		ConquerMix = new MFCD("CONQUER.MIX", &FastKey);
		if (ConquerMix != NULL) {
			ConquerMix->Promote();
		}
	}
	if (CCFileClass("SOUNDS.MIX").Is_Available()) {
		MFCD* sounds_mix = new MFCD("SOUNDS.MIX", &FastKey);
		if (sounds_mix != NULL) {
			sounds_mix->Promote();
		}
	}
	if (CCFileClass("SPEECH.MIX").Is_Available()) {
		MFCD* speech_mix = new MFCD("SPEECH.MIX", &FastKey);
		if (speech_mix != NULL) {
			speech_mix->Promote();
		}
	}
	if (CCFileClass("RUSSIAN.MIX").Is_Available()) {
		MFCD* russian_mix = new MFCD("RUSSIAN.MIX", &FastKey);
		if (russian_mix != NULL) {
			russian_mix->Promote();
		}
	}
	if (CCFileClass("ALLIES.MIX").Is_Available()) {
		MFCD* allies_mix = new MFCD("ALLIES.MIX", &FastKey);
		if (allies_mix != NULL) {
			allies_mix->Promote();
		}
	}
}

static void Configure_Main_Mix_View(int requested_main_mix)
{
	if (requested_main_mix < kMainMixAllied || requested_main_mix > kMainMixAftermath) {
		requested_main_mix = kMainMixAllied;
	}

	if (WWFS_GetMainMixOverride() != requested_main_mix) {
		WWFS_SetMainMixOverride(requested_main_mix);
		Replace_Registered_Mix(g_active_main_mix, "MAIN.MIX");
		Register_Shadow_Main_Mix_View_Families();
		Register_Expansion_Campaign_Scenario_Mixes();
	}

	Refresh_Frontend_Main_Mix_View();
}

static void Configure_Default_Main_Mix_Internal(void)
{
	Configure_Main_Mix_View(kMainMixAllied);
}

static void Configure_Scenario_Main_Mix_Internal(char const* scenario_name)
{
	Configure_Main_Mix_View(Main_Mix_View_For_Scenario(scenario_name));
}

static void Configure_Score_Presentation_Main_Mix_Internal(void)
{
	if (Scen.ScenarioName[0] != '\0') {
		Configure_Scenario_Main_Mix_Internal(Scen.ScenarioName);
		if (g_bootstrap_conquer_mix != NULL) {
			g_bootstrap_conquer_mix->Promote();
		}
		return;
	}

	HousesType house = HOUSE_GOOD;

	if (PlayerPtr != NULL && PlayerPtr->Class.Is_Valid()) {
		house = PlayerPtr->Class->House;
	}

	Configure_Main_Mix_View(Base_Main_Mix_For_House(house));
	if (g_bootstrap_conquer_mix != NULL) {
		g_bootstrap_conquer_mix->Promote();
	}
}

void Configure_Expansion_Main_Mix(bool counterstrike)
{
	Configure_Main_Mix_View(counterstrike ? kMainMixCounterstrike : kMainMixAftermath);
}

static bool Has_Required_Game_Data(void)
{
	const bool redalert_mix = Is_Local_GameData_File_Available("REDALERT.MIX");
	bool base_mixfiles_present = true;

	for (size_t index = 0; index < ARRAY_SIZE(kGameDataBaseMixfiles); ++index) {
		const bool present = Is_Local_GameData_File_Available(kGameDataBaseMixfiles[index]);
		init_trace("Has_Required_Game_Data %s=%d", kGameDataBaseMixfiles[index], present);
		if (!present) {
			base_mixfiles_present = false;
		}
	}

	if (!redalert_mix || !base_mixfiles_present) {
		init_trace("Has_Required_Game_Data REDALERT=%d base_mixfiles_present=%d", redalert_mix, base_mixfiles_present);
		return false;
	}

	Register_GameData_Base_Mixfiles();

	const bool conquer_mix = CCFileClass("CONQUER.MIX").Is_Available();
	const bool general_mix = CCFileClass("GENERAL.MIX").Is_Available();
	init_trace("Has_Required_Game_Data REDALERT=%d CONQUER=%d GENERAL=%d", redalert_mix, conquer_mix, general_mix);
	return conquer_mix && general_mix;
}

#if RA_ENABLE_DVD_MOVIES
MFCD* g_movie_mix_extra = NULL;
#endif

static bool Is_Movie_Mix_Available(const char* filename)
{
	return CCFileClass(filename).Is_Available();
}

static void Register_Expansion_Mixfiles_From_Directory(const char* directory, const char* pattern, bool cache_mix)
{
	if (directory == NULL || directory[0] == '\0') {
		return;
	}

	int mix_count = 0;
	char **mix_files = WWFS_GlobDirectory(directory, pattern, SDL_GLOB_CASEINSENSITIVE, &mix_count);
	if (mix_files == NULL) {
		return;
	}

	for (int index = 0; index < mix_count; ++index) {
		char name[_MAX_FNAME];
		char ext[_MAX_EXT];
		char basename[_MAX_FNAME + _MAX_EXT];
		WWFS_SplitPath(mix_files[index], NULL, NULL, name, ext);
		WWFS_MakePath(basename, NULL, NULL, name, ext);

		if (SDL_strcasecmp(directory, ".") != 0 && RawFileClass(basename).Is_Available()) {
			continue;
		}

		char * ptr = strdup(mix_files[index]);
		new MFCD(ptr, &FastKey);
		if (cache_mix) {
			MFCD::Cache(ptr);
		}
	}

	SDL_free(mix_files);
}

static void Register_Expansion_Scenario_Mix(const char* scenario_name, bool cache_mix)
{
	char scenario_ini[32];
	char scenario_mix[32];

	std::snprintf(scenario_ini, sizeof(scenario_ini), "%s.INI", scenario_name);
	if (CCFileClass(scenario_ini).Is_Available()) {
		return;
	}

	std::snprintf(scenario_mix, sizeof(scenario_mix), "%s.MIX", scenario_name);
	if (!CCFileClass(scenario_mix).Is_Available()) {
		return;
	}

	MFCD* scenario_mix_file = new MFCD(scenario_mix, &FastKey);
	if (scenario_mix_file != NULL) {
		scenario_mix_file->Promote();
	}
	if (cache_mix) {
		bool ok = MFCD::Cache(scenario_mix);
		assert(ok);
		(void)ok;
	}

}

static void Register_Expansion_Campaign_Scenario_Mixes(void)
{
	for (size_t index = 0; kExpansionCampaignScenarioNames[index] != NULL; ++index) {
		Register_Expansion_Scenario_Mix(kExpansionCampaignScenarioNames[index], true);
	}
}

static void Register_Expansion_Scenario_Mixes_From_Packet(const char* packet_name)
{
	CCFileClass file(packet_name);
	if (!file.Is_Available()) {
		return;
	}

	INIClass ini;
	if (!ini.Load(file)) {
		return;
	}

	const int count = ini.Entry_Count("Missions");
	for (int index = 0; index < count; ++index) {
		const char* mission_name = ini.Get_Entry("Missions", index);
		if (mission_name == NULL || mission_name[0] == '\0') {
			continue;
		}

		char scenario_name[_MAX_FNAME];
		std::snprintf(scenario_name, sizeof(scenario_name), "%s", mission_name);
		char* extension = std::strrchr(scenario_name, '.');
		if (extension != NULL) {
			*extension = '\0';
		}
		Register_Expansion_Scenario_Mix(scenario_name, true);
	}
}

static void Load_Movie_Mixfiles(void)
{
	const bool has_movies1 = Is_Movie_Mix_Available("MOVIES1.MIX");
	if (MoviesMix != NULL) {
		delete MoviesMix;
		MoviesMix = NULL;
	}
	MoviesMix = new MFCD(has_movies1 ? "MOVIES1.MIX" : "MOVIES2.MIX", &FastKey);
	assert(MoviesMix != NULL);

#if RA_ENABLE_DVD_MOVIES
	if (g_movie_mix_extra != NULL) {
		delete g_movie_mix_extra;
		g_movie_mix_extra = NULL;
	}
	const bool has_movies2 = Is_Movie_Mix_Available("MOVIES2.MIX");
	if (has_movies1 && has_movies2) {
		g_movie_mix_extra = new MFCD("MOVIES2.MIX", &FastKey);
		assert(g_movie_mix_extra != NULL);
	}
#endif

	MoviesMix->Promote();
}
}

void Configure_Default_Main_Mix(void)
{
	Configure_Default_Main_Mix_Internal();
}

void Configure_Scenario_Main_Mix(char const* scenario_name)
{
	Configure_Scenario_Main_Mix_Internal(scenario_name);
}

void Configure_Score_Presentation_Main_Mix(void)
{
	Configure_Score_Presentation_Main_Mix_Internal();
}

RemapControlType SidebarScheme;

#include "WOLDEBUG.H"

#ifdef CHEAT_KEYS
extern bool bNoMovies;
#endif

/****************************************
**	Function prototypes for this module **
*****************************************/
static void Play_Intro(bool sequenced=false);
static void Init_Color_Remaps(void);
static void Init_Heaps(void);
static void Init_Expansion_Files(void);
static void Init_One_Time_Systems(void);
static void Init_Fonts(void);
static void Init_Game_Data_Access(void);
static void Init_Bootstrap_Mixfiles(void);
static void Init_Secondary_Mixfiles(void);
static void Init_Mouse(void);
static void Bootstrap(void);
//static void Init_Authorization(void);
static void Init_Bulk_Data(void);
static void Init_Keys(void);

extern int UnitBuildPenalty;

extern uint32_t RandNumb;

//extern int SimRandIndex;
void Init_Random(void);

#define ATTRACT_MODE_TIMEOUT	3600		// timeout for attract mode

bool Load_Recording_Values(CCFileClass & file);
bool Save_Recording_Values(CCFileClass & file);

#ifdef WOLAPI_INTEGRATION
extern int WOL_Main();
extern WolapiObject* pWolapi;
#endif

#ifdef FIXIT_VERSION_3
bool Expansion_Dialog( bool bCounterstrike );
#endif

extern bool Is_Mission_Counterstrike (char *file_name);

/***********************************************************************************************
 * Load_Prolog_Page -- Loads the special pre-prolog "please wait" page.                        *
 *                                                                                             *
 *    This loads and displays the prolog page that is displayed before the prolog movie        *
 *    is played. This page is necessary because there is much loading that occurs before       *
 *    the prolog movie is played and looking at a picture is better than looking at a blank    *
 *    screen.                                                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Load_Prolog_Page(void)
{
	Hide_Mouse();
	Load_Title_Screen("PROLOG.PCX", &HidPage, CCPalette);
	HidPage.Blit(SeenPage);
	CCPalette.Set();
	Show_Mouse();
}

/***********************************************************************************************
 * Init_Game -- Main game initialization routine.                                              *
 *                                                                                             *
 *    Perform all one-time game initializations here. This includes all                        *
 *    allocations and table setups. The intro and other one-time startup                       *
 *    tasks are also performed here.                                                           *
 *                                                                                             *
 * INPUT:   argc,argv   -- Command line arguments.                                             *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Only call this ONCE!                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/07/1992 JLB : Created.                                                                 *
 *=============================================================================================*/
#include	"SHA.H"
//#include    <locale.h>
bool Init_Game(int , char * [])
{
	/*
	**	Allocate the benchmark tracking objects only if the machine and
	**	compile flags indicate.
	*/
	#ifdef CHEAT_KEYS
	if (Processor() >= 2) {
		Benches = new Benchmark [BENCH_COUNT];
	}
	#endif

	/*
	**	Initialize the encryption keys.
	*/
	Init_Keys();

	/*
	**	Bootstrap as much as possible before error-prone initializations are
	**	performed. This bootstrap process will enable the error message
	**	handler to function.
	*/
	Bootstrap();
	init_trace("Init_Game bootstrap complete");

	/*
	**	Check for an initialize a working mouse pointer. Display error and bail if
	**	no mouse driver is installed.
	*/
	Init_Mouse();
	init_trace("Init_Game mouse init complete");

	/*
	**	Initialize access to the required game data.
	*/
	Init_Game_Data_Access();
	init_trace("Init_Game media init complete data_ready=%d", Has_Required_Game_Data());

	if (Special.IsFromInstall) {
		Load_Prolog_Page();
	}

	/*
	**	Register and cache any secondary mixfiles.
	*/
	Init_Secondary_Mixfiles();
	init_trace("Init_Game secondary mixfiles ready");

	/*
	**	This is a special hack to initialize the heaps that must be in place before the
	**	rules file is processed. These heaps should properly be allocated as a consequence
	**	of processing the rules.ini file, but that is a bit beyond the capabilities of
	**	the rule parser routine (currently).
	*/
	HouseTypes.Set_Heap(HOUSE_COUNT);
	BuildingTypes.Set_Heap(STRUCT_COUNT);
	AircraftTypes.Set_Heap(AIRCRAFT_COUNT);
	InfantryTypes.Set_Heap(INFANTRY_COUNT);
	BulletTypes.Set_Heap(BULLET_COUNT);
	AnimTypes.Set_Heap(ANIM_COUNT);
	UnitTypes.Set_Heap(UNIT_COUNT);
	VesselTypes.Set_Heap(VESSEL_COUNT);
	TemplateTypes.Set_Heap(TEMPLATE_COUNT);
	TerrainTypes.Set_Heap(TERRAIN_COUNT);
	OverlayTypes.Set_Heap(OVERLAY_COUNT);
	SmudgeTypes.Set_Heap(SMUDGE_COUNT);
	init_trace("Init_Game type heaps allocated");

	HouseTypeClass::Init_Heap();
	BuildingTypeClass::Init_Heap();
	AircraftTypeClass::Init_Heap();
	InfantryTypeClass::Init_Heap();
	BulletTypeClass::Init_Heap();
	AnimTypeClass::Init_Heap();
	UnitTypeClass::Init_Heap();
	VesselTypeClass::Init_Heap();
	TemplateTypeClass::Init_Heap();
	TerrainTypeClass::Init_Heap();
	OverlayTypeClass::Init_Heap();
	SmudgeTypeClass::Init_Heap();
	init_trace("Init_Game type classes initialized");

	/*
	**	Find and process any rules for this game.
	*/
	CCFileClass rules_file("RULES.INI");
	init_trace("Init_Game RULES.INI load start");
	if (RuleINI.Load(rules_file, false)) {
		init_trace("Init_Game RULES.INI load complete");
		init_trace("Init_Game Rule.Process start");
		Rule.Process(RuleINI);
		init_trace("Init_Game Rule.Process complete");
	}
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
	//  Aftermath runtime change 9/29/98
	//	This is safe to do, as only rules for aftermath units are included in this ini.
	if (Is_Aftermath_Installed() == true) {
		CCFileClass aftermath_file("AFTRMATH.INI");
		init_trace("Init_Game AFTRMATH.INI load start");
		if (AftermathINI.Load(aftermath_file, false)) {
			init_trace("Init_Game AFTRMATH.INI load complete");
			init_trace("Init_Game Aftermath rule overlay start");
			Rule.General(AftermathINI);
			Rule.Recharge(AftermathINI);
			Rule.AI(AftermathINI);
			Rule.Powerups(AftermathINI);
			Rule.Land_Types(AftermathINI);
			Rule.Themes(AftermathINI);
			Rule.IQ(AftermathINI);
			Rule.Objects(AftermathINI);
			Rule.Difficulty(AftermathINI);
			init_trace("Init_Game Aftermath rule overlay complete");
		}
	}
#endif
	init_trace("Init_Game rules processed");

	Session.MaxPlayers = Rule.MaxPlayers;

	/*
	**	Initialize the game object heaps as well as other rules-dependant buffer allocations.
	*/
	Init_Heaps();
	init_trace("Init_Game object heaps initialized");

	/*
	**	Initialize the animation system.
	*/
	Anim_Init();
	init_trace("Init_Game animation system initialized");

	/*
	**	Play the startup animation.
	*/
	if (!Special.IsFromInstall) {
		VisiblePage.Clear();
//		Mono_Printf("Playing Intro\n");
		init_trace("Init_Game Play_Intro start");
		Play_Intro();
		init_trace("Init_Game Play_Intro complete");
		memset(CurrentPalette, 0x01, 768);
		WhitePalette.Set();
	} else {
		memset(CurrentPalette, 0x01, 768);
	}

	/*
	**	Initialize the text remap tables.
	*/
	Init_Color_Remaps();

	/*
	**	Get authorization to access the game.
	*/
//	Init_Authorization();
//	Show_Mouse();

	/*
	**	Set the logic page to the seenpage.
	*/
	Set_Logic_Page(SeenBuff);

	/*
	**	If not automatically launching into the intro, then display the title
	**	page while the bulk data is cached.
	*/
	if (!Special.IsFromInstall) {
		init_trace("Init_Game Load_Title_Page start");
		Load_Title_Page(true);
		init_trace("Init_Game Load_Title_Page complete");

		Hide_Mouse();
		Fancy_Text_Print(TXT_STAND_BY, 160*RESFACTOR, 120*RESFACTOR, &ColorRemaps[PCOLOR_DIALOG_BLUE], TBLACK, TPF_CENTER|TPF_TEXT|TPF_DROPSHADOW);
		Show_Mouse();

		CCPalette.Set(FADE_PALETTE_SLOW);
		Call_Back();
		init_trace("Init_Game title page shown");
	}

	/*
	**	Initialize the bulk data. This takes the longest time and must be performed once
	**	before the regular game starts.
	*/
	Init_Bulk_Data();
	init_trace("Init_Game bulk data initialized");

	/*
	**	Initialize the multiplayer score values
	*/
	Session.GamesPlayed = 0;
	Session.NumScores = 0;
	Session.CurGame = 0;
	for (int i = 0; i < MAX_MULTI_NAMES; i++) {
		Session.Score[i].Name[0] = '\0';
		Session.Score[i].Wins = 0;
		for (int j = 0; j < MAX_MULTI_GAMES; j++) {
			Session.Score[i].Kills[j] = -1;	// -1 = this player didn't play this round
		}
	}

	/*
	** Copy the title screen's palette into the GamePalette & OriginalPalette,
	** because the options Load routine uses these palettes to set the brightness, etc.
	*/
	GamePalette = CCPalette;
//	InGamePalette = CCPalette;
	OriginalPalette = CCPalette;

	/*
	**	Read game options, so the GameSpeed is initialized when multiplayer
	** dialogs are invoked.  (GameSpeed must be synchronized between systems.)
	*/
	Options.Load_Settings();

	return(true);
}

#ifdef NETWORK_UDP			//	Steve Tall missed this one - ajw
extern bool Get_Broadcast_Addresses (void);
#endif

/***********************************************************************************************
 * Select_Game -- The game's main menu                                                         *
 *                                                                                             *
 * INPUT:                                                                                      *
 *		fade		if true, will fade the palette in gradually												  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *		none.																												  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *		none.																												  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/05/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
bool Select_Game(bool fade)
{
	//	Enums in Select_Game() must match order of buttons in Main_Menu().
#ifdef FIXIT_VERSION_3
	enum {
		SEL_TIMEOUT = -1,				// main menu timeout--go into attract mode
		SEL_NEW_SCENARIO_CS,				// Expansion scenario to play.
		SEL_NEW_SCENARIO_AM,				// Expansion scenario to play.
		SEL_START_NEW_GAME,			// start a new game
		SEL_LOAD_MISSION,				// load a saved game
		SEL_MULTIPLAYER_GAME,		// play a multiplayer game
		SEL_INTRO,						// couch-potato mode
		SEL_EXIT,						// exit to DOS
		SEL_FAME,						// view the hall o' fame
		SEL_NONE,						// placeholder default value
	};
#else	//	FIXIT_VERSION_3
	enum {
		SEL_TIMEOUT = -1,				// main menu timeout--go into attract mode
		SEL_NEW_SCENARIO,				// Expansion scenario to play.
		SEL_START_NEW_GAME,			// start a new game
#ifndef INTERNET_OFF // Denzil 5/1/98 - Internet play
		SEL_INTERNET,
#endif	// !INTERNET_OFF
		SEL_LOAD_MISSION,				// load a saved game
		SEL_MULTIPLAYER_GAME,		// play a multiplayer game
		SEL_INTRO,						// couch-potato mode
		SEL_EXIT,						// exit to DOS
		SEL_FAME,						// view the hall o' fame
		SEL_NONE,						// placeholder default value
	};
#endif	//	FIXIT_VERSION_3

	bool gameloaded=false;			// Has the game been loaded from the menu?
	int selection;						// the default selection
	bool process = true;				// false = break out of while loop
	bool display = true;

#ifdef DONGLE
	/* These where added by ColinM for the dongle checking */
	int16_t iRet = 0;
	uint16_t iPortNr = 1;  /* automatic port scan enabled */
	uint8_t cSCodeSER[] = "\x41\x42";
	uint32_t ulIdRet = 0;
	uint8_t cBoxName[]= "\x00\x00";
#endif
	
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
	int cdcheck = 0;
	bool cs = Is_Counterstrike_Installed();
	(void)cs;
#endif

	Show_Mouse();

#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
	NewUnitsEnabled = SecretUnitsEnabled = 0;	// Assume new units disabled, unless specifically .INI enabled or multiplayer negotiations enable it.
#endif

	/*
	**	[Re]set any globals that need it, in preparation for a new scenario
	*/
	GameActive = true;
	Stop_Speaking();
	DoList.Init();
	#ifdef MIRROR_QUEUE
	MirrorList.Init();
	#endif
	OutList.Init();
	Frame = 0;
	Scen.MissionTimer = 0;
	Scen.MissionTimer.Stop();
	Scen.CDifficulty = DIFF_NORMAL;
	Scen.Difficulty = DIFF_NORMAL;
	PlayerWins = false;
	PlayerLoses = false;
	Session.ObiWan = false;
	Debug_Unshroud = false;
	Map.Set_Cursor_Shape(0);
	Map.PendingObjectPtr = 0;
	Map.PendingObject = 0;
	Map.PendingHouse = HOUSE_NONE;

	Session.ProcessTicks = 0;
	Session.ProcessFrames = 0;
	Session.DesiredFrameRate = 30;
#if(TIMING_FIX)
	NewMaxAheadFrame1 = 0;
	NewMaxAheadFrame2 = 0;
#endif

/* ColinM added to check for dongle */
#ifdef DONGLE
	iRet = CbN_BoxReady( iPortNr , cBoxName);
	if (cBoxName[0] != 0xc5 && cBoxName[0] != 0xc9) 
	{
	WWMessageBox().Process("Please ensure dongle is attached. Run the dongle batch file too.", TXT_OK);
			Emergency_Exit(EXIT_FAILURE);
	}

	iRet = CbN_ReadSER( iPortNr, cSCodeSER, &ulIdRet);
	if (ulIdRet != 0xa0095)
	{
	WWMessageBox().Process("Please ensure dongle is attached. Run the dongle batch file too.", TXT_OK);
			Emergency_Exit(EXIT_FAILURE);
	}
#endif

	/*
	**	Init multiplayer game scores.  Let Wins accumulate; just init the current
	** Kills for this game.  Kills of -1 means this player didn't play this round.
	*/
	for (int i = 0 ; i < MAX_MULTI_GAMES; i++) {
		Session.Score[i].Kills[Session.CurGame] = -1;
	}

	/*
	**	Set default mouse shape
	*/
	Map.Set_Default_Mouse(MOUSE_NORMAL, false);

	/*
	**	If the last game we played was a multiplayer game, jump right to that
	**	menu by pre-setting 'selection'.
	*/
	if (Session.Type == GAME_NORMAL) {
		selection = SEL_NONE;
	} else {
		selection = SEL_MULTIPLAYER_GAME;
	}

	/*
	**	Main menu processing; only do this if we're not in editor mode.
	*/
	if (!Debug_Map) {
		Configure_Default_Main_Mix_Internal();

		/*
		**	Menu selection processing loop
		*/
		Theme.Queue_Song(THEME_CRUS);

		/*
		** If we're playing back a recording, load all pertinent values & skip
		** the menu loop.  Hide the now-useless mouse pointer.
		*/
		if (Session.Play && Session.RecordFile.Is_Available()) {
			if (Session.RecordFile.Open(READ)) {
				Load_Recording_Values(Session.RecordFile);
				process = false;
				Theme.Fade_Out();
			} else
				Session.Play = false;
		}

		while (process) {

			/*
			**	Redraw the title page if needed
			*/
			if (display) {
				Hide_Mouse();

				/*
				**	Display the title page; fade it in if this is the first time
				**	through the loop, and the 'fade' flag is true
				*/
				Load_Title_Page();
				GamePalette = CCPalette;

				HidPage.Blit(SeenPage);
//				if (fade) {
//					WhitePalette.Set();
//					CCPalette.Set(FADE_PALETTE_SLOW, Call_Back);
//					fade = false;
//				} else {
					CCPalette.Set();
//				}

				Set_Logic_Page(SeenBuff);
				display = false;
				Show_Mouse();
			}

			/*
			**	Display menu and fetch selection from player.
			*/
			if (Special.IsFromInstall) selection = SEL_START_NEW_GAME;

#ifdef WOLAPI_INTEGRATION
			if( pWolapi )
				selection =	SEL_MULTIPLAYER_GAME;		//	We are returning from a game.
#endif

			if (selection == SEL_NONE) {
#ifdef FIXIT_ANTS
				AntsEnabled = false;
#endif
				selection = Main_Menu(ATTRACT_MODE_TIMEOUT);
			}
			Call_Back();

			switch (selection) {

				/*
				**	Pick an expansion scenario.
				*/
#ifdef FIXIT_VERSION_3
				case SEL_NEW_SCENARIO_CS:
				case SEL_NEW_SCENARIO_AM:
#else	//	FIXIT_VERSION_3
				case SEL_NEW_SCENARIO:
#endif	//	FIXIT_VERSION_3
					Scen.CarryOverMoney = 0;
					IsTanyaDead = false;
					SaveTanya = false;

#ifdef FIXIT_VERSION_3
					if( selection == SEL_NEW_SCENARIO_CS )
					{
						Configure_Expansion_Main_Mix(true);
						if(!Expansion_Dialog( true )){
						   selection = SEL_NONE;
						   break;
						}
					}
					else
					{
						Configure_Expansion_Main_Mix(false);
						if(!Expansion_Dialog( false )){
						   selection = SEL_NONE;
						   break;
						}
					}
#else	//	FIXIT_VERSION_3
	#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
					if (cs) {
						cdcheck = 2;
					}
					if (Is_Aftermath_Installed()) {
						if (!cdcheck) {
							cdcheck = 3;
						} else {
							cdcheck = 4;	// special case: means check for 3 or 4
						}
					}
	#else
	#endif
					if(!Expansion_Dialog()){
					   selection = SEL_NONE;
					   break;
					}
#endif	//	FIXIT_VERSION_3

#ifdef FIXIT_DIFFICULTY
	#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
					switch (Fetch_Difficulty(cdcheck >= 3)) {
	#else
					switch (Fetch_Difficulty()) {
	#endif
		 				case 0:
							Scen.CDifficulty = DIFF_HARD;
							Scen.Difficulty = DIFF_EASY;
							break;

						case 1:
							Scen.CDifficulty = DIFF_HARD;
							Scen.Difficulty = DIFF_NORMAL;
							break;

						case 2:
							Scen.CDifficulty = DIFF_NORMAL;
							Scen.Difficulty = DIFF_NORMAL;
							break;

						case 3:
							Scen.CDifficulty = DIFF_EASY;
							Scen.Difficulty = DIFF_NORMAL;
							break;

						case 4:
							Scen.CDifficulty = DIFF_EASY;
							Scen.Difficulty = DIFF_HARD;
							break;
					}
#endif
					Theme.Fade_Out();
					Theme.Queue_Song(THEME_FIRST);
					Session.Type = GAME_NORMAL;
					process = false;
					break;

				/*
				**	SEL_START_NEW_GAME: Play the game
				*/
				case SEL_START_NEW_GAME:
					if (Special.IsFromInstall) {
						Scen.CDifficulty = DIFF_NORMAL;
						Scen.Difficulty = DIFF_NORMAL;
					} else {
						switch (Fetch_Difficulty()) {
							case 0:
								Scen.CDifficulty = DIFF_HARD;
								Scen.Difficulty = DIFF_EASY;
								break;

							case 1:
								Scen.CDifficulty = DIFF_HARD;
								Scen.Difficulty = DIFF_NORMAL;
								break;

							case 2:
								Scen.CDifficulty = DIFF_NORMAL;
								Scen.Difficulty = DIFF_NORMAL;
								break;

							case 3:
								Scen.CDifficulty = DIFF_EASY;
								Scen.Difficulty = DIFF_NORMAL;
								break;

							case 4:
								Scen.CDifficulty = DIFF_EASY;
								Scen.Difficulty = DIFF_HARD;
								break;
						}
					}
					Scen.CarryOverMoney = 0;
					BuildLevel = 10;
					IsTanyaDead = false;
					SaveTanya = false;
					Whom = HOUSE_GOOD;

					if (!Special.IsFromInstall) {
#ifdef FIXIT_ANTS
						if (AntsEnabled)  {
							Scen.Set_Scenario_Name("SCA01EA.INI");
						} else {
#endif
#ifdef FIXIT_VERSION_3
							switch (WWMessageBox().Process(TXT_CHOOSE, TXT_ALLIES, TXT_CANCEL, TXT_SOVIET)) {
								case 2:
									Scen.Set_Scenario_Name("SCU01EA.INI");
									break;
								default:
									selection = SEL_NONE;
									continue;
#else
							switch (WWMessageBox().Process(TXT_CHOOSE, TXT_ALLIES, TXT_SOVIET)) {
								case 1:
									Scen.Set_Scenario_Name("SCU01EA.INI");
									break;
								default:
#endif
								case 0:
									Scen.Set_Scenario_Name("SCG01EA.INI");
									break;

							}
#ifdef FIXIT_ANTS
						}
#endif
						Theme.Fade_Out();
						Load_Title_Page();
					} else {
						Theme.Fade_Out();
						const bool allied_campaign = Choose_Side();
						Hide_Mouse();
						Scen.Set_Scenario_Name(allied_campaign ? "SCG01EA.INI" : "SCU01EA.INI");
					}

					Session.Type = GAME_NORMAL;
					process = false;
					break;

				/*
				**	Load a saved game.
				*/
				case SEL_LOAD_MISSION:
					if (LoadOptionsClass(LoadOptionsClass::LOAD).Process()) {
						Theme.Queue_Song(THEME_FIRST);
						process = false;
						gameloaded = true;
					} else {
						display = true;
						selection = SEL_NONE;
					}
					break;

				/*
				**	SEL_MULTIPLAYER_GAME: set 'Session.Type' to NULL-modem, modem, or
				**	network play.
				*/
				case SEL_MULTIPLAYER_GAME:
#ifdef WOLAPI_INTEGRATION
					if( !pWolapi )
					{
#endif
#if RA_ENABLE_TCP
					if (Internet_Has_Command_Line_Connect()) {
						Session.Type = GAME_INTERNET;
					} else
#endif
					switch (Session.Type) {

						/*
						**	If 'Session.Type' isn't already set up for a multiplayer game,
						**	we must prompt the user for which type of multiplayer game
						**	they want.
						*/
						case GAME_NORMAL:
							Session.Type = Select_MPlayer_Game();
							if (Session.Type == GAME_NORMAL) {		// 'Cancel'
								display = true;
								selection = SEL_NONE;
							}
							break;

						case GAME_SKIRMISH:
							if ( !Com_Scenario_Dialog(true) ) {
								Session.Type = Select_MPlayer_Game();
								if (Session.Type == GAME_NORMAL) {		// user hit Cancel
									display = true;
									selection = SEL_NONE;
								}
							}
							else
							{
								//	Ever hits? Session.Type set to GAME_SKIRMISH without user selecting in Select_MPlayer_Game()?
#ifdef FIXIT_VERSION_3
								//	If mission is Counterstrike, CS CD will be required. But aftermath units require AM CD.
								bAftermathMultiplayer = Is_Aftermath_Installed() && !Is_Mission_Counterstrike( Scen.ScenarioName );
								//	ajw I'll bet this was needed before also...
								Session.ScenarioIsOfficial = Session.Scenarios[Session.Options.ScenarioIndex]->Get_Official();
#endif
							}
							break;

						case GAME_INTERNET:
							Session.Type = Select_MPlayer_Game();
							if (Session.Type == GAME_NORMAL) {		// 'Cancel'
								display = true;
								selection = SEL_NONE;
							}
							break;

						default:
							break;

					}
#ifdef WOLAPI_INTEGRATION
					}	//	if( !pWolapi )

					if( pWolapi )
						Session.Type = GAME_INTERNET;
#endif
//debugprint( "Session.Type = %i\n", Session.Type );
					switch (Session.Type) {
						/*
						**	Immediate local/portable multiplayer start paths
						*/
						case GAME_SKIRMISH:
							Theme.Fade_Out();
							process = false;
#ifdef FIXIT_VERSION_3
							Options.ScoreVolume = Options.MultiScoreVolume;
#else
							Options.ScoreVolume = 0;
#endif
							break;

						case GAME_INTERNET:
#ifdef WOLAPI_INTEGRATION		//	implies also NETWORK_UDP
							if( pWolapi )
							{
							if( PacketTransport )
								delete PacketTransport;
							PacketTransport = new UDPInterfaceClass;
							assert( PacketTransport != NULL );
							if( PacketTransport->Init() )
							{
								switch( WOL_Main() )
								{
								case 1:
									//	Start game.
#ifdef FIXIT_VERSION_3
									Options.ScoreVolume = Options.MultiScoreVolume;
#else
									Options.ScoreVolume = 0;
#endif
									process = false;
									Theme.Fade_Out();
									break;
								case 0:
									//	User cancelled.
									Session.Type = GAME_NORMAL;
									display = true;
									selection = SEL_MULTIPLAYER_GAME;	//SEL_NONE;
									delete PacketTransport;
									PacketTransport = NULL;
									break;
								case -1:
									//	Patch was downloaded. Exit app.
									Theme.Fade_Out();
									BlackPalette.Set( FADE_PALETTE_SLOW );
									return false;
								}
							}
							else
							{
								Session.Type = GAME_NORMAL;
								display = true;
								selection = SEL_MULTIPLAYER_GAME;	//SEL_NONE;
								delete PacketTransport;
								PacketTransport = NULL;
							}
							break;
							}
#endif
#if RA_ENABLE_TCP
#ifdef NETWORK_UDP
							if (PacketTransport ) delete PacketTransport;
							PacketTransport = new UDPInterfaceClass;
							assert ( PacketTransport != NULL);
#endif	//NETWORK_UDP
							if (Internet_Remote_Connect()) {
#ifdef FIXIT_VERSION_3
								Options.ScoreVolume = Options.MultiScoreVolume;
#else
								Options.ScoreVolume = 0;
#endif
								process = false;
								Theme.Fade_Out();
							} else {
								Session.Type = GAME_NORMAL;
								display = true;
								selection = SEL_NONE;
#ifdef NETWORK_UDP
								delete PacketTransport;
								PacketTransport = NULL;
#endif	//NETWORK_UDP
							}
							break;
#endif

						/*
						**	Network (UDP): start a new network game.
						*/
						case GAME_UDP:
							WWDebugString ("RA95 - Game type is UDP.\n");
							/*
							** Init network system & remote-connect
							*/
#ifdef NETWORK_UDP
							if (PacketTransport ) delete PacketTransport;
							PacketTransport = new UDPInterfaceClass;
							assert ( PacketTransport != NULL);

#endif	//NETWORK_UDP
							WWDebugString ("RA95 - About to call Init_Network.\n");
							if (Session.Type == GAME_UDP && Init_Network() && Remote_Connect()) {
#ifdef FIXIT_VERSION_3
								Options.ScoreVolume = Options.MultiScoreVolume;
#else
								Options.ScoreVolume = 0;
#endif
								process = false;
								Theme.Fade_Out();
							} else {						// user hit cancel, or init failed
								Session.Type = GAME_NORMAL;
								display = true;
								selection = SEL_NONE;
#ifdef NETWORK_UDP
								delete PacketTransport;
								PacketTransport = NULL;
#endif	//NETWORK_UDP
							}
							break;

						default:
							break;

					}
					break;

				/*
				**	Play a VQ
				*/
				case SEL_INTRO:
					Theme.Fade_Out();
					if (Debug_Flag) {
						Play_Intro(Debug_Flag);
					} else {
						Hide_Mouse();
						VisiblePage.Clear();
						Show_Mouse();
						Play_Movie(VQ_INTRO_MOVIE, THEME_NONE, true);		// no transition picture to briefing
						Keyboard->Clear();
						Play_Movie(VQ_SIZZLE, THEME_NONE, true);
						Play_Movie(VQ_SIZZLE2, THEME_NONE, true);
//						Play_Movie(VQ_INTRO_MOVIE, THEME_NONE, false);		// has transitino picture to briefing
					}
					Theme.Queue_Song(THEME_CRUS);
					display = true;
					fade = true;
					selection = SEL_NONE;
					break;

				/*
				**	Exit to DOS.
				*/
				case SEL_EXIT:
					Theme.Fade_Out();
					BlackPalette.Set(FADE_PALETTE_SLOW);
					return(false);

				/*
				**	Display the hall of fame.
				*/
				case SEL_FAME:
					break;

				case SEL_TIMEOUT:
					if (Session.Attract && Session.RecordFile.Is_Available()) {
						Session.Play = true;
						if (Session.RecordFile.Open(READ)) {
							Load_Recording_Values(Session.RecordFile);
							process = false;
							Theme.Fade_Out();
						} else {
							Session.Play = false;
							selection = SEL_NONE;
						}
					} else {
						selection = SEL_NONE;
					}
					break;

				default:
					break;
			}
		}
	} else {

		/*
		** For Debug_Map (editor) mode to load scenario
		*/
		Scen.Set_Scenario_Name("SCG01EA.INI");
	}

	/*
	**	Don't carry stray keystrokes into game.
	*/
	Keyboard->Clear();

	/*
	** Initialize the random number generator(s)
	*/
	Init_Random();

	/*
	** Save initialization values if we're recording this game.
	*/
	if (Session.Record) {
		if (Session.RecordFile.Open(WRITE)) {
			Save_Recording_Values(Session.RecordFile);
		} else {
			Session.Record = false;
		}
	}

#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
	switch(Session.Type) {
		case GAME_UDP:
#ifdef FIXIT_VERSION_3
			if( !bAftermathMultiplayer ) {
#else
			if (PlayingAgainstVersion < VERSION_AFTERMATH_CS) {
#endif
				NewUnitsEnabled = SecretUnitsEnabled = false;
			} else {
				NewUnitsEnabled = true;
			}
//			debugprint( "Non Internet game: NewUnitsEnabled = %i\n", NewUnitsEnabled );
			break;
		#ifdef WOLAPI_INTEGRATION
		case GAME_INTERNET:
			if( !pWolapi )
			{
//				debugprint( "pWolapi is null on internet game!" );
				Fatal( "pWolapi is null on internet game!" );
			}
			//if( pWolapi->bEnableNewAftermathUnits )
			if( bAftermathMultiplayer )
				NewUnitsEnabled = true;
			else
				NewUnitsEnabled = SecretUnitsEnabled = false;
//			debugprint( "Internet game: NewUnitsEnabled = %i\n", NewUnitsEnabled );
			break;
		#endif
		default:
			break;
	}
#endif
	/*
	**	Load the scenario.  Specify variation 'A' for the editor; for the game,
	**	don't specify a variation, to make 'Set_Scenario_Name()' pick a random one.
	**	Skip this if we've already loaded a save-game.
	*/
	if (!gameloaded && !Session.LoadGame) {
//		if (Debug_Map) {
//			Set_Scenario_Name(Scen.ScenarioName, Scen.Scenario, Scen.ScenPlayer, Scen.ScenDir, SCEN_VAR_A);
//		}  else {
//			Set_Scenario_Name(Scen.ScenarioName, Scen.Scenario, Scen.ScenPlayer, Scen.ScenDir);
//		}

		/*
		** Start_Scenario() changes the palette; so, fade out & clear the screen
		** before calling it.
		*/
		Hide_Mouse();

		if (selection != SEL_START_NEW_GAME) {
			BlackPalette.Set(FADE_PALETTE_MEDIUM, Call_Back);
			HiddenPage.Clear();
			VisiblePage.Clear();
		}
		Show_Mouse();
//Mono_Printf("About to call Start Scenario with %s\n", Scen.ScenarioName);
		if (!Start_Scenario(Scen.ScenarioName)) {
			return(false);
		}
		if (Special.IsFromInstall) Show_Mouse();
		Special.IsFromInstall = false;
	}

	/*
	**	For multiplayer games, initialize the inter-player message system.
	**	Do this after loading the scenario, so the map's upper-left corner is
	**	properly set.
	*/
	Session.Messages.Init(
		Map.TacPixelX, Map.TacPixelY, 	// x,y for messages
		6, 										// max # msgs
		MAX_MESSAGE_LENGTH-14,				// max msg length
		7 * RESFACTOR,							// font height in pixels
		-1, -1, 									// x,y for edit line (appears above msgs)
		0,//BG		1,							// enable edit overflow
		20,          							// min,
		MAX_MESSAGE_LENGTH - 14,			//    max for trimming overflow
		Lepton_To_Pixel(Map.TacLeptonWidth));	// Width in pixels of buffer

	if (Session.Type != GAME_NORMAL && Session.Type != GAME_SKIRMISH &&
		!Session.Play) {
		Session.Create_Connections();
	}

	/*
	** If this isnt an internet game that set the unit build rate to its default value
	*/
	if (Session.Type != GAME_INTERNET){
		UnitBuildPenalty = 100;
	}

	/*
	**	Hide the SeenPage; force the map to render one frame.  The caller can
	**	then fade the palette in.
	**	(If we loaded a game, this step will fade out the title screen.  If we
	**	started a scenario, Start_Scenario() will have played a couple of VQ
	**	movies, which will have cleared the screen to black already.)
	*/
	Call_Back();
	Hide_Mouse();
	BlackPalette.Set(FADE_PALETTE_MEDIUM, Call_Back);
//	Fade_Palette_To(BlackPalette, FADE_PALETTE_MEDIUM, Call_Back);
	HiddenPage.Clear();
	VisiblePage.Clear();
	Show_Mouse();
	Set_Logic_Page(SeenBuff);
	/*
	** Sidebar is always active in hi-res.
	*/
	if (!Debug_Map) {
		Map.SidebarClass::Activate(1);
	}
	Map.Flag_To_Redraw();
	Call_Back();
	Map.Render();

#ifdef WOLAPI_INTEGRATION

						//ajw debugging only
//						debugprint( "Debugging Session...\n" );
//						debugprint( "Session.Players count is %i.\n", Session.Players.Count() );
						for (int i = 0; i < Session.Players.Count(); i++)
						{
							NetNumType net;
							NetNodeType node;
							Session.Players[i]->Address.Get_Address( net, node );
//							debugprint( "Player %i, %s, color %i, ip %i.%i.%i.%i.%i.%i\n", i, Session.Players[i]->Name, 
//								Session.Players[i]->Player.Color, node[0], node[1], node[2], node[3], node[4], node[5] );
						}
//						debugprint( "PlanetWestwoodPortNumber is %i\n", PlanetWestwoodPortNumber );

#endif

	return(true);
}

/***********************************************************************************************
 * Play_Intro -- plays the introduction & logo movies                                          *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *		none.																												  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *		none.																												  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/06/1995 BRR : Created.                                                                 *
 *   05/08/1996 JLB : Modified for Red Alert and direction control.                            *
 *=============================================================================================*/
static void Play_Intro(bool sequenced)
{
	static VQType _counter = VQ_FIRST;

	Keyboard->Clear();
	if (sequenced) {
		if (_counter <= VQ_FIRST) _counter = VQ_COUNT;
		if (_counter == VQ_COUNT) _counter--;
		if (_counter == VQ_REDINTRO) _counter--;
		if (_counter == VQ_TITLE) _counter--;
		Hide_Mouse();
		VisiblePage.Clear();
		Show_Mouse();
		Play_Movie(VQType(_counter--), THEME_NONE);
		
//		Show_Mouse();
	} else {
		Hide_Mouse();
		VisiblePage.Clear();
		Show_Mouse();
		char fullname[_MAX_FNAME+_MAX_EXT];
		WWFS_MakePath(fullname, NULL, NULL, VQName[VQ_REDINTRO], ".VQA");
		const int hires_available = CCFileClass(fullname).Is_Available();
		init_trace("Play_Intro selecting hi-res=%d file=%s session=%d", hires_available, fullname, Session.Type);
		if (hires_available) {
			const bool hires_played = Try_Play_Movie(VQ_REDINTRO, THEME_NONE, false);
			init_trace("Play_Intro hi-res result=%d", hires_played ? 1 : 0);
			if (!hires_played) {
				init_trace("Play_Intro hi-res intro failed, falling back to %s", VQName[VQ_TITLE]);
				(void)Try_Play_Movie(VQ_TITLE, THEME_NONE, false);
			}
		} else {
			init_trace("Play_Intro hi-res intro unavailable, falling back to %s", VQName[VQ_TITLE]);
			(void)Try_Play_Movie(VQ_TITLE, THEME_NONE, false);
		}
	}
}

/***********************************************************************************************
 * Anim_Init -- Initialize the VQ animation control structure.                                 *
 *                                                                                             *
 *    VQ animations are controlled by a structure passed to the VQ player. This routine        *
 *    initializes the structure to values required by C&C.                                     *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Only need to call this routine once at the beginning of the game.               *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/20/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
#ifdef MOVIE640
GraphicBufferClass VQ640(640, 400, (void *)NULL);
#endif
void Anim_Init(void)
{

	/* Configure player with INI file */
	VQA_DefaultConfig(&AnimControl);
	AnimControl.DrawFlags = VQACFGF_TOPLEFT;
	AnimControl.DrawFlags |= VQACFGF_BUFFER;
//AnimControl.DrawFlags |= VQACFGF_NODRAW;
//BG - M. Grayford says turn this off	AnimControl.DrawFlags |= VQACFGF_NOSKIP;

	AnimControl.DrawFlags |= VQACFGF_NOSKIP;
	AnimControl.FrameRate = -1;
	AnimControl.DrawRate = -1;
	AnimControl.DrawerCallback = VQ_Call_Back;
	AnimControl.ImageWidth = 320;
	AnimControl.ImageHeight = 200;
	AnimControl.ImageBuf = (uint8_t *)SysMemPage.Get_Offset();
#ifdef MOVIE640
	if(IsVQ640) {
		AnimControl.ImageWidth = 640;
		AnimControl.ImageHeight = 400;
		AnimControl.ImageBuf = (uint8_t *)VQ640.Get_Offset();
	}
#endif
	AnimControl.Vmode = 0;
	AnimControl.OptionFlags |= VQAOPTF_CAPTIONS|VQAOPTF_EVA;
	if (SlowPalette) {
		AnimControl.OptionFlags |= VQAOPTF_SLOWPAL;
	}
	AnimControl.AudioObject = SoundObject;
	AnimControl.PrimaryBuffer = PrimaryBufferPtr;
	if (MonoClass::Is_Enabled()) {
		AnimControl.OptionFlags |= VQAOPTF_MONO;
	}

}

/***********************************************************************************************
 * Parse_Command_Line -- Parses the command line parameters.                                   *
 *                                                                                             *
 *    This routine should be called before the graphic mode is initialized. It examines the    *
 *    command line parameters and sets the appropriate globals. If there is an error, then     *
 *    it outputs a command summary and then returns false.                                     *
 *                                                                                             *
 * INPUT:   argc  -- The number of command line arguments.                                     *
 *                                                                                             *
 *          argv  -- Pointer to character string array that holds the individual arguments.    *
 *                                                                                             *
 * OUTPUT:  bool; Was the command line parsed successfully?                                    *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/18/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool Parse_Command_Line(int argc, char * argv[])
{
	/*
	**	Parse the command line and set globals to reflect the parameters
	**	passed in.
	*/
	Whom = HOUSE_GOOD;
	Special.Init();

	Debug_Map = false;
	Debug_Unshroud = false;

#if RA_ENABLE_TCP
	Internet_Clear_Command_Line_Connect();
	Internet_Clear_Command_Line_Name();
#endif

	for (int index = 1; index < argc; index++) {
		char * string;		// Pointer to argument.

		if (argv[index] != NULL && SDL_strcasecmp(argv[index], "-gamedata") == 0) {
			if (index + 1 < argc) {
				++index;
			}
			continue;
		}

#if RA_ENABLE_TCP
		if (argv[index] != NULL && SDL_strcasecmp(argv[index], "-host") == 0) {
			uint16_t port = PlanetWestwoodPortNumber;

			if (Internet_Has_Command_Line_Connect()) {
				puts(TEXT_OPTIONS);
				return(false);
			}

			if (index + 1 < argc && argv[index + 1] != NULL && argv[index + 1][0] != '\0' && argv[index + 1][0] != '-') {
				if (!Internet_Parse_Port(argv[index + 1], &port)) {
					puts(TEXT_OPTIONS);
					return(false);
				}
				++index;
			}

			Internet_Set_Command_Line_Host(port);
			Session.Type = GAME_INTERNET;
			continue;
		}

		if (argv[index] != NULL && SDL_strcasecmp(argv[index], "-connect") == 0) {
			uint16_t port = 0;

			if (Internet_Has_Command_Line_Connect() ||
				index + 2 >= argc ||
				argv[index + 1] == NULL ||
				argv[index + 1][0] == '\0' ||
				argv[index + 2] == NULL ||
				argv[index + 2][0] == '\0' ||
				!Internet_Parse_Port(argv[index + 2], &port)) {
				puts(TEXT_OPTIONS);
				return(false);
			}

			Internet_Set_Command_Line_Connect(argv[index + 1], port);
			Session.Type = GAME_INTERNET;
			index += 2;
			continue;
		}

		if (argv[index] != NULL && SDL_strcasecmp(argv[index], "-name") == 0) {
			if (index + 1 >= argc || argv[index + 1] == NULL || argv[index + 1][0] == '\0' ||
				SDL_strlen(argv[index + 1]) >= sizeof(Session.Handle)) {
				puts(TEXT_OPTIONS);
				return(false);
			}

			Internet_Set_Command_Line_Name(argv[index + 1]);
			++index;
			continue;
		}
#endif

		string = SDL_strupr(argv[index]);

		/*
		**	Print usage text only if requested.
		*/
		if (SDL_strcasecmp("/?", string) == 0 || SDL_strcasecmp("-?", string) == 0 || SDL_strcasecmp("-h", string) == 0 || SDL_strcasecmp("/h", string) == 0) {
			/*
			**	Unrecognized command line parameter... Display usage
			**	and then exit.
			*/
			puts(TEXT_OPTIONS);
			return(false);
		}

		bool processed = true;
		int32_t ob = Obfuscate(string);

		/*
		**	Check to see if the parameter is a cheat enabling one.
		*/
		int32_t const * optr = &CheatCodes[0];
		while (*optr) {
			if (*optr++ == ob) {
				Debug_Playtest = true;
				Debug_Flag = true;
				break;
			}
		}

		/*
		**	Check to see if the parameter is a cheat enabling one.
		*/
		optr = &PlayCodes[0];
		while (*optr) {
			if (*optr++ == ob) {
				Debug_Playtest = true;
				Debug_Flag = true;
				break;
			}
		}

		/*
		**	Check to see if the parameter is a scenario editor
		**	enabling one.
		*/
		optr = &EditorCodes[0];
		while (*optr) {
			if (*optr++ == ob) {
				Debug_Map = true;
				Debug_Unshroud = true;
				Debug_Flag = true;
				Debug_Playtest = true;
				break;
			}
		}

		switch (ob) {

#ifdef VIRGIN_CHEAT_KEYS
			case PARM_PLAYTEST:
				Debug_Playtest = true;
				break;
#endif

			/*
			** Special flag - is C&C being run from the install program?
			*/
			case PARM_INSTALL:
				Special.IsFromInstall = true;
// If uncommented, will disable the <ESC> key during the first movie run.
//				BreakoutAllowed = false;
				break;

			default:
				processed = false;
				break;
		}
		if (processed) continue;

#ifdef CHEAT_KEYS
		/*
		**	Scenario Editor Mode
		*/
		if (SDL_strcasecmp(string, "-CHECKMAP") == 0) {
			Debug_Check_Map = true;
			continue;
		}

#endif

#if (0)
		/*
		** Build speed modifier
		*/
		if (strstr (string, "-UNITRATE:")){
			int unit_rate;
			sscanf (string, "-UNITRATE:%d", &unit_rate);
			UnitBuildPenalty = unit_rate;
		}
#endif	//(0)

		/*
		**	Specify destination connection for network play
		*/
		if (strstr(string, "-DESTNET")) {
			NetNumType net;
			NetNodeType node;

			/*
			** Scan the command-line string, pulling off each address piece
			*/
			int i = 0;
			char * p = strtok(string + 8, ".");
			while (p) {
				int x;

				sscanf(p, "%x", &x);			// convert from hex string to int
				if (i < 4) {
					net[i] = (char)x;			// fill NetNum
				} else {
					node[i-4] = (char)x;		// fill NetNode
				}
				i++;
				p = strtok(NULL, ".");
			}

			/*
			** If all the address components were successfully read, fill in the
			** BridgeNet with a broadcast address to the network across the bridge.
			*/
			if (i >= 4) {
				Session.IsBridge = 1;
				memset(node, 0xff, 6);
				Session.BridgeNet = UDPAddressClass(net, node);
			}
			continue;
		}

		/*
		**	Specify socket ID, as an offset from 0x4000.
		*/
		if (strstr(string, "-SOCKET")) {
			uint16_t socket;

			socket = (uint16_t)(atoi(string + strlen("SOCKET")));
			socket += 0x4000;
			if (socket >= 0x4000 && socket < 0x8000) {
				Udp.Set_Socket (socket);
			}
			continue;
		}

		/*
		**	Set the Net Stealth option
		*/
		if (strstr(string, "-STEALTH")) {
			Session.NetStealth = true;
			continue;
		}

		/*
		**	Set the Net Protection option
		*/
		if (strstr(string, "-MESSAGES")) {
			Session.NetProtect = false;
			continue;
		}

		/*
		**	Allow "attract" mode
		*/
		if (strstr(string, "-ATTRACT")) {
			Session.Attract = true;
			continue;
		}

		/*
		** Set screen to 640x480 instead of 640x400
		*/
		if (strstr(string, "-480")) {
			ScreenHeight = 480;
			continue;
		}

#ifdef CHEAT_KEYS
		/*
		**	Specify the random number seed (for debugging)
		*/
		if (strstr(string, "-SEED")) {
			CustomSeed = (uint16_t)(atoi(string + strlen("SEED")));
			continue;
		}

#endif


		/*
		** look for passed-in video mode to default to
		*/

#ifdef CHEAT_KEYS
		if (strstr(string,"-NOMOVIES")){
			bNoMovies = true;
		}
#endif

		/*
		**	Special command line control parsing.
		*/
		if (SDL_strncasecmp(string, "-X", strlen("-O")) == 0) {
			string += strlen("-X");
			while (*string) {
				char code = *string++;
				switch (toupper(code)) {

#ifdef  CHEAT_KEYS
					/*
					**	Monochrome debug screen enable.
					*/
					case 'M':
						MonoClass::Enable();
						break;

					/*
					**	Inert weapons -- no units take damage.
					*/
					case 'I':
						Special.IsInert = true;
						break;

					/*
					**	Hussled recharge timer.
					*/
					case 'H':
						Special.IsSpeedBuild = true;
						break;

					/*
					**	"Record" a multi-player game
					*/
					case 'X':
						Session.Record = 1;
						break;

					/*
					**	"Play Back" a multi-player game
					*/
					case 'Y':
						Session.Play = 1;
						break;

					/*
					**	Print lots of debug stuff about events & packets
					*/
					case 'P':
						Debug_Print_Events = true;
						break;
#endif

					/*
					**	Quiet mode override control.
					*/
					case 'Q':
						Debug_Quiet = true;
						break;

					default:
						puts(TEXT_INVALID);
						return(false);
				}

			}

			continue;
		}
	}
	return(true);
}

/***********************************************************************************************
 * Obfuscate -- Sufficiently transform parameter to thwart casual hackers.                     *
 *                                                                                             *
 *    This routine borrows from CRC and PGP technology to sufficiently alter the parameter     *
 *    in order to make it difficult to reverse engineer the key phrase. This is designed to    *
 *    be used for hidden game options that will be released at a later time over Westwood's    *
 *    Web page or through magazine hint articles.                                              *
 *                                                                                             *
 *    This algorithm is cryptographically categorized as a "one way hash".                     *
 *                                                                                             *
 *    Since this is a one way transformation, it becomes much more difficult to reverse        *
 *    engineer the pass phrase even if the resultant pass code is known. This has an added     *
 *    benefit of making this algorithm immune to traditional cryptographic attacks.            *
 *                                                                                             *
 *    The largest strength of this transformation algorithm lies in the restriction on the     *
 *    source vector being legal ASCII uppercase characters. This restriction alone makes even  *
 *    a simple CRC transformation practically impossible to reverse engineer. This algorithm   *
 *    uses far more than a simple CRC transformation to achieve added strength from advanced   *
 *    attack methods.                                                                          *
 *                                                                                             *
 * INPUT:   string   -- Pointer to the key phrase that will be transformed into a code.        *
 *                                                                                             *
 * OUTPUT:  Returns with the code that the key phrase is translated into.                      *
 *                                                                                             *
 * WARNINGS:   A zero length pass phrase results in a 0x00000000 result code.                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
int32_t Obfuscate(char const * string)
{
	char buffer[128];

	if (!string) return(0);
	memset(buffer, '\xA5', sizeof(buffer));

	/*
	**	Copy key phrase into a working buffer. This hides any transformation done
	**	to the string.
	*/
	strncpy(buffer, string, sizeof(buffer));
	buffer[sizeof(buffer)-1] = '\0';
	int length = strlen(buffer);
	int index;

	/*
	**	Only upper case letters are significant.
	*/
	SDL_strupr(buffer);

	/*
	**	Ensure that only visible ASCII characters compose the key phrase. This
	**	discourages the direct forced illegal character input method of attack.
	*/
	for (int index = 0; index < length; index++) {
		if (!isgraph(buffer[index])) {
			buffer[index] = 'A' + (index%26);
		}
	}

	/*
	**	Increase the strength of even short pass phrases by extending the
	**	length to be at least a minimum number of characters. This helps prevent
	**	a weak pass phrase from compromising the obfuscation process. This
	**	process also forces the key phrase to be an even multiple of four.
	**	This is necessary to support the cypher process that occurs later.
	*/
	if (length < 16 || (length & 0x03)) {
		int maxlen = 16;
		if (((length+3) & 0x00FC) > maxlen) {
			maxlen = ((length+3) & 0x00FC);
		}
		for (index = length; index < maxlen; index++) {
			buffer[index] = 'A' + ((('?' ^ buffer[index-length]) + index) % 26);
		}
		length = index;
		buffer[length] = '\0';
	}

	/*
	**	Transform the buffer into a number. This transformation is character
	**	order dependant.
	*/
	int32_t code = Calculate_CRC(buffer, length);

	/*
	**	Record a copy of this initial transformation to be used in a later
	**	self referential transformation.
	*/
	int32_t copy = code;

	/*
	**	Reverse the character string and combine with the previous transformation.
	**	This doubles the workload of trying to reverse engineer the CRC calculation.
	*/
	SDL_strrev(buffer);
	code ^= Calculate_CRC(buffer, length);

	/*
	**	Perform a self referential transformation. This makes a reverse engineering
	**	by using a cause and effect attack more difficult.
	*/
	code = code ^ copy;

	/*
	**	Unroll and combine the code value into the pass phrase and then perform
	**	another self referential transformation. Although this is a trivial cypher
	**	process, it gives the sophisticated hacker false hope since the strong
	**	cypher process occurs later.
	*/
	SDL_strrev(buffer);		// Restore original string order.
	for (index = 0; index < length; index++) {
		code ^= (uint8_t)buffer[index];
		uint8_t temp = (uint8_t)code;
		buffer[index] ^= temp;
		code >>= 8;
		code |= (((int32_t)temp)<<24);
	}

	/*
	**	Introduce loss into the vector. This strengthens the key against traditional
	**	cryptographic attack engines. Since this also weakens the key against
	**	unconventional attacks, the loss is limited to less than 10%.
	*/
	for (index = 0; index < length; index++) {
		static uint8_t _lossbits[] = {0x00,0x08,0x00,0x20,0x00,0x04,0x10,0x00};
		static uint8_t _addbits[] = {0x10,0x00,0x00,0x80,0x40,0x00,0x00,0x04};

		buffer[index] |= _addbits[index % (sizeof(_addbits)/sizeof(_addbits[0]))];
		buffer[index] &= ~_lossbits[index % (sizeof(_lossbits)/sizeof(_lossbits[0]))];
	}

	/*
	**	Perform a general cypher transformation on the vector
	**	and use the vector itself as the cypher key. This is a variation on the
	**	cypher process used in PGP. It is a very strong cypher process with no known
	**	weaknesses. However, in this case, the cypher key is the vector itself and this
	**	opens up a weakness against attacks that have access to this transformation
	**	algorithm. The sheer workload of reversing this transformation should be enough
	**	to discourage even the most determined hackers.
	*/
	for (index = 0; index < length; index += 4) {
		int16_t key1 = buffer[index];
		int16_t key2 = buffer[index+1];
		int16_t key3 = buffer[index+2];
		int16_t key4 = buffer[index+3];
		int16_t val1 = key1;
		int16_t val2 = key2;
		int16_t val3 = key3;
		int16_t val4 = key4;

		val1 *= key1;
		val2 += key2;
		val3 += key3;
		val4 *= key4;

		int16_t s3 = val3;
		val3 ^= val1;
		val3 *= key1;
		int16_t s2 = val2;
		val2 ^= val4;
		val2 += val3;
		val2 *= key3;
		val3 += val2;

		val1 ^= val2;
		val4 ^= val3;

		val2 ^= s3;
		val3 ^= s2;

		buffer[index] = val1;
		buffer[index+1] = val2;
		buffer[index+2] = val3;
		buffer[index+3] = val4;
	}

	/*
	**	Convert this final vector into a cypher key code to be
	**	returned by this routine.
	*/
	code = Calculate_CRC(buffer, length);

	/*
	**	Return the final code value.
	*/
	return(code);
}

/***********************************************************************************************
 * Calculate_CRC -- Calculates a one-way hash from a data block.                               *
 *                                                                                             *
 *    This routine is used to create a hash value from a data block. The algorithm is similar  *
 *    to a CRC, but is faster.                                                                 *
 *                                                                                             *
 * INPUT:   buffer   -- Pointer to a buffer of data to be 'hashed'.                            *
 *                                                                                             *
 *          len      -- The length of the buffer to compute the hash upon.                     *
 *                                                                                             *
 * OUTPUT:  Returns with a 32bit hash value calculated from the specified buffer.              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/02/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
int32_t Calculate_CRC(void * buffer, int32_t len)
{
	return(CRCEngine()(buffer, len));
}

/***************************************************************************
 * Init_Random -- Initializes the random-number generator                  *
 *                                                                         *
 * INPUT:                                                                  *
 *		none.																						*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		none.																						*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   12/04/1995 BRR : Created.                                             *
 *=========================================================================*/
void Init_Random(void)
{

		/*
		**	Gather some "random" bits from the system timer. Actually, only the
		**	low order millisecond bits are secure. The other bits could be
		**	easily guessed from the system clock (most clocks are fairly accurate
		**	and thus predictable).
		*/
		SDL_Time current_time = 0;
		SDL_DateTime t = {};
		SDL_GetCurrentTime(&current_time);
		SDL_TimeToDateTime(current_time, &t, false);
		CryptRandom.Seed_Byte(t.nanosecond / SDL_NS_PER_MS);
		CryptRandom.Seed_Bit(t.second);
		CryptRandom.Seed_Bit(t.second>>1);
		CryptRandom.Seed_Bit(t.second>>2);
		CryptRandom.Seed_Bit(t.second>>3);
		CryptRandom.Seed_Bit(t.second>>4);
		CryptRandom.Seed_Bit(t.minute);
		CryptRandom.Seed_Bit(t.minute>>1);
		CryptRandom.Seed_Bit(t.minute>>2);
		CryptRandom.Seed_Bit(t.minute>>3);
		CryptRandom.Seed_Bit(t.minute>>4);
		CryptRandom.Seed_Bit(t.hour);
		CryptRandom.Seed_Bit(t.day);
		CryptRandom.Seed_Bit(t.day_of_week);
		CryptRandom.Seed_Bit(t.month);
		CryptRandom.Seed_Bit(t.year);

#ifdef FIXIT_MULTI_SAVE
	//
	// If we've loaded a multiplayer save game, return now; the random #
	// class is loaded along with ScenarioClass.
	//
	if (Session.LoadGame) {
		return;
	}

	//
	// If we're playing a recording, the Seed is loaded in
	// Load_Recording_Values().  Just init the random # and return.
	//
	if (Session.Play) {
		RandNumb = Seed;
		Scen.RandomNumber = Seed;
		return;
	}
#else
	/*
	** Do nothing if we've loaded a multiplayer game, or we're playing back
	** a recording; the random number generator is initialized by loading
	** the game.
	*/
	if (Session.LoadGame || Session.Play) {
		RandNumb = Seed;
		Scen.RandomNumber = Seed;
		return;
	}
#endif	// FIXIT_MULTI_SAVE

	/*
	**	Initialize the random number Seed.  For multiplayer, this will have been done
	** in the connection dialogs.  For single-player games, AND if we're not playing
	** back a recording, init the Seed to a random value.
	*/
	if (Session.Type == GAME_NORMAL || (Session.Type == GAME_SKIRMISH &&
		!Session.Play)) {

		/*
		** Set the optional user-specified seed
		*/
		if (CustomSeed != 0) {
			Seed = CustomSeed;
		} else {
			srand(time(NULL));
			Seed = rand();
		}
	}

	/*
	**	Initialize the random-number generators
	*/
	Scen.RandomNumber = Seed;
	RandNumb = Seed;
}

/***********************************************************************************************
 * Load_Title_Page -- Load the background art for the title page.                              *
 *                                                                                             *
 *    This routine will load the background art in a machine independent format. There is      *
 *    different art required for the hi-res and lo-res versions of the game.                   *
 *                                                                                             *
 * INPUT:   visible  -- Should the title page art be copied to the visible page by this        *
 *                      routine?                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Be sure the mouse is hidden if the image is to be copied to the visible page.   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
void Load_Title_Page(bool visible)
{
	Load_Title_Screen("TITLE.PCX", &HidPage, CCPalette);
	if (visible) {
		HidPage.Blit(SeenPage);
	}
}

/***********************************************************************************************
 * Init_Color_Remaps -- Initialize the text remap tables.                                      *
 *                                                                                             *
 *    There are various color scheme remap tables that are dependant upon the color remap      *
 *    information embedded within the palette control file. This routine will fetch that       *
 *    data and build the text remap tables as indicated.                                       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Color_Remaps(void)
{
	/*
	**	Setup the remap tables.  PALETTE.CPS contains a special set of pixels in
	** the upper-left corner.  Each row of 16 pixels is one range of colors.  The
	** first row represents unity (the default color units are drawn in); rows
	** after that are the remap colors.
	*/

	SysMemPage.Clear();
	Load_Picture("PALETTE.CPS", SysMemPage, SysMemPage, NULL, BM_DEFAULT);
	SysMemPage.Blit(HidPage);
	for (PlayerColorType pcolor = PCOLOR_FIRST; pcolor < PCOLOR_COUNT; pcolor++) {

		uint8_t * ptr = ColorRemaps[pcolor].RemapTable;

		for (int color = 0; color < 256; color++) {
			ptr[color] = color;
		}

		for (int index = 0; index < 16; index++) {
			ptr[HidPage.Get_Pixel(index, 0)] = HidPage.Get_Pixel(index, pcolor);
		}
		for (int index = 0; index < 6; index++) {
			ColorRemaps[pcolor].FontRemap[10+index] = HidPage.Get_Pixel(2+index, pcolor);
		}
		ColorRemaps[pcolor].BrightColor = WHITE;
//		ColorRemaps[pcolor].BrightColor = HidPage.Get_Pixel(1, pcolor);
		ColorRemaps[pcolor].Color = HidPage.Get_Pixel(4, pcolor);

		ColorRemaps[pcolor].Shadow = HidPage.Get_Pixel(10, pcolor);
		ColorRemaps[pcolor].Background = HidPage.Get_Pixel(9, pcolor);
		ColorRemaps[pcolor].Corners = HidPage.Get_Pixel(7, pcolor);
		ColorRemaps[pcolor].Highlight = HidPage.Get_Pixel(4, pcolor);
		ColorRemaps[pcolor].Bright = HidPage.Get_Pixel(0, pcolor);
		ColorRemaps[pcolor].Underline = HidPage.Get_Pixel(0, pcolor);
		ColorRemaps[pcolor].Bar = HidPage.Get_Pixel(6, pcolor);

		/*
		**	This must grab from column 4 because the multiplayer color dialog palette counts
		**	on this to be true.
		*/
		ColorRemaps[pcolor].Box = HidPage.Get_Pixel(4, pcolor);
	}

	/*
	** Now do the special dim grey scheme
	*/
	for (int color = 0; color < 256; color++) {
		GreyScheme.RemapTable[color] = color;
	}
	for (int index = 0; index < 6; index++) {
		GreyScheme.FontRemap[10+index] = HidPage.Get_Pixel(9+index, PCOLOR_GREY) & 0x00FF;
	}
	GreyScheme.BrightColor = HidPage.Get_Pixel(3, PCOLOR_GREY) & 0x00FF;
	GreyScheme.Color = HidPage.Get_Pixel(7, PCOLOR_GREY) & 0x00FF;

	GreyScheme.Shadow = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(15, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Background = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(14, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Corners = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(13, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Highlight = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(9, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Bright = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(5, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Underline = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(5, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Bar = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(11, PCOLOR_GREY) & 0x00FF];
	GreyScheme.Box = ColorRemaps[PCOLOR_GREY].RemapTable[HidPage.Get_Pixel(11, PCOLOR_GREY) & 0x00FF];

	/*
	** Set up the metallic remap table for the font that prints over the tabs
	*/
	memset ((void*)&MetalScheme, 4, sizeof(MetalScheme));
	for (int color_counter = 0; color_counter < 16; color_counter++) {
		MetalScheme.FontRemap[color_counter] = color_counter;
	}
	MetalScheme.FontRemap[1] = 128;
	MetalScheme.FontRemap[2] = 12;
	MetalScheme.FontRemap[3] = 13;
	MetalScheme.FontRemap[4] = 14;
	MetalScheme.Color = 128;
	MetalScheme.Background = 0;
	MetalScheme.Underline = 128;

	/*
	** Set up the font remap table for the mission briefing font
	*/
	for (int colr = 0; colr < 16; colr++) {
		ColorRemaps[PCOLOR_TYPE].FontRemap[colr] = HidPage.Get_Pixel(colr, PCOLOR_TYPE);
	}

	ColorRemaps[PCOLOR_TYPE].Shadow = 11;
	ColorRemaps[PCOLOR_TYPE].Background = 10;
	ColorRemaps[PCOLOR_TYPE].Corners = 10;
	ColorRemaps[PCOLOR_TYPE].Highlight = 9;
	ColorRemaps[PCOLOR_TYPE].Bright = 15;
	ColorRemaps[PCOLOR_TYPE].Underline = 11;
	ColorRemaps[PCOLOR_TYPE].Bar = 11;
	ColorRemaps[PCOLOR_TYPE].Box = 10;
	ColorRemaps[PCOLOR_TYPE].BrightColor = 15;
	ColorRemaps[PCOLOR_TYPE].Color = 9;

	GadgetClass::Set_Color_Scheme(&ColorRemaps[PCOLOR_DIALOG_BLUE]);
//	GadgetClass::Set_Color_Scheme(&ColorRemaps[PCOLOR_BLUE]);
}

/***********************************************************************************************
 * Init_Heaps -- Initialize the game heaps and buffers.                                        *
 *                                                                                             *
 *    This routine will allocate the game heaps and buffers. The rules file has already been   *
 *    processed by the time that this routine is called.                                       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Heaps(void)
{
	/*
	**	Initialize the game object heaps.
	*/
	Vessels.Set_Heap(Rule.VesselMax);
	Units.Set_Heap(Rule.UnitMax);
	Factories.Set_Heap(Rule.FactoryMax);
	Terrains.Set_Heap(Rule.TerrainMax);
	Templates.Set_Heap(Rule.TemplateMax);
	Smudges.Set_Heap(Rule.SmudgeMax);
	Overlays.Set_Heap(Rule.OverlayMax);
	Infantry.Set_Heap(Rule.InfantryMax);
	Bullets.Set_Heap(Rule.BulletMax);
	Buildings.Set_Heap(Rule.BuildingMax);
	Anims.Set_Heap(Rule.AnimMax);
	Aircraft.Set_Heap(Rule.AircraftMax);
	Triggers.Set_Heap(Rule.TriggerMax);
	TeamTypes.Set_Heap(Rule.TeamTypeMax);
	Teams.Set_Heap(Rule.TeamMax);
	Houses.Set_Heap(HOUSE_MAX);
	TriggerTypes.Set_Heap(Rule.TrigTypeMax);
//	Weapons.Set_Heap(Rule.WeaponMax);

	/*
	**	Speech holding tank buffer. Since speech does not mix, it can be placed
	**	into a custom holding tank only as large as the largest speech file to
	**	be played.
	*/
	for (int index = 0; index < static_cast<int>(ARRAY_SIZE(SpeechBuffer)); index++) {
		SpeechBuffer[index] = new char [SPEECH_BUFFER_SIZE];
		SpeechRecord[index] = VOX_NONE;
		assert(SpeechBuffer[index] != NULL);
	}

	/*
	**	Allocate the theater buffer block.
	*/
	TheaterBuffer = new Buffer(THEATER_BUFFER_SIZE);
	assert(TheaterBuffer != NULL);
}

/***********************************************************************************************
 * Init_Expansion_Files -- Fetch any override expansion mixfiles.                              *
 *                                                                                             *
 *    This routine will search for and register/cache any override mixfiles found.             *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Expansion_Files(void)
{
	Register_GameData_Base_Mixfiles();
	Register_GameData_Main_Mixfile();
	if (GeneralMix == NULL && CCFileClass("GENERAL.MIX").Is_Available()) {
		GeneralMix = new MFCD("GENERAL.MIX", &FastKey);
		assert(GeneralMix != NULL);
	}

	Register_Expansion_Campaign_Scenario_Mixes();
	Register_Expansion_Scenario_Mixes_From_Packet("CSTRIKE.PKT");
#ifdef FIXIT_CSII
	Register_Expansion_Scenario_Mixes_From_Packet("AFTMATH.PKT");
#endif

	/*
	**	Before all else, cache any additional mixfiles.
	*/
	Register_Expansion_Mixfiles_From_Directory(".", "SC*.MIX", true);
	Register_Expansion_Mixfiles_From_Directory(".", "SS*.MIX", false);
}

/***********************************************************************************************
 * Init_One_Time_Systems -- Initialize internal pointers to the bulk data.                     *
 *                                                                                             *
 *    This performs the one-time processing required after the bulk data has been cached but   *
 *    before the game actually starts. Typically, this routine extracts pointers to all the    *
 *    embedded data sub-files within the main game data mixfile. This routine must be called   *
 *    AFTER the bulk data has been cached.                                                     *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Call this routine AFTER the bulk data has been cached.                          *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_One_Time_Systems(void)
{
	init_trace("Init_One_Time_Systems start");
	Call_Back();
	init_trace("Init_One_Time_Systems Map.One_Time");
	Map.One_Time();
	init_trace("Init_One_Time_Systems Map.One_Time complete");
	Call_Back();
	init_trace("Init_One_Time_Systems Logic.One_Time");
	Logic.One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems Options.One_Time");
	Options.One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems Session.One_Time");
	Session.One_Time();
#if RA_ENABLE_TCP
	Internet_Apply_Command_Line_Name();
#endif
	Call_Back();

	init_trace("Init_One_Time_Systems ObjectTypeClass::One_Time");
	ObjectTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems BuildingTypeClass::One_Time");
	BuildingTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems BulletTypeClass::One_Time");
	BulletTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems HouseTypeClass::One_Time");
	HouseTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems TemplateTypeClass::One_Time");
	TemplateTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems OverlayTypeClass::One_Time");
	OverlayTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems SmudgeTypeClass::One_Time");
	SmudgeTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems TerrainTypeClass::One_Time");
	TerrainTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems UnitTypeClass::One_Time");
	UnitTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems VesselTypeClass::One_Time");
	VesselTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems InfantryTypeClass::One_Time");
	InfantryTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems AnimTypeClass::One_Time");
	AnimTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems AircraftTypeClass::One_Time");
	AircraftTypeClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems HouseClass::One_Time");
	HouseClass::One_Time();
	Call_Back();
	init_trace("Init_One_Time_Systems complete");
}

/***********************************************************************************************
 * Init_Fonts -- Initialize all the game font pointers.                                        *
 *                                                                                             *
 *    This routine is used to fetch pointers to the game fonts. The mixfile containing these   *
 *    fonts must have been previously cached. This routine is a necessary prerequisite to      *
 *    displaying any dialogs or printing any text.                                             *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Fonts(void)
{
	Metal12FontPtr = MFCD::Retrieve("12METFNT.FNT");
	MapFontPtr = MFCD::Retrieve("HELP.FNT");
	Font6Ptr = MFCD::Retrieve("6POINT.FNT");
	GradFont6Ptr = MFCD::Retrieve("GRAD6FNT.FNT");
	EditorFont = MFCD::Retrieve("EDITFNT.FNT");
	Font8Ptr = MFCD::Retrieve("8POINT.FNT");
	FontPtr = (char *)Font8Ptr;
	Set_Font(FontPtr);
	Font3Ptr = MFCD::Retrieve("3POINT.FNT");
	ScoreFontPtr = MFCD::Retrieve("SCOREFNT.FNT");
	FontLEDPtr = MFCD::Retrieve("LED.FNT");
	VCRFontPtr = MFCD::Retrieve("VCR.FNT");
	TypeFontPtr = MFCD::Retrieve("8POINT.FNT");    //("TYPE.FNT"); //VG 10/17/96
}

/***********************************************************************************************
 * Init_Game_Data_Access -- Validate the fixed GameData asset layout.                          *
 *                                                                                             *
 *    This routine is called during startup to ensure the selected asset directory contains    *
 *    the core archives required to continue bootstrapping the game.                           *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   The fonts, palettes, and other bootstrap systems must have been initialized     *
 *             prior to calling this routine because this routine may display a fatal          *
 *             startup error if required data is missing.                                      *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Game_Data_Access(void)
{
	const bool data_ready = Has_Required_Game_Data();
	VisiblePage.Clear();
	HidPage.Clear();
	init_trace("Init_Game_Data_Access start data_ready=%d", data_ready);

	if (data_ready) {
		return;
	}

	VisiblePage.Clear();
	GamePalette.Set();
	Show_Mouse();
	WWMessageBox().Process("Required game data was not found.", TXT_OK);
	Emergency_Exit(EXIT_FAILURE);
}

/***********************************************************************************************
 * Init_Bootstrap_Mixfiles -- Registers and caches any mixfiles needed for bootstrapping.      *
 *                                                                                             *
 *    This routine will register the initial mixfiles that are required to display error       *
 *    messages and get input from the player.                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Be sure to call this routine before any dialogs would be displayed to the       *
 *             player.                                                                         *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Bootstrap_Mixfiles(void)
{
#ifdef WOLAPI_INTEGRATION
	CCFileClass fileWolapiMix( "WOLAPI.MIX" );
	if( fileWolapiMix.Is_Available() )
	{
		new MFCD( "WOLAPI.MIX", &FastKey );
		MFCD::Cache( "WOLAPI.MIX" );
	}
#endif

#ifdef FIXIT_CSII	//	Ok. ajw
	CCFileClass file2("EXPAND2.MIX");
	if (file2.Is_Available()) {
		new MFCD("EXPAND2.MIX", &FastKey);
		bool ok = MFCD::Cache("EXPAND2.MIX");
		assert(ok);
		(void)ok;
	}
#endif

#ifdef FIXIT_CSII	//	Ok. ajw
	bool ok1;
	new MFCD("HIRES1.MIX", &FastKey);
	ok1 = MFCD::Cache("HIRES1.MIX");
	assert(ok1);
	(void)ok1;
#endif

#ifdef FIXIT_ANTS	//	Ok. ajw
	CCFileClass file("EXPAND.MIX");
	if (file.Is_Available()) {
		new MFCD("EXPAND.MIX", &FastKey);
		bool ok = MFCD::Cache("EXPAND.MIX");
		assert(ok);
		(void)ok;
	}
#endif

	new MFCD("REDALERT.MIX", &FastKey);

	/*
	**	Bootstrap enough of the system so that the error dialog box can successfully
	**	be displayed.
	*/
	new MFCD("LOCAL.MIX", &FastKey);			// Cached.
	bool ok = MFCD::Cache("LOCAL.MIX");
	assert(ok);
	(void)ok;

	new MFCD("HIRES.MIX", &FastKey);
	ok = MFCD::Cache("HIRES.MIX");
	assert(ok);
	(void)ok;

	new MFCD("NCHIRES.MIX", &FastKey);		//Non-cached hires stuff incl VQ palettes
}

/***********************************************************************************************
 * Init_Secondary_Mixfiles -- Register and cache secondary mixfiles.                           *
 *                                                                                             *
 *    This routine is used to register the mixfiles that are needed for main menu processing.  *
 *    Call this routine before the main menu is display and processed.                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
//#define DENZIL_MIXEXTRACT
#ifdef DENZIL_MIXEXTRACT
void Extract(char* filename, char* outfile);
#endif
 
static void Init_Secondary_Mixfiles(void)
{
	Register_GameData_Main_Mixfile();
	init_trace("Init_Secondary_Mixfiles MAIN.MIX ready");

	//Denzil extract mixfile
	#ifdef DENZIL_MIXEXTRACT
	#if(0)
	Extract("CONQUER.MIX", "o:\\projects\\radvd\\data\\extract\\conquer.mix");
	Extract("EDHI.MIX", "o:\\projects\\radvd\\data\\extract\\edhi.mix");
	Extract("EDLO.MIX", "o:\\projects\\radvd\\data\\extract\\edlo.mix");
	Extract("GENERAL.MIX", "o:\\projects\\radvd\\data\\extract\\general.mix");
	Extract("INTERIOR.MIX", "o:\\projects\\radvd\\data\\extract\\interior.mix");
	Extract("MOVIES1.MIX", "o:\\projects\\radvd\\data\\extract\\movies1.mix");
	Extract("SCORES.MIX", "o:\\projects\\radvd\\data\\extract\\scores.mix");
	Extract("SNOW.MIX", "o:\\projects\\radvd\\data\\extract\\snow.mix");
	Extract("SOUNDS.MIX", "o:\\projects\\radvd\\data\\extract\\sounds.mix");
	Extract("RUSSIAN.MIX", "o:\\projects\\radvd\\data\\extract\\russian.mix");
	Extract("ALLIES.MIX", "o:\\projects\\radvd\\data\\extract\\allies.mix");
	Extract("TEMPERAT.MIX", "o:\\projects\\radvd\\data\\extract\\temperat.mix");
	#else
	Extract("CONQUER.MIX", "o:\\projects\\radvd\\data\\extract\\conquer.mix");
	Extract("EDHI.MIX", "o:\\projects\\radvd\\data\\extract\\edhi.mix");
	Extract("EDLO.MIX", "o:\\projects\\radvd\\data\\extract\\edlo.mix");
	Extract("GENERAL.MIX", "o:\\projects\\radvd\\data\\extract\\general.mix");
	Extract("INTERIOR.MIX", "o:\\projects\\radvd\\data\\extract\\interior.mix");
	Extract("MOVIES2.MIX", "o:\\projects\\radvd\\data\\extract\\movies2.mix");
	Extract("SCORES.MIX", "o:\\projects\\radvd\\data\\extract\\scores.mix");
	Extract("SNOW.MIX", "o:\\projects\\radvd\\data\\extract\\snow.mix");
	Extract("SOUNDS.MIX", "o:\\projects\\radvd\\data\\extract\\sounds.mix");
	Extract("RUSSIAN.MIX", "o:\\projects\\radvd\\data\\extract\\russian.mix");
	Extract("ALLIES.MIX", "o:\\projects\\radvd\\data\\extract\\allies.mix");
	Extract("TEMPERAT.MIX", "o:\\projects\\radvd\\data\\extract\\temperat.mix");
	#endif
	#endif
	
	/*
	**	Inform the file system of the various MIX files.
	*/
	ConquerMix = new MFCD("CONQUER.MIX", &FastKey);			// Cached.
	g_bootstrap_conquer_mix = ConquerMix;
	init_trace("Init_Secondary_Mixfiles CONQUER.MIX ready");
//	new MFCD("TRANSIT.MIX", &FastKey);

	if (GeneralMix == NULL) GeneralMix = new MFCD("GENERAL.MIX", &FastKey);			// Never cached.
	init_trace("Init_Secondary_Mixfiles GENERAL.MIX ready");

	Load_Movie_Mixfiles();
	init_trace("Init_Secondary_Mixfiles movies mix ready");

	/*
	**	Register the score mixfile.
	*/
	ScoresPresent = true;
	ScoreMix = new MFCD("SCORES.MIX", &FastKey);
	init_trace("Init_Secondary_Mixfiles SCORES.MIX ready");
	init_trace("Init_Secondary_Mixfiles ThemeClass::Scan start");
	ThemeClass::Scan();
	init_trace("Init_Secondary_Mixfiles ThemeClass::Scan complete");

	/*
	**	These are sound card specific, but the install program would have
	**	copied the correct versions to the hard drive.
	*/
	new MFCD("SPEECH.MIX", &FastKey);			// Never cached.
	init_trace("Init_Secondary_Mixfiles SPEECH.MIX ready");
	new MFCD("SOUNDS.MIX", &FastKey);			// Cached.
	init_trace("Init_Secondary_Mixfiles SOUNDS.MIX ready");
	new MFCD("RUSSIAN.MIX", &FastKey);			// Cached.
	init_trace("Init_Secondary_Mixfiles RUSSIAN.MIX ready");
	new MFCD("ALLIES.MIX", &FastKey);			// Cached.
	init_trace("Init_Secondary_Mixfiles ALLIES.MIX ready");
}

/***********************************************************************************************
 * Bootstrap -- Perform the initial bootstrap procedure.                                       *
 *                                                                                             *
 *    This routine will load and initialize the game engine such that a dialog box could be    *
 *    displayed. Because this is very critical, call this routine before any other game        *
 *    initialization code.                                                                     *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Bootstrap(void)
{
	BlackPalette.Set();

	/*
	** Process the message loop until we are in focus. We need to be in focus to read pixels from
	** the screen.
	*/
	init_trace("Bootstrap waiting for focus GameInFocus=%d seen=%d", GameInFocus, BootstrapFocusSeen);
	do {
		Keyboard->Check();
		if (!GameInFocus && !BootstrapFocusSeen) {
			SDL_GameInput_Delay(1);
		}
	} while (!GameInFocus && !BootstrapFocusSeen);
	init_trace("Bootstrap focus acquired GameInFocus=%d seen=%d", GameInFocus, BootstrapFocusSeen);
	AllSurfaces.SurfacesRestored = false;

	/*
	**	Perform any special debug-only processing. This includes preparing the
	**	monochrome screen.
	*/
	Mono_Clear_Screen();

	/*
	**	Register and make resident all local mixfiles with particular emphasis
	**	on the mixfiles that are necessary to display and error messages and
	**	process further initialization.
	*/
	Init_Bootstrap_Mixfiles();

	/*
	**	Initialize the resident font pointers.
	*/
	Init_Fonts();

	/*
	**	Setup the keyboard processor in preparation for the game.
	*/
		Keyboard->Clear();

	/*
	**	This is the shape staging buffer. It must always be available, so it is
	**	allocated here and never freed. The library sets the globals ShapeBuffer
	**	and ShapeBufferSize to these values, so it can be accessed for other
	**	purposes.
	*/
	Set_Shape_Buffer(new uint8_t[SHAPE_BUFFER_SIZE], SHAPE_BUFFER_SIZE);

	/*
	**	Fetch the language text from the hard drive first. If it cannot be
	**	found on the hard drive, then look for it in the mixfile.
	*/
#ifdef STEVES_LOAD_OVERRIDE
	RawFileClass strings ("CONQUER.ENG");
	if (strings.Is_Available()){
		SystemStrings = new char [strings.Size()];
		strings.Read((void*)SystemStrings, strings.Size());
	}else{
		SystemStrings = (char const *)MFCD::Retrieve(Language_Name("CONQUER"));
	}
#else
	SystemStrings = (char const *)MFCD::Retrieve(Language_Name("CONQUER"));
#endif
	DebugStrings = (char const *)MFCD::Retrieve("DEBUG.ENG");

	/*
	**	Default palette initialization.
	*/
	memmove((uint8_t *)&GamePalette[0], (void *)MFCD::Retrieve("TEMPERAT.PAL"), 768L);
	WhitePalette[0] = BlackPalette[0];
//	GamePalette.Set();

	/*
	**	Initialize expansion files (if present). Expansion files must be located
	**	in the current directory.
	*/
	Init_Expansion_Files();

	SidebarScheme.Background 	= BLACK;
	SidebarScheme.Corners    	= LTGREY;
	SidebarScheme.Shadow		 	= DKGREY;
	SidebarScheme.Highlight  	= WHITE;
	SidebarScheme.Color		 	= LTGREY;
	SidebarScheme.Bright		 	= WHITE;
	SidebarScheme.BrightColor	= WHITE;
	SidebarScheme.Box		 	 	= LTGREY;
	GadgetClass::Set_Color_Scheme(&SidebarScheme);
}

/***********************************************************************************************
 * Init_Mouse -- Initialize the mouse system.                                                  *
 *                                                                                             *
 *    This routine will ensure that a valid mouse driver is present and a working mouse        *
 *    pointer can be displayed. The mouse is hidden when this routine exits.                   *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Mouse(void)
{
	/*
	** Since there is no mouse shape currently available we need
	** to set one of our own.
	*/
	SDL_GameInput_ShowCursor(false);
	if (MouseInstalled) {
		void const * temp_mouse_shapes = MFCD::Retrieve("MOUSE.SHP");
		if (temp_mouse_shapes) {
			Set_Mouse_Cursor(0, 0, Extract_Shape(temp_mouse_shapes, 0));
			while (Get_Mouse_State() > 1) Show_Mouse();
		}
	} else {
		char buffer[255];
		GamePalette.Set();
		GamePalette.Set();
		sprintf(buffer, TEXT_NO_MOUSE);
		VisiblePage.Clear();
		WWMessageBox().Process(buffer, TXT_OK);
		//Prog_End();
		Emergency_Exit(1);
	}

	Map.Set_Default_Mouse(MOUSE_NORMAL, false);
	Show_Mouse();
	while (Get_Mouse_State() > 1) Show_Mouse();
	Call_Back();
	Hide_Mouse();
}


/***********************************************************************************************
 * Init_Bulk_Data -- Initialize the time-consuming mixfile caching.                            *
 *                                                                                             *
 *    This routine is called to handle the time consuming process of game initialization.      *
 *    The title page will be displayed when this routine is called.                            *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   This routine will take a very long time.                                        *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/03/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Bulk_Data(void)
{
	/*
	**	Cache the main game data. This operation can take a very long time.
	*/
	MFCD::Cache("CONQUER.MIX");
	if (!Debug_Quiet) {
		MFCD::Cache("SOUNDS.MIX");
		MFCD::Cache("RUSSIAN.MIX");
		MFCD::Cache("ALLIES.MIX");
	}
	Call_Back();

	/*
	**	Fetch the tutorial message data.
	*/
	INIClass ini;
	CCFileClass tutorial_file("TUTORIAL.INI");
	ini.Load(tutorial_file);
	for (int index = 0; index < static_cast<int>(ARRAY_SIZE(TutorialText)); index++) {
		TutorialText[index] = NULL;

		char buffer[128];
		char num[10];
		sprintf(num, "%d", index);
		if (ini.Get_String("Tutorial", num, "", buffer, sizeof(buffer))) {
			TutorialText[index] = strdup(buffer);
		}
	}

	/*
	**	Perform one-time game system initializations.
	*/
	Init_One_Time_Systems();
}

/***********************************************************************************************
 * Init_Keys -- Initialize the cryptographic keys.                                             *
 *                                                                                             *
 *    This routine will initialize the fast cryptographic key. It will also initialize the     *
 *    slow one if this is a scenario editor version of the game.                               *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/08/1996 JLB : Created.                                                                 *
 *=============================================================================================*/
static void Init_Keys(void)
{
	RAMFileClass file((void*)Keys, strlen(Keys));
	INIClass ini;
	ini.Load(file);

	FastKey = ini.Get_PKey(true);
#ifdef SCENARIO_EDITOR
	SlowKey = ini.Get_PKey(false);
#endif
}

/***************************************************************************
 * Save_Recording_Values -- Saves multiplayer-specific values              *
 *                                                                         *
 * This routine saves multiplayer values that need to be restored for a		*
 * save game.  In addition to saving the random # seed for this scenario, 	*
 * it saves the contents of the actual random number generator; this 		*
 * ensures that the random # sequencer will pick up where it left off when	*
 * the game was saved.																		*
 * This routine also saves the header for a Recording file, so it must 		*
 * save some data not needed specifically by a save-game file (ie Seed).	*
 *                                                                         *
 * INPUT:                                                                  *
 *		file		file to save to															*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		true = success, false = failure													*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   09/28/1995 BRR : Created.                                             *
 *=========================================================================*/
bool Save_Recording_Values(CCFileClass & file)
{
	Session.Save(file);
	file.Write(&BuildLevel, sizeof(BuildLevel));
	file.Write(&Debug_Unshroud, sizeof(Debug_Unshroud));
	file.Write(&Seed, sizeof(Seed));
	file.Write(&Scen.Scenario, sizeof(Scen.Scenario));
	file.Write(Scen.ScenarioName, sizeof(Scen.ScenarioName));
	file.Write(&Whom, sizeof(Whom));
	file.Write(&Special, sizeof(SpecialClass));
	file.Write(&Options, sizeof(GameOptionsClass));

	return (true);
}

/***************************************************************************
 * Load_Recording_Values -- Loads multiplayer-specific values              *
 *                                                                         *
 * INPUT:                                                                  *
 *		file			file to load from														*
 *                                                                         *
 * OUTPUT:                                                                 *
 *		true = success, false = failure													*
 *                                                                         *
 * WARNINGS:                                                               *
 *		none.																						*
 *                                                                         *
 * HISTORY:                                                                *
 *   09/28/1995 BRR : Created.                                             *
 *=========================================================================*/
bool Load_Recording_Values(CCFileClass & file)
{
	Session.Load(file);
	file.Read(&BuildLevel, sizeof(BuildLevel));
	file.Read(&Debug_Unshroud, sizeof(Debug_Unshroud));
	file.Read(&Seed, sizeof(Seed));
	file.Read(&Scen.Scenario, sizeof(Scen.Scenario));
	file.Read(Scen.ScenarioName, sizeof(Scen.ScenarioName));
	file.Read(&Whom, sizeof(Whom));
	file.Read(&Special, sizeof(SpecialClass));
	file.Read(&Options, sizeof(GameOptionsClass));
	return (true);
}

void __PRO(void) {
//	printf("_pro\n");
}

#ifdef DENZIL_MIXEXTRACT
void Extract(char* filename, char* outname)
	{
	CCFileClass inFile(filename);
	CCFileClass outFile(outname);

	inFile.Open();
	outFile.Open(WRITE);
	
	void* buffer = malloc(32768);

	if (buffer)
		{
		uint32_t size = inFile.Size();
		uint32_t bytes;

		while (size > 0)
			{
			bytes = inFile.Read(buffer, 32768);
			outFile.Write(buffer, bytes);
			size -= bytes;
			}
		
		free(buffer);
		}
	}
#endif
