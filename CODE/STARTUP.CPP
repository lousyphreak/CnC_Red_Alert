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
/* $Header: /counterstrike/STARTUP.CPP 6     3/15/97 7:18p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : STARTUP.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : October 3, 1994                                              *
 *                                                                                             *
 *                  Last Update : September 30, 1996 [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Prog_End -- Cleans up library systems in prep for game exit.                              *
 *   RedAlert_Main -- Initial startup routine (preps library systems).                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include	"SDLINPUT.H"
#include <SDL3/SDL.h>
#include "ra_messagebox.h"
#include	<cstdarg>
#include	<cstring>
#include	<cstdio>
#include	<cstdlib>
#include	<string>

#include <SDL3/SDL_filesystem.h>

#ifdef WOLAPI_INTEGRATION
//#include "WOLDEBUG.H"
#endif

bool Read_Private_Config_Struct(FileClass & file, NewConfigType * config);
void Print_Error_End_Exit(char * string);
void Print_Error_Exit(char * string);

namespace
{
const char* const kStartupGameDataBaseMixfiles[] = {
	"MAIN1.MIX",
	"MAIN2.MIX",
	"MAIN3.MIX",
	"MAIN4.MIX"
};

bool startup_trace_enabled()
{
	static const bool enabled = std::getenv("RA_TRACE_STARTUP") != NULL;
	return enabled;
}

void startup_trace(const char* format, ...)
{
	if (!startup_trace_enabled()) {
		return;
	}

	char message[1024];
	va_list arguments;
	va_start(arguments, format);
	std::vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);

	std::fputs("[startup] ", stderr);
	std::fputs(message, stderr);
	std::fputc('\n', stderr);
	std::fflush(stderr);
}

bool command_line_contains_forbidden_network_path(int argc, char* argv[])
{
	for (int index = 1; index < argc; ++index) {
		const char* argument = argv[index];
		if (argument == NULL) {
			continue;
		}

		if (strstr(argument, "f:\\projects\\c&c0") != NULL ||
				strstr(argument, "F:\\PROJECTS\\C&C0") != NULL) {
			return true;
		}
	}

	return false;
}

void copy_base_path(char* buffer, size_t buffer_size)
{
	if (buffer == NULL || buffer_size == 0) {
		return;
	}

	std::string base_path_string;
#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
	base_path_string = WWFS_GetBaseDirectoryPath();
#else
	const char* base_path = SDL_GetBasePath();
	base_path_string = (base_path != NULL) ? base_path : "./";
#endif
	if (!base_path_string.empty() && base_path_string.back() != '/' && base_path_string.back() != '\\') {
		base_path_string.push_back('/');
	}
	std::snprintf(buffer, buffer_size, "%s", base_path_string.c_str());
}

std::string normalize_directory_path(const char* path)
{
	std::string normalized = WWFS_NormalizePath(path);
	while (normalized.size() > 1 && (normalized.back() == '/' || normalized.back() == '\\')) {
		normalized.pop_back();
	}
	return normalized.empty() ? std::string(".") : normalized;
}

std::string current_working_directory_path()
{
	char* current_directory = SDL_GetCurrentDirectory();
	if (current_directory == NULL || current_directory[0] == '\0') {
		if (current_directory != NULL) {
			SDL_free(current_directory);
		}
		return std::string();
	}

	std::string normalized = normalize_directory_path(current_directory);
	SDL_free(current_directory);
	return normalized;
}

std::string startup_base_directory_path()
{
#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
	const std::string base_directory = WWFS_GetBaseDirectoryPath();
	if (!base_directory.empty()) {
		return normalize_directory_path(base_directory.c_str());
	}
#else
	const char* base_path = SDL_GetBasePath();
	if (base_path != NULL && base_path[0] != '\0') {
		return normalize_directory_path(base_path);
	}
#endif

	return std::string();
}

std::string executable_directory_path(const char* executable_path)
{
	const std::string base_directory = startup_base_directory_path();
	if (!base_directory.empty()) {
		return base_directory;
	}

	char drive[_MAX_DRIVE];
	char path[260];
	char working_directory[260];

	WWFS_SplitPath(executable_path, drive, path, NULL, NULL);
	if (!path[0]) {
		std::strcpy(path, ".");
	}
	size_t path_length = std::strlen(path);
	if (path_length > 0 && (path[path_length - 1] == '\\' || path[path_length - 1] == '/')) {
		path[path_length - 1] = '\0';
	}
	working_directory[0] = '\0';
	if (drive[0]) {
		std::strcat(working_directory, drive);
	}
	std::strcat(working_directory, path);
	return normalize_directory_path(working_directory);
}

bool directory_contains_local_game_data(const std::string& directory)
{
	SDL_PathInfo info;
	if (!WWFS_GetPathInfo(directory.c_str(), &info) || info.type != SDL_PATHTYPE_DIRECTORY) {
		return false;
	}

	const std::string redalert_mix = directory + "/REDALERT.MIX";
	if (!WWFS_GetPathInfo(redalert_mix.c_str(), nullptr)) {
		return false;
	}

	for (size_t index = 0; index < ARRAY_SIZE(kStartupGameDataBaseMixfiles); ++index) {
		const std::string mixfile = directory + "/" + kStartupGameDataBaseMixfiles[index];
		if (!WWFS_GetPathInfo(mixfile.c_str(), nullptr)) {
			return false;
		}
	}

	return true;
}

bool directory_contains_game_data(const std::string& directory)
{
	if (directory_contains_local_game_data(directory)) {
		return true;
	}

#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
	const std::string emscripten_asset_root = normalize_directory_path(WWFS_GetBaseDirectoryPath().c_str());
	if (SDL_strcasecmp(directory.c_str(), emscripten_asset_root.c_str()) == 0) {
		return true;
	}
#endif

	return false;
}

bool try_get_command_line_gamedata_directory(int argc, char* argv[], std::string* directory)
{
	if (directory == NULL) {
		return false;
	}

	for (int index = 1; index < argc; ++index) {
		const char* argument = argv[index];
		if (argument == NULL || SDL_strcasecmp(argument, "-gamedata") != 0) {
			continue;
		}
		if (index + 1 >= argc || argv[index + 1] == NULL || argv[index + 1][0] == '\0') {
			return false;
		}

		*directory = normalize_directory_path(argv[index + 1]);
		return true;
	}

	return false;
}

std::string resolve_game_data_directory(int argc, char* argv[], const char* executable_path)
{
	std::string command_line_directory;
	if (try_get_command_line_gamedata_directory(argc, argv, &command_line_directory)) {
		startup_trace("Checking -gamedata directory \"%s\"", command_line_directory.c_str());
		if (directory_contains_game_data(command_line_directory)) {
			return command_line_directory;
		}
	}

	const std::string current_working_directory = current_working_directory_path();
	if (!current_working_directory.empty()) {
		startup_trace("Checking current working directory \"%s\"", current_working_directory.c_str());
		if (directory_contains_game_data(current_working_directory)) {
			return current_working_directory;
		}
	}

	const std::string executable_directory = executable_directory_path(executable_path);
	startup_trace("Checking executable directory \"%s\"", executable_directory.c_str());
	if (directory_contains_game_data(executable_directory)) {
		return executable_directory;
	}

	return std::string();
}

void print_missing_game_data_error(int argc, char* argv[], const char* executable_path)
{
	std::string command_line_directory;
	const bool has_command_line_directory = try_get_command_line_gamedata_directory(argc, argv, &command_line_directory);
	const std::string current_working_directory = current_working_directory_path();
	const std::string executable_directory = executable_directory_path(executable_path);

	fprintf(stderr, "Required game data was not found.\n");
	fprintf(stderr, "Checked for REDALERT.MIX and MAIN1.MIX..MAIN4.MIX in this order:\n");
	if (has_command_line_directory) {
		fprintf(stderr, "  1. -gamedata %s\n", command_line_directory.c_str());
	} else {
		fprintf(stderr, "  1. -gamedata <dir> (not provided)\n");
	}
	fprintf(stderr,
		"  2. current working directory: %s\n",
		current_working_directory.empty() ? "(unavailable)" : current_working_directory.c_str());
	fprintf(stderr,
		"  3. executable directory: %s\n",
		executable_directory.empty() ? "(unavailable)" : executable_directory.c_str());
	fprintf(stderr, "Use -gamedata <dir> or launch the game from a directory that contains those files.\n");
}

void destroy_main_window_for_shutdown(int ready_to_quit, const char* reason)
{
	ReadyToQuit = ready_to_quit;
	startup_trace("Destroying main window for %s shutdown", reason);
	Main_Window_Destroy();
}
}

//WinTimerClass * WinTimer;
extern void Main_Window_Create(void);
void Check_Use_Compressed_Shapes (void);
void Read_Setup_Options(RawFileClass *config_file);

const char* Game_Config_Section();

#if (ENGLISH)
#define WINDOW_NAME		"Red Alert"
#endif

#if (FRENCH)
#define WINDOW_NAME		"Alerte Rouge"
#endif

#if (GERMAN)
#define WINDOW_NAME		"Alarmstufe Rot"
#endif

/***********************************************************************************************
 * RedAlert_Main -- Initial startup routine (preps library systems).                           *
 *                                                                                             *
 *    This is the routine that is first called when the program starts up. It basically        *
 *    handles the command line parsing and setting up library systems.                         *
 *                                                                                             *
 * INPUT:   argc  -- Number of command line arguments.                                         *
 *                                                                                             *
 *          argv  -- Pointer to array of command line argument strings.                        *
 *                                                                                             *
 * OUTPUT:  Returns with execution failure code (if any).                                      *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
int RedAlert_Main(int argc, char* argv[])

{
	char path_to_exe[132];
	char* fallback_argv[1];

	if (argv == NULL || argc <= 0 || argv[0] == NULL || argv[0][0] == '\0') {
		copy_base_path(&path_to_exe[0], sizeof(path_to_exe));
		fallback_argv[0] = &path_to_exe[0];
		argv = fallback_argv;
		argc = 1;
	}

	startup_trace("RedAlert_Main entered argc=%d argv0=\"%s\"", argc, argv[0] ? argv[0] : "");

	if (Ram_Free(MEM_NORMAL) < 7000000) {
		printf (TEXT_NO_RAM);

#if (0)

		/*
		** Take a stab at finding out how much memory there is available.
		*/

		for ( int mem = 13*1024*1024 ; mem >0 ; mem -=1024 ) {
			temp_mem = malloc (mem);
			if (temp_mem){
				free (temp_mem);
				printf ("Memory available: %d", mem);
				break;
			}
		}
#endif	//(0)
		return(EXIT_FAILURE);
	}

	if (command_line_contains_forbidden_network_path(argc, argv)) {
		RA_ShowMessageBox("Playing off of the network is not allowed.", "Red Alert", SDL_MESSAGEBOX_ERROR);
		return(EXIT_FAILURE);
	}

	/*
	**	Select the asset directory in this exact order:
	**	1) -gamedata <dir>
	**	2) current working directory
	**	3) executable directory
	*/
	const std::string game_data_directory = resolve_game_data_directory(argc, argv, argv[0]);
	if (game_data_directory.empty()) {
		startup_trace("No GameData directory found");
		print_missing_game_data_error(argc, argv, argv[0]);
		return(EXIT_FAILURE);
	}
	startup_trace("Using asset root \"%s\"", game_data_directory.c_str());
	if (!WWFS_SetCurrentDirectory(game_data_directory.c_str())) {
		fprintf(stderr, "Failed to use game data directory: %s\n", game_data_directory.c_str());
		return(EXIT_FAILURE);
	}
	startup_trace("WWFS_SetCurrentDirectory complete");

#ifdef WOLAPI_INTEGRATION
	//	Look for special wolapi install program, used after the patch to version 3, to install "Shared Internet Components".
	SDL_PathInfo wfd_info;
	bool bWOLSetupFile = WWFS_GetPathInfo("wolsetup.exe", &wfd_info);
//	if( bWOLSetupFile )
//		debugprint( "Found wolsetup.exe\n" );
	//	Look for the setup-complete config flag that wolsetup.exe leaves behind.
	uint32_t dwValue;
	if( RA_ReadConfigUint32( Game_Config_Section(), "WolapiInstallComplete", &dwValue ) )
	{
//		debugprint( "Found WolapiInstallComplete in config\n" );
		//	Setup has finished. Delete the setup exe and remove reg key.
		if( bWOLSetupFile )
		{
			if( WWFS_RemovePath("wolsetup.exe") )
				RA_DeleteConfigValue( Game_Config_Section(), "WolapiInstallComplete" );
		}
		else
			RA_DeleteConfigValue( Game_Config_Section(), "WolapiInstallComplete" );
	}

	//	I've been having problems getting the patch to delete "conquer.eng", which is present in the game
	//	directory for 1.08, but which must NOT be present for this version (Aftermath mix files provide the
	//	string overrides that the 1.08 separate conquer.eng did before Aftermath).
	//	Delete conquer.eng if it's found.
	if( WWFS_GetPathInfo( "conquer.eng", &wfd_info ) )
		WWFS_RemovePath( "conquer.eng" );

#endif

	startup_trace("Calling Parse_Command_Line");
	if (Parse_Command_Line(argc, argv)) {
		startup_trace("Parse_Command_Line complete");

		WindowsTimer = new WinTimerClass(60, FALSE);
		startup_trace("WinTimerClass created");

		uint32_t time_test = WindowsTimer->Get_System_Tick_Count();
		SDL_GameInput_Delay(1000);
		startup_trace("Initial timer delay complete");
		if (WindowsTimer->Get_System_Tick_Count() == time_test){
			RA_ShowMessageBox(TEXT_ERROR_TIMER, TEXT_SHORT_TITLE, SDL_MESSAGEBOX_ERROR);
			return(EXIT_FAILURE);
		}
		RawFileClass cfile(CONFIG_FILE_NAME);
		const bool config_available = cfile.Is_Available();
		startup_trace("Config availability checked available=%d", config_available ? 1 : 0);


		Keyboard = new KeyboardClass();
		SDL_GameInput_Set_Main_Thread();
		startup_trace("Keyboard created and main thread registered");

		/*
		** If there is loads of memory then use uncompressed shapes
		*/
		Check_Use_Compressed_Shapes();
		startup_trace("Check_Use_Compressed_Shapes complete");

		/*
		** If there is not enough disk space free, don't allow the product to run.
		*/
		if (Disk_Space_Available() < INIT_FREE_DISK_SPACE) {

			char	disk_space_message [512];
#if (GERMAN | FRENCH)
			snprintf(disk_space_message, sizeof(disk_space_message), TEXT_CRITICALLY_LOW, (INIT_FREE_DISK_SPACE) / (1024 * 1024));
#else
			snprintf(disk_space_message, sizeof(disk_space_message), "%s", TEXT_CRITICALLY_LOW);
#endif
			if (!RA_ShowConfirmationMessageBox(disk_space_message, TEXT_SHORT_TITLE)) {
				if ( WindowsTimer )
					delete WindowsTimer;
				return (EXIT_FAILURE);
			}
		}

		if (config_available) {
			startup_trace("Reading private config struct");
			Read_Private_Config_Struct(cfile, &NewConfig);

			/*
			** Set the options as requested by the ccsetup program
			*/
			startup_trace("Reading setup options");
			Read_Setup_Options( &cfile );
			startup_trace("Read_Setup_Options complete");
		}

		startup_trace("Calling Main_Window_Create");
		Main_Window_Create();
		startup_trace("Main_Window_Create complete hwnd=%p size=%dx%d", MainWindow, ScreenWidth, ScreenHeight);
		if (!Audio_Init(MainWindow, 16, false, 11025 * 2, 0)) {
			startup_trace("Audio_Init failed");
			if (MainWindow != NULL) {
				RA_DestroyWindow(MainWindow);
				MainWindow = NULL;
			}
			return (EXIT_FAILURE);
		}
		startup_trace("Audio_Init complete");

		int32_t video_success = FALSE;
		/*
		** Set 640x400 video mode. If its not available then try for 640x480
		*/
		if (ScreenHeight == 400) {
			if (Set_Video_Mode (MainWindow, ScreenWidth, ScreenHeight, 8)) {
				video_success = TRUE;
			} else {
				if (Set_Video_Mode (MainWindow, ScreenWidth, 480, 8)) {
					video_success = TRUE;
					ScreenHeight = 480;
				}
			}
		} else {
			if (Set_Video_Mode (MainWindow, ScreenWidth, ScreenHeight, 8)) {
				video_success = TRUE;
			}
		}

		if (!video_success) {
			RA_ShowMessageBox(TEXT_VIDEO_ERROR, TEXT_SHORT_TITLE, SDL_MESSAGEBOX_WARNING);
			if (WindowsTimer) delete WindowsTimer;
			//if (Palette) delete Palette;
			return (EXIT_FAILURE);
		}
		startup_trace("Set_Video_Mode complete screen_height=%d", ScreenHeight);

		if (ScreenWidth==320) {
			VisiblePage.Init( ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)0);
		} else {
			VisiblePage.Init( ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)(GBC_VISIBLE | GBC_VIDEOMEM));
			HiddenPage.Init (ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)GBC_VIDEOMEM);
			VisiblePage.Attach_Surface(&HiddenPage);
		}

		ScreenHeight = 400;

		if (VisiblePage.Get_Height() == 480) {
			SeenBuff.Attach(&VisiblePage, 0, 40, 640, 400);
			HidPage.Attach(&HiddenPage, 0, 40, 640, 400);
		} else {
			SeenBuff.Attach(&VisiblePage, 0, 0, 640, 400);
			HidPage.Attach(&HiddenPage, 0, 0, 640, 400);
		}

		Options.Adjust_Variables_For_Resolution();

		/*
		** Install the memory error handler
		*/
		Memory_Error = &Memory_Error_Handler;

		WindowList[0][WINDOWWIDTH] 	= SeenBuff.Get_Width();
		WindowList[0][WINDOWHEIGHT]	= SeenBuff.Get_Height();
		WindowList[WINDOW_EDITOR][WINDOWWIDTH] = SeenBuff.Get_Width();
		WindowList[WINDOW_EDITOR][WINDOWHEIGHT]= SeenBuff.Get_Height();

		WWMouse = new WWMouseClass(&SeenBuff, 48, 48);
		MouseInstalled = TRUE;

		/*
		** See if we should run the intro
		*/
		INIClass ini;
		if (config_available) {
			ini.Load(cfile);
		}

		/*
		**	Check for forced intro movie run disabling. If the conquer
		**	configuration file says "no", then don't run the intro.
		*/
		if (config_available && !Special.IsFromInstall) {
			Special.IsFromInstall = ini.Get_Bool("Intro", "PlayIntro", true);
		}
		if (config_available) {
			SlowPalette = ini.Get_Bool("Options", "SlowPalette", false);
		}

		/*
		** Regardless of whether we should run it or not, here we're
		** gonna change it to say "no" in the future.
		*/
		if (Special.IsFromInstall) {
			BreakoutAllowed = true;
//			BreakoutAllowed = false;
			if (config_available) {
				ini.Put_Bool("Intro", "PlayIntro", false);
				ini.Save(cfile);
			}
		}

		/*
		**	If the intro is being run for the first time, then don't
		**	allow breaking out of it with the <ESC> key.
		*/
		if (Special.IsFromInstall) {
			BreakoutAllowed = true;
//			BreakoutAllowed = false;
		}

		Memory_Error_Exit = Print_Error_End_Exit;

		startup_trace("Entering Main_Game argc=%d", argc);
		Main_Game(argc, argv);
		startup_trace("Main_Game returned ReadyToQuit=%d", ReadyToQuit);

		VisiblePage.Clear();
		HiddenPage.Clear();
		Memory_Error_Exit = Print_Error_Exit;
		destroy_main_window_for_shutdown(1, "clean");

		return (EXIT_SUCCESS);

	}
	/*
	**	Restore the current drive and directory.
	*/
	return(EXIT_SUCCESS);
}

/***********************************************************************************************
 * Prog_End -- Cleans up library systems in prep for game exit.                                *
 *                                                                                             *
 *    This routine should be called before the game terminates. It handles cleaning up         *
 *    library systems so that a graceful return to the host operating system is achieved.      *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void  Prog_End(void)
{
	Sound_End();
	if (WWMouse) {
		delete WWMouse;
		WWMouse = NULL;
	}
	if (WindowsTimer) {
		 delete WindowsTimer;
		 WindowsTimer = NULL;
	}
}

void Print_Error_End_Exit(char * string)
{
	Prog_End();
	printf( "%s\n", string );
	exit(1);
}

void Print_Error_Exit(char * string)
{
	printf( "%s\n", string );
	exit(1);
}

/***********************************************************************************************
 * Emergency_Exit -- Function to call when we want to exit unexpectedly.                       *
 *                   Use this function instead of exit(n) so everything is properly cleaned up.*
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Code to return to the OS                                                          *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    3/13/97 1:32AM ST : Created                                                              *
 *=============================================================================================*/
void Emergency_Exit ( int code )
{

	/*
	** Clear out the video buffers so we dont glitch when we lose focus
	*/
	VisiblePage.Clear();
	HiddenPage.Clear();
	BlackPalette.Set();
	Memory_Error_Exit = Print_Error_Exit;
	destroy_main_window_for_shutdown(3, "emergency");
	exit ( code );
}

/***********************************************************************************************
 * Read_Setup_Options -- Read stuff in from the INI file that we need to know sooner           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Ptr to config file class                                                          *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    6/7/96 4:09PM ST : Created                                                               *
 *   09/30/1996 JLB : Uses INI class.                                                          *
 *=============================================================================================*/
void Read_Setup_Options( RawFileClass *config_file )
{
	if (config_file->Is_Available()) {

		INIClass ini;

		ini.Load(*config_file);

		/*
		** Read in the boolean options
		*/
		ScreenHeight = ini.Get_Bool("Options", "Resolution", false) ? 480 : 400;

		/*
		** See if an alternative socket number has been specified
		*/
		int socket = ini.Get_Int("Options", "Socket", 0);
		if (socket >0 ) {
			socket += 0x4000;
			if (socket >= 0x4000 && socket < 0x8000) {
				Udp.Set_Socket (socket);
			}
		}

		/*
		** See if a destination network has been specified
		*/
		char netbuf [512];
		memset(netbuf, 0, sizeof(netbuf));
		char * netptr = netbuf;
		bool found = ini.Get_String("Options", "DestNet", NULL, netbuf, sizeof(netbuf));

		if (found && netptr != NULL && strlen(netbuf)) {
			NetNumType net;
			NetNodeType node;

			/*
			** Scan the string, pulling off each address piece
			*/
			int i = 0;
			char * p = strtok(netbuf,".");
			int x;
			while (p != NULL) {
				sscanf(p,"%x",&x);			// convert from hex string to int
				if (i < 4) {
					net[i] = (char)x;			// fill NetNum
				} else {
					node[i-4] = (char)x;		// fill NetNode
				}
				i++;
				p = strtok(NULL,".");
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
		}
	}
}
