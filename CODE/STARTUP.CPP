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
#include "ra_messagebox.h"
#include	<cstdarg>
#include	<cstdio>
#include	<cstdlib>

#include <SDL3/SDL_filesystem.h>

#ifdef WOLAPI_INTEGRATION
//#include "WOLDEBUG.H"
#endif

bool Read_Private_Config_Struct(FileClass & file, NewConfigType * config);
void Print_Error_End_Exit(char * string);
void Print_Error_Exit(char * string);

namespace
{
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

	va_list arguments;
	va_start(arguments, format);
	std::fputs("[startup] ", stderr);
	std::vfprintf(stderr, format, arguments);
	std::fputc('\n', stderr);
	std::fflush(stderr);
	va_end(arguments);
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

	const char* base_path = SDL_GetBasePath();
	std::snprintf(buffer, buffer_size, "%s", (base_path != NULL) ? base_path : "./");
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
	**	Change directory to the where the executable is located. Handle the
	**	case where there is no path attached to argv[0].
	*/
	char drive[_MAX_DRIVE];
	char path[260];
	char working_directory[260];
	WWFS_SplitPath(argv[0], drive, path, NULL, NULL);
	if (!path[0]) {
		strcpy(path, ".");
	}
	if (path[strlen(path)-1] == '\\') {
		path[strlen(path)-1] = '\0';
	}
	working_directory[0] = '\0';
	if (drive[0]) {
		strcat(working_directory, drive);
	}
	strcat(working_directory, path);
	WWFS_SetCurrentDirectory(working_directory);

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

	if (Parse_Command_Line(argc, argv)) {

		WindowsTimer = new WinTimerClass(60, FALSE);

		uint32_t time_test = WindowsTimer->Get_System_Tick_Count();
		SDL_GameInput_Delay(1000);
		if (WindowsTimer->Get_System_Tick_Count() == time_test){
			RA_ShowMessageBox(TEXT_ERROR_TIMER, TEXT_SHORT_TITLE, SDL_MESSAGEBOX_ERROR);
			return(EXIT_FAILURE);
		}
		RawFileClass cfile(CONFIG_FILE_NAME);


		Keyboard = new KeyboardClass();
		SDL_GameInput_Set_Main_Thread();

		/*
		** If there is loads of memory then use uncompressed shapes
		*/
		Check_Use_Compressed_Shapes();

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

		if (cfile.Is_Available()) {

			Read_Private_Config_Struct(cfile, &NewConfig);

			/*
			** Set the options as requested by the ccsetup program
			*/
			Read_Setup_Options( &cfile );

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

			CDFileClass::Set_CD_Drive (CDList.Get_First_CD_Drive());

			/*
			** See if we should run the intro
			*/
			INIClass ini;
			ini.Load(cfile);

			/*
			**	Check for forced intro movie run disabling. If the conquer
			**	configuration file says "no", then don't run the intro.
			*/
			if (!Special.IsFromInstall) {
				Special.IsFromInstall = ini.Get_Bool("Intro", "PlayIntro", true);
			}
			SlowPalette = ini.Get_Bool("Options", "SlowPalette", false);

			/*
			** Regardless of whether we should run it or not, here we're
			** gonna change it to say "no" in the future.
			*/
			if (Special.IsFromInstall) {
				BreakoutAllowed = true;
//				BreakoutAllowed = false;
				ini.Put_Bool("Intro", "PlayIntro", false);
				ini.Save(cfile);
			}

			/*
			**	If the intro is being run for the first time, then don't
			**	allow breaking out of it with the <ESC> key.
			*/
			if (Special.IsFromInstall) {
				BreakoutAllowed = true;
//				BreakoutAllowed = false;
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

		} else {
			puts(TEXT_SETUP_FIRST);
			Keyboard->Get();
		}

		if (WindowsTimer) {
		 	delete WindowsTimer;
		 	WindowsTimer = NULL;
		}

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
