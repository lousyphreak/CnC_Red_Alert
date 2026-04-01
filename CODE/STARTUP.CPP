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
 *   main -- Initial startup routine (preps library systems).                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include	"SDLINPUT.H"
#include	<cstdarg>
#include	<cstdio>
#include	<cstdlib>
#include <fcntl.h>

#include	"IPX95.H"

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
}

//WinTimerClass * WinTimer;
extern void Create_Main_Window ( HANDLE instance , int command_show , int width , int height);
HINSTANCE	ProgramInstance;
void Check_Use_Compressed_Shapes (void);
void Read_Setup_Options(RawFileClass *config_file);
bool VideoBackBufferAllowed = true;

const char* Game_Registry_Key();

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
 * main -- Initial startup routine (preps library systems).                                    *
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
//int PASCAL WinMain(HINSTANCE, HINSTANCE, char *, int )
int PASCAL WinMain ( HINSTANCE instance , HINSTANCE , char * command_line , int command_show )

{

	bool surface_in_system_memory = false;

	startup_trace("WinMain entered command_line=\"%s\"", command_line ? command_line : "");

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


	if (strstr(command_line, "f:\\projects\\c&c0") != NULL ||
			strstr(command_line, "F:\\PROJECTS\\C&C0") != NULL) {
		MessageBox(0, "Playing off of the network is not allowed.", "Red Alert", MB_OK|MB_ICONSTOP);
		return(EXIT_FAILURE);
	}

	int		argc;				//Command line argument count
	unsigned	command_scan;
	char		command_char;
	char *	argv[20];		//Pointers to command line arguments
	char		path_to_exe[132];

	ProgramInstance = instance;

	/*
	** Get the full path to the .EXE
	*/
	GetModuleFileName (instance, &path_to_exe[0], 132);

	/*
	** First argument is supposed to be a pointer to the .EXE that is running
	**
	*/
	argc=1;							//Set argument count to 1
	argv[0]=&path_to_exe[0];	//Set 1st command line argument to point to full path

	/*
	** Get pointers to command line arguments just like if we were in DOS
	**
	** The command line we get is cr/zero? terminated.
	**
	*/

	command_scan=0;

	do {
		/*
		** Scan for non-space character on command line
		*/
		do {
			command_char = *( command_line+command_scan++ );
		} while ( command_char==' ' );

		if ( command_char!=0 && command_char != 13 ) {
			argv[argc++]=command_line+command_scan-1;

			/*
			** Scan for space character on command line
			*/
			do {
				command_char = *( command_line+command_scan++ );
			} while ( command_char!=' ' && command_char != 0 && command_char!=13 );
		*( command_line+command_scan-1 ) = 0;
		}

	} while ( command_char != 0 && command_char != 13 && argc<20 );


	/*
	**	Change directory to the where the executable is located. Handle the
	**	case where there is no path attached to argv[0].
	*/
	char drive[_MAX_DRIVE];
	char path[_MAX_PATH];
	char working_directory[_MAX_PATH];
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
	//	Look for special registry entry that tells us when the setup exe has done its thing.
	HKEY hKey;
	RegOpenKeyEx( HKEY_LOCAL_MACHINE, Game_Registry_Key(), 0, KEY_READ, &hKey );
	DWORD dwValue;
	DWORD dwBufSize = sizeof( DWORD );
	if( RegQueryValueEx( hKey, "WolapiInstallComplete", 0, NULL, (LPBYTE)&dwValue, &dwBufSize ) == ERROR_SUCCESS )
	{
//		debugprint( "Found WolapiInstallComplete in registry\n" );
		//	Setup has finished. Delete the setup exe and remove reg key.
		if( bWOLSetupFile )
		{
			if( WWFS_RemovePath("wolsetup.exe") )
				RegDeleteValue( hKey, "WolapiInstallComplete" );
		}
		else
			RegDeleteValue( hKey, "WolapiInstallComplete" );
	}
	RegCloseKey( hKey );

	//	I've been having problems getting the patch to delete "conquer.eng", which is present in the game
	//	directory for 1.08, but which must NOT be present for this version (Aftermath mix files provide the
	//	string overrides that the 1.08 separate conquer.eng did before Aftermath).
	//	Delete conquer.eng if it's found.
	if( WWFS_GetPathInfo( "conquer.eng", &wfd_info ) )
		WWFS_RemovePath( "conquer.eng" );

#endif

	if (Parse_Command_Line(argc, argv)) {

#if(TEN)
	//
	// Only allow the TEN version of the game to run if the TEN
	// or AllowSoloPlayOptions arguments are specified.
	//
	if (Session.AllowSolo==0 && Session.Type != GAME_TEN) {
		MessageBox(NULL, "Red Alert for TEN\n (c) 1996 Westwood Studios",
			"Red Alert", MB_OK);
		exit(0);
	}
#endif	// TEN

#if(MPATH)
	//
	// Only allow the MPATH version of the game to run if the MPATH
	// or AllowSoloPlayOptions arguments are specified.
	//
	if (Session.AllowSolo==0 && Session.Type != GAME_MPATH) {
		MessageBox(NULL, "Red Alert for MPATH\n (c) 1996 Westwood Studios",
			"Red Alert", MB_OK);
		exit(0);
	}
#endif	// MPATH

		WindowsTimer = new WinTimerClass(60, FALSE);

		int time_test = WindowsTimer->Get_System_Tick_Count();
		Sleep (1000);
		if (WindowsTimer->Get_System_Tick_Count() == time_test){
			MessageBox(0, TEXT_ERROR_TIMER, TEXT_SHORT_TITLE, MB_OK|MB_ICONSTOP);
			return(EXIT_FAILURE);
		}
		RawFileClass cfile(CONFIG_FILE_NAME);


#ifdef NEVER
		Keyboard->IsLibrary = true;
		if (Debug_Flag) {
			Keyboard_Attributes_On(DEBUGINT);
		}
#endif

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
			sprintf (disk_space_message, TEXT_CRITICALLY_LOW, (INIT_FREE_DISK_SPACE) / (1024 * 1024));
			int reply = MessageBox(NULL, disk_space_message, TEXT_SHORT_TITLE, MB_ICONQUESTION|MB_YESNO);
			if (reply == IDNO) {
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

			Create_Main_Window( instance , command_show , ScreenWidth , ScreenHeight );
			startup_trace("Create_Main_Window complete hwnd=%p size=%dx%d", MainWindow, ScreenWidth, ScreenHeight);
			SoundOn = Audio_Init ( MainWindow , 16 , false , 11025*2 , 0 );
			startup_trace("Audio_Init complete SoundOn=%d", SoundOn);


			BOOL video_success = FALSE;
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
				MessageBox(MainWindow, TEXT_VIDEO_ERROR, TEXT_SHORT_TITLE, MB_ICONEXCLAMATION|MB_OK);
				if (WindowsTimer) delete WindowsTimer;
				//if (Palette) delete Palette;
				return (EXIT_FAILURE);
			}
			startup_trace("Set_Video_Mode complete screen_height=%d", ScreenHeight);

			if (ScreenWidth==320) {
				VisiblePage.Init( ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)0);
				ModeXBuff.Init( ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)(GBC_VISIBLE | GBC_VIDEOMEM));
			} else {
				VisiblePage.Init( ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)(GBC_VISIBLE | GBC_VIDEOMEM));

				/*
				** Check that we really got a video memory page. Failure is fatal.
				*/
				surface_in_system_memory = VisiblePage.Get_Video_Surface()->IsSystemMemory();
				if (surface_in_system_memory) {
					/*
					** Aaaarrgghh!
					*/
					WWDebugString(TEXT_DDRAW_ERROR);WWDebugString("\n");
					MessageBox(MainWindow, TEXT_DDRAW_ERROR, TEXT_SHORT_TITLE, MB_ICONEXCLAMATION|MB_OK);
					if (WindowsTimer) delete WindowsTimer;
					return (EXIT_FAILURE);
				}

				/*
				** If we have enough left then put the hidpage in video memory unless...
				**
				** If there is no blitter then we will get better performance with a system
				** memory hidpage
				**
				** Use a system memory page if the user has specified it via the ccsetup program.
				*/
				unsigned video_memory = Get_Free_Video_Memory();
				unsigned video_capabilities = Get_Video_Hardware_Capabilities();
				if (video_memory < ScreenWidth*ScreenHeight ||
						(! (video_capabilities & VIDEO_BLITTER)) ||
						(video_capabilities & VIDEO_NO_HARDWARE_ASSIST) ||
						!VideoBackBufferAllowed) {
					HiddenPage.Init (ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)0);
				} else {
					//HiddenPage.Init (ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)0);
					HiddenPage.Init (ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)GBC_VIDEOMEM);

					/*
					** Make sure we really got a video memory hid page. If we didnt then things
					** will run very slowly.
					*/
					surface_in_system_memory = HiddenPage.Get_Video_Surface()->IsSystemMemory();
					if (surface_in_system_memory) {

						/*
						** Oh dear, big trub. This must be an IBM crAptiva or something similarly cruddy.
						** We must redo the Hidden Page as system memory.
						*/
						AllSurfaces.Remove_Surface(HiddenPage.Get_Video_Surface());  // Remove the old surface from the AllSurfaces list
						HiddenPage.Get_Video_Surface()->Release();
						HiddenPage.Init (ScreenWidth , ScreenHeight , NULL , 0 , (GBC_Enum)0);
					} else {
						VisiblePage.Attach_Surface(&HiddenPage);
					}
				}
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
			** Don't do this for TEN & MPath.
			*/
			if (!Special.IsFromInstall && Session.Type != GAME_TEN &&
				Session.Type != GAME_MPATH) {
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

			/*
			** Flag that this is a clean shutdown (not killed with Ctrl-Alt-Del)
			*/
			ReadyToQuit = 1;

			/*
			** Post a message to our message handler to tell it to clean up.
			*/
			startup_trace("Posting WM_DESTROY for clean shutdown");
			PostMessage(MainWindow, WM_DESTROY, 0, 0);

			/*
			** Wait until the message handler has dealt with the message
			*/
			do
			{
				Keyboard->Check();
			}while (ReadyToQuit == 1);

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
void __cdecl Prog_End(void)
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

	/*
	** Flag that this is an emergency shut down - not a clean shutdown but
	** not killed with Ctrl-Alt-Del either.
	*/
	ReadyToQuit = 3;

			/*
			** Post a message to our message handler to tell it to clean up.
			*/
			startup_trace("Posting WM_DESTROY for clean shutdown");
			PostMessage(MainWindow, WM_DESTROY, 0, 0);

	/*
	** Wait until the message handler has dealt with the message
	*/
	do
	{
		Keyboard->Check();
	}while (ReadyToQuit == 3);



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
		VideoBackBufferAllowed = ini.Get_Bool("Options", "VideoBackBuffer", true);
		AllowHardwareBlitFills = ini.Get_Bool("Options", "HardwareFills", true);
		ScreenHeight = ini.Get_Bool("Options", "Resolution", false) ? 480 : 400;

		/*
		** See if an alternative socket number has been specified
		*/
		int socket = ini.Get_Int("Options", "Socket", 0);
		if (socket >0 ) {
			socket += 0x4000;
			if (socket >= 0x4000 && socket < 0x8000) {
				Ipx.Set_Socket (socket);
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
				Session.BridgeNet = IPXAddressClass(net, node);
			}
		}
	}
}

void Get_OS_Version (void)
{

	WindowsNT = ((GetVersion() & 0x80000000) == 0) ? true : false;

#if (0)
	OSVERSIONINFO	osversion;
	if ( GetVersionEx (&osversion) ){
		WindowsNT = (osversion.dwPlatformId == VER_PLATFORM_WIN32_NT) ? true : false;
		OutputDebugString ("RA95 - Got OS version\n");
	}else{
		OutputDebugString ("RA95 - Failed to get OS version\n");
		char fuck [128];
		sprintf (fuck,"RA95 - Error code is %d\n", GetLastError());
		OutputDebugString (fuck);

	}
#endif	//(0)

}
