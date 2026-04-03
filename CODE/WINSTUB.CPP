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

/* $Header: /CounterStrike/WINSTUB.CPP 3     3/13/97 2:06p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : WINSTUB.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Steve Tall                                                   *
 *                                                                                             *
 *                   Start Date : 10/04/95                                                     *
 *                                                                                             *
 *                  Last Update : October 4th 1995 [ST]                                        *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Overview:                                                                                   *
 *   This file contains stubs for undefined externals when linked under Watcom for Win 95      *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 *                                                                                             *
 * Functions:                                                                                  *
 *   Assert_Failure -- display the line and source file where a failed assert occurred         *
 *   Check_For_Focus_Loss -- check for the end of the focus loss                               *
 *   Create_Main_Window -- opens the MainWindow for C&C                                        *
 *   Focus_Loss -- this function is called when a library function detects focus loss          *
 *   Memory_Error_Handler -- Handle a possibly fatal failure to allocate memory                *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"FUNCTION.H"
#include "SDLINPUT.H"
#include <SDL3/SDL_video.h>
#include	<cstdarg>
#include	<cstdio>
#include	<cstdlib>

#ifdef WINSOCK_IPX
#include "WSPROTO.H"
#else	//WINSOCK_IPX
#include "TCPIP.H"
#include "IPX95.H"
#endif	//WINSOCK_IPX

void output(int16_t,int16_t)
{}


extern	void VQA_PauseAudio(void);
extern	void VQA_ResumeAudio(void);
extern int32_t BootstrapFocusSeen;

//#include "WOLDEBUG.H"

namespace
{
bool winstub_trace_enabled()
{
	static const bool enabled = std::getenv("RA_TRACE_STARTUP") != NULL;
	return enabled;
}

void winstub_trace(const char* format, ...)
{
	if (!winstub_trace_enabled()) {
		return;
	}

	va_list arguments;
	va_start(arguments, format);
	std::fputs("[winstub] ", stderr);
	std::vfprintf(stderr, format, arguments);
	std::fputc('\n', stderr);
	std::fflush(stderr);
	va_end(arguments);
}

void apply_window_show_command(RAWindow* window, int command_show)
{
	if (window == NULL || window->sdl_window == NULL) {
		return;
	}

	switch (command_show) {
		case SW_HIDE:
			SDL_HideWindow(window->sdl_window);
			return;

		case SW_MINIMIZE:
		case SW_SHOWMINIMIZED:
			SDL_ShowWindow(window->sdl_window);
			SDL_MinimizeWindow(window->sdl_window);
			return;

		case SW_SHOWMAXIMIZED:
			SDL_ShowWindow(window->sdl_window);
			SDL_MaximizeWindow(window->sdl_window);
			break;

		case SW_RESTORE:
			SDL_ShowWindow(window->sdl_window);
			SDL_RestoreWindow(window->sdl_window);
			break;

		default:
			SDL_ShowWindow(window->sdl_window);
			break;
	}

	SDL_RaiseWindow(window->sdl_window);
}

void restore_focus_after_loss(void)
{
	Start_Primary_Sound_Buffer(TRUE);
	if (!InMovie) {
		Theme.Stop();
		Theme.Queue_Song(THEME_PICK_ANOTHER);
	}
}
}


/***********************************************************************************************
 * Focus_Loss -- this function is called when a library function detects focus loss            *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    2/1/96 2:10PM ST : Created                                                               *
 *=============================================================================================*/

void Focus_Loss(void)
{
	Theme.Suspend();
	Stop_Primary_Sound_Buffer();
	if (WWMouse) WWMouse->Clear_Cursor_Clip();
}

void Focus_Restore(void)
{
	Restore_Cached_Icons();
	Map.Flag_To_Redraw(true);
	Start_Primary_Sound_Buffer(TRUE);
	if (WWMouse) WWMouse->Set_Cursor_Clip();
	VisiblePage.Clear();
	HiddenPage.Clear();
}



/***********************************************************************************************
 * Check_For_Focus_Loss -- check for the end of the focus loss                                 *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    2/2/96 10:49AM ST : Created                                                              *
 *=============================================================================================*/

void Check_For_Focus_Loss(void)
{
	static int32_t	focus_last_time = 1;

	if ( !GameInFocus ) {
		Focus_Loss();
		SDL_GameInput_Pump();
	}

	if (!focus_last_time && GameInFocus) {

		VQA_PauseAudio();
		CountDownTimerClass cd;
		cd.Set(60*1);

		do {
			SDL_GameInput_Pump();
			if (AllDone) {
				return;
			}
			if (cd.Time()) {
				Sleep(1);
			}

		} while (cd.Time());
		VQA_ResumeAudio();
		restore_focus_after_loss();
//		AllSurfaces.Restore_Surfaces();
//		VisiblePage.Clear();
//		HiddenPage.Clear();
//		Map.Flag_To_Redraw(true);
	}

	focus_last_time = GameInFocus;

}



extern bool InMovie;

void Main_Window_Handle_Focus_Change(bool focused)
{
	winstub_trace("Main window focus change hwnd=%p active=%u", MainWindow, focused ? 1u : 0u);
	if (focused) {
		BootstrapFocusSeen = TRUE;
		GameInFocus = TRUE;
		AllSurfaces.Set_Surface_Focus(TRUE);
		AllSurfaces.Restore_Surfaces();
	} else if (!BootstrapFocusSeen) {
		GameInFocus = FALSE;
		Focus_Loss();
		AllSurfaces.Set_Surface_Focus(FALSE);
		AllSurfaces.Restore_Surfaces();
	} else {
		winstub_trace("Ignoring post-bootstrap focus loss");
		GameInFocus = TRUE;
	}
}

void Main_Window_Destroy(void)
{
	winstub_trace("Main window destroy hwnd=%p ReadyToQuit=%d", MainWindow, ReadyToQuit);
	Prog_End();
	Invalidate_Cached_Icons();
	VisiblePage.Un_Init();
	HiddenPage.Un_Init();
	AllSurfaces.Release();
	if (!InDebugger) Reset_Video_Mode();
	Stop_Profiler();

	if (MainWindow != NULL) {
		RA_DestroyWindow(MainWindow);
		MainWindow = NULL;
	}

	AllDone = TRUE;

	switch (ReadyToQuit) {
		case 1:
			ReadyToQuit = 2;
			break;

		case 0:
			Shutdown_Network();
#ifndef WINSOCK_IPX
			if (Winsock.Get_Connected()) Winsock.Close();
			Unload_IPX_Dll();
#endif	//WINSOCK_IPX
			ExitProcess(0);
			break;

		case 3:
			Shutdown_Network();
#ifndef WINSOCK_IPX
			if (Session.Type == GAME_IPX){
				IPX_Shut_Down95();
			}
#ifdef FIXIT_CSII	//	checked - ajw 9/28/98
#else
			Unload_IPX_Dll();
#endif
			if (Winsock.Get_Connected()) Winsock.Close();
#endif	//WINSOCK_IPX
			ReadyToQuit = 2;
			break;
	}
}

void Main_Window_Handle_Close_Request(void)
{
	winstub_trace("Main window close request hwnd=%p", MainWindow);
	Main_Window_Destroy();
}

void Main_Window_Show_Maximized(void)
{
	apply_window_show_command(MainWindow, SW_SHOWMAXIMIZED);
}



void* DebugFile = INVALID_HANDLE_VALUE;

/***********************************************************************************************
 * WWDebugString -- sends a string to the debugger and echos it to disk                        *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    string                                                                            *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/28/96 12:48PM ST : Created                                                              *
 *=============================================================================================*/
void WWDebugString (char *string)
{
#if (0)
	char outstr[256];

	sprintf (outstr, "%s", string);

	uint32_t	actual;
	if (DebugFile == INVALID_HANDLE_VALUE){
		DebugFile = CreateFile("debug.txt", GENERIC_WRITE, 0,
												NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}else{
		DebugFile = CreateFile("debug.txt", GENERIC_WRITE, 0,
												NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	if (DebugFile != INVALID_HANDLE_VALUE){
		SetFilePointer (DebugFile, 0, NULL, FILE_END);
		WriteFile(DebugFile, outstr, strlen(outstr)+1, &actual, NULL);
		CloseHandle (DebugFile);
	}

	OutputDebugString (string);
#else	//(0)

	string = string;
//	debugprint( string );

#endif	//(0)

}









/***********************************************************************************************
 * Create_Main_Window -- opens the MainWindow for C&C                                          *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    instance -- handle to program instance                                            *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/10/95 4:08PM ST : Created                                                             *
 *=============================================================================================*/

#define CC_ICON 1

#if (ENGLISH)
#define WINDOW_NAME		"Red Alert"
#endif

#if (FRENCH)
#define WINDOW_NAME		"Alerte Rouge"
#endif

#if (GERMAN)
#define WINDOW_NAME		"Alarmstufe Rot"
#endif


void Create_Main_Window ( void* instance , int command_show , int width , int height )

{
	RAWindow*		hwnd ;
	hwnd = RA_CreateWindow(
		WINDOW_NAME,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		SDL_WINDOW_RESIZABLE);
// Denzil
width = width; height = height;
// End

	apply_window_show_command(hwnd, command_show);
	MainWindow=hwnd;			//Save the handle to our main window
	hInstance = instance;

	Audio_Focus_Loss_Function = &Focus_Loss;
	Misc_Focus_Loss_Function = &Focus_Loss;
	Misc_Focus_Restore_Function = &Focus_Restore;
	Gbuffer_Focus_Loss_Function = &Focus_Loss;
}


void Window_Dialog_Box(void*  hinst, LPCTSTR  lpszTemplate, RAWindow*  hwndOwner, void*  dlgprc)
{
	/*
	** Get rid of the Westwood mouse cursor because we are showing a
	** dialog box and we want to have the right windows cursor showing
	** for it.
	*/
	Hide_Mouse();
	SDL_GameInput_ShowCursor(TRUE);

	/*
	** Pop up the dialog box and then run a standard message handler
	** until the dialog box is closed.
	*/

	DialogBox(hinst, lpszTemplate, hwndOwner, dlgprc);

	/*
	** Restore the westwood mouse cursor and get rid of the windows one
	** because it is now time to restore back to the westwood way of
	** doing things.
	*/
	SDL_GameInput_ShowCursor(FALSE);
	Show_Mouse();
}



typedef struct tColourList {

	char	Red;
	char	Green;
	char	Blue;
} ColourList;

ColourList	ColourLookup[9]={
	0,0,0,
	63,0,0,
	0,63,0,
	0,0,63,
	63,0,63,
	63,63,0,
	0,63,63,
	32,32,32,
	63,63,63
};




int	DebugColour=1;


extern "C" void Set_Palette_Register(int number, int red, int green, int blue);
#pragma off (unreferenced)
void Colour_Debug (int call_number)
{
	//#if 0
	//if (DebugColour==call_number || !call_number) {

		//if (call_number) {
		//	Wait_Vert_Blank();
		//}

		Set_Palette_Register (0,ColourLookup[call_number].Red ,
										ColourLookup[call_number].Green,
										ColourLookup[call_number].Blue);
	//}
	//#endif
}

#pragma on (unreferenced)





int32_t Any_Locked (void)
{
	if (SeenBuff.Get_LockCount() ||
		HidPage.Get_LockCount()) {
		return (TRUE);
	} else {
		return(FALSE);
	}
}








//
// Miscellaneous stubs. Mainly for multi player stuff
//
//
//

//IPXAddressClass::IPXAddressClass(void) {
//	int i;
//	i++;
//}
//int IPXManagerClass::Num_Connections(void) { return (0); }
//int IPXManagerClass::Connection_ID( int ) { return (0); }
//IPXAddressClass * IPXManagerClass::Connection_Address( int ) { return ((IPXAddressClass*)0); }
//char * IPXManagerClass::Connection_Name( int ) { return ((char*)0); }
//int IPXAddressClass::Is_Broadcast() { return (0); }
//int IPXManagerClass::Send_Global_Message( void *, int, int, IPXAddressClass * ) { return (0); }
//int IPXManagerClass::Service() { return (0); }
//int IPXManagerClass::Get_Global_Message( void  *, int  *, IPXAddressClass  *, short unsigned  * ) { return (0); }
//int IPXAddressClass::operator ==( IPXAddressClass  & ) { return (0); }
//IPXManagerClass::IPXManagerClass( int, int, int, int, short unsigned, short unsigned ) {}
//IPXManagerClass::~IPXManagerClass() {
//	int i;
//	i++;
//	}
//int  IPXManagerClass::Delete_Connection( int ) { return (0); }
//IPXAddressClass::IPXAddressClass( char unsigned  *, char unsigned  * ) {}
//void  IPXManagerClass::Set_Socket( short unsigned ) {}
//int  IPXManagerClass::Is_IPX() { return (0); }
//int  IPXManagerClass::Init() { return (0); }
//void  IPXAddressClass::Get_Address( char unsigned  *, char unsigned  * ) {}
//void  IPXManagerClass::Set_Bridge( char unsigned  * ) {}
//int  IPXManagerClass::Global_Num_Send() { return (0); }
//void  IPXManagerClass::Set_Timing( long unsigned, long unsigned, long unsigned ) {}
//unsigned long IPXManagerClass::Global_Response_Time() { return (0); }
//int  IPXManagerClass::Create_Connection( int, char  *, IPXAddressClass  * ) { return (0); }
//int  IPXAddressClass::operator !=( IPXAddressClass  & ) { return (0); }
//int  IPXManagerClass::Send_Private_Message( void  *, int, int, int ) { return (0); }
//int  IPXManagerClass::Get_Private_Message( void  *, int  *, int  * ) { return (0); }
//int  IPXManagerClass::Connection_Index( int ) { return (0); }
//void  IPXManagerClass::Reset_Response_Time() {}
//long unsigned  IPXManagerClass::Response_Time() { return (0); }
//int  IPXManagerClass::Private_Num_Send( int ) { return (0); }

//_VQAHandle  *  VQA_Alloc(void) { return ((_VQAHandle *)0); }
//void  VQA_Init( _VQAHandle  *, long ( *)()) {}
//long  VQA_Open( _VQAHandle  *, char const  *, _VQAConfig  * ) { return (0); }
//void  VQA_Free( _VQAHandle  * ) {}
//void  VQA_Close( _VQAHandle  * ) {}
//long  VQA_Play( _VQAHandle  *, long ) { return (0); }

//void VQA_Init(VQAHandle *, long(*)(VQAHandle *vqa, long action,	void *buffer, long nbytes)) {}

//long VQA_Open(VQAHandle *, char const *, VQAConfig *)
//{
//	return (0);
//}

//void VQA_Close(VQAHandle *) {}

//long VQA_Play(VQAHandle *, long)
//{
//	return (0);
//}


uint8_t	*VQPalette;
int32_t				VQNumBytes;
uint32_t	VQSlowpal;
bool				VQPaletteChange = false;


extern "C"{
	void  SetPalette(uint8_t *palette, int32_t numbytes, uint32_t slowpal);
}

void Create_Palette_Interpolation_Table(void);



void Flag_To_Set_Palette(uint8_t *palette, int32_t numbytes, uint32_t slowpal)
{
	VQPalette = palette;
	VQNumBytes = numbytes;
	VQSlowpal = slowpal;
	VQPaletteChange = true;
}



void Check_VQ_Palette_Set(void)
{
	if (VQPaletteChange) {
		SetPalette(VQPalette, VQNumBytes, VQSlowpal);
		VQPaletteChange = false;
	}
}





void  SetPalette(uint8_t *palette, int32_t numbytes, uint32_t slowpal)
{
	static int palette_trace_count = 0;
	uint8_t movie_palette[256 * 3];
	bool used_precomputed_palette = false;

	if (palette == NULL) {
		return;
	}

	memcpy(movie_palette, palette, sizeof(movie_palette));

	for (int i = 0; i < 256 * 3; ++i) {
		movie_palette[i] &= 63;
	}
	Increase_Palette_Luminance(movie_palette, 15, 15, 15, 63);

	if (winstub_trace_enabled() && palette != NULL && palette_trace_count < 8) {
		int nonzero_components = 0;
		for (int i = 0; i < 256 * 3; ++i) {
			if (movie_palette[i] != 0) {
				++nonzero_components;
			}
		}
		winstub_trace("movie palette bytes=%ld slow=%lu first=%u,%u,%u second=%u,%u,%u",
			numbytes,
			slowpal,
			(unsigned)movie_palette[0],
			(unsigned)movie_palette[1],
			(unsigned)movie_palette[2],
			(unsigned)movie_palette[3],
			(unsigned)movie_palette[4],
			(unsigned)movie_palette[5]);
		winstub_trace("movie palette nonzero=%d idx55=%u,%u,%u idx163=%u,%u,%u idx240=%u,%u,%u",
			nonzero_components,
			(unsigned)movie_palette[55 * 3 + 0],
			(unsigned)movie_palette[55 * 3 + 1],
			(unsigned)movie_palette[55 * 3 + 2],
			(unsigned)movie_palette[163 * 3 + 0],
			(unsigned)movie_palette[163 * 3 + 1],
			(unsigned)movie_palette[163 * 3 + 2],
			(unsigned)movie_palette[240 * 3 + 0],
			(unsigned)movie_palette[240 * 3 + 1],
			(unsigned)movie_palette[240 * 3 + 2]);
		++palette_trace_count;
	}

	if (PalettesRead
			&& PaletteCounter < ARRAY_SIZE(InterpolatedPalettes)
			&& InterpolatedPalettes[PaletteCounter] != NULL) {
		memcpy(&PaletteInterpolationTable[0][0], InterpolatedPalettes[PaletteCounter], 65536);
		used_precomputed_palette = true;
	}

	if (PalettesRead) {
		++PaletteCounter;
	}

	Set_Palette(movie_palette);
	InterpolationPalette = CurrentPalette;

	if (!used_precomputed_palette) {
		Create_Palette_Interpolation_Table();
	} else {
		InterpolationPaletteChanged = FALSE;
	}
}


#ifndef NDEBUG
/***********************************************************************************************
 * Assert_Failure -- display the line and source file where a failed assert occurred           *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    line number in source file                                                        *
 *           name of source file                                                               *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    4/17/96 9:58AM ST : Created                                                              *
 *=============================================================================================*/

void Assert_Failure (char *expression, int line, char *file)
{
	char	assertbuf[256];
	char   timebuff[512];
	SYSTEMTIME	time;

	sprintf (assertbuf, "assert '%s' failed at line %d in module %s.\n", expression, line, file);

	if (!MonoClass::Is_Enabled()) MonoClass::Enable();

	Mono_Clear_Screen();
	Mono_Printf("%s", assertbuf);

	WWDebugString(assertbuf);

	GetLocalTime(&time);

	sprintf (timebuff, "%02d/%02d/%04d %02d:%02d:%02d - %s", time.wMonth, time.wDay, time.wYear,
																		time.wHour, time.wMinute, time.wSecond,
																		assertbuf);


	SDL_IOStream * assert_io = WWFS_OpenFile("ASSERT.TXT", "ab");
	if (assert_io) {
		SDL_WriteIO(assert_io, timebuff, strlen(timebuff));
		SDL_CloseIO(assert_io);
	}

	WWMessageBox().Process(assertbuf);
//	WWMessageBox().Process("Red Alert demo timed out - Aborting");
	//Get_Key();

	Prog_End();
	Invalidate_Cached_Icons();
	AllDone = TRUE;
	ExitProcess(0);
}
#endif







/***********************************************************************************************
 * Memory_Error_Handler -- Handle a possibly fatal failure to allocate memory                  *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    5/22/96 3:57PM ST : Created                                                              *
 *=============================================================================================*/
void Memory_Error_Handler(void)
{
	VisiblePage.Clear();
	CCPalette.Set();
	while (Get_Mouse_State()) {Show_Mouse();};
	WWMessageBox().Process(TEXT_MEMORY_ERROR, TEXT_ABORT, false);

	ReadyToQuit = 1;

	Main_Window_Destroy();
	do
	{
		Keyboard->Check();
	}while (ReadyToQuit == 1);

	ExitProcess(0);
}








GraphicBufferClass* Read_PCX_File(char* name, char* Palette, void *Buff, int32_t Size);
void Load_Title_Screen(char *name, GraphicViewPortClass *video_page, uint8_t *palette)
{

	GraphicBufferClass *load_buffer;


	load_buffer = Read_PCX_File (name, (char*)palette, NULL, 0);

	if (load_buffer) {
		load_buffer->Blit(*video_page);
		delete load_buffer;
	}
}



#include "FILEPCX.H"

/***************************************************************************
 * READ_PCX_FILE -- read a pcx file into a Graphic Buffer                  *
 *                                                                         *
 *	GraphicBufferClass* Read_PCX_File (char* name, char* palette ,void *Buff, long size );	*
 *  																								*
 *                                                                         *
 * INPUT: name is a NULL terminated string of the format [xxxx.pcx]        *
 *        palette is optional, if palette != NULL the the color palette of *
 *					 the pcx file will be place in the memory block pointed	   *
 *               by palette.																*
 *			 Buff is optional, if Buff == NULL a new memory Buffer		 		*
 *					 will be allocated, otherwise the file will be placed 		*
 *					 at location pointed by Buffer;										*
 *			Size is the size in bytes of the memory block pointed by Buff		*
 *				  is also optional;															*                                                                         *
 * OUTPUT: on success a pointer to a GraphicBufferClass containing the     *
 *         pcx file, NULL otherwise.                                       *
 *																									*
 * WARNINGS:                                                               *
 *         Appears to be a comment-free zone                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/03/1995 JRJ : Created.                                             *
 *   04/30/1996 ST : Tidied up and modified to use CCFileClass             *
 *=========================================================================*/

#define	POOL_SIZE 2048
#define	READ_CHAR()  *file_ptr++ ; \
							 if ( file_ptr	>= & pool [ POOL_SIZE ]	) { \
								 file_handle.Read (pool , POOL_SIZE ); \
								 file_ptr = pool ; \
							 }


GraphicBufferClass* Read_PCX_File(char* name, char* palette, void *Buff, int32_t Size)
{
	unsigned					i, j;
	unsigned					rle;
	unsigned					color;
	unsigned					scan_pos;
	char						*file_ptr;
	int						width;
	int						height;
	char						*buffer;
	PCX_HEADER				header;
	RGB						*pal;
	char						pool [POOL_SIZE];
	GraphicBufferClass	*pic;

	CCFileClass file_handle(name);

	if (!file_handle.Is_Available()) return (NULL);

	file_handle.Open(READ);

	file_handle.Read (&header, sizeof (PCX_HEADER));

	if (header.id != 10 &&  header.version != 5 && header.pixelsize != 8 ) return NULL ;

	width = header.width - header.x + 1;
	height = header.height - header.y + 1;

	if (Buff) {
    	buffer = (char *)Buff;
    	i = Size / width;
    	height = MIN (i - 1, height);
    	pic = new GraphicBufferClass(width, height, buffer, Size);
    	if ( !(pic && pic->Get_Buffer()) ) return NULL ;
	} else {
    	pic = new GraphicBufferClass(width, height, NULL, width*(height+4));
    	if ( !(pic && pic->Get_Buffer()) ) return NULL ;
	}

	buffer = (char *) pic->Get_Buffer();
	file_ptr = pool ;
	file_handle.Read (pool , POOL_SIZE);

	if ( header.byte_per_line != width ) {

		for ( scan_pos = j = 0 ; j < height ; j ++, scan_pos += width ) {
			for ( i = 0 ; i < width ; ) {
				rle = READ_CHAR ();
				if ( rle > 192 ) {
					rle -= 192 ;
					color =	READ_CHAR (); ;
					memset ( buffer + scan_pos + i , color , rle );
					i += rle;
				} else {
					*(buffer+scan_pos + i++ ) = (char)rle;
				}
			}
      	}

		if ( i == width ) rle = READ_CHAR ();
		if ( rle > 192 )  rle = READ_CHAR ();

	} else {

		for ( i = 0 ; i < width * height ; ) {
  			rle = READ_CHAR ();
  			rle &= 0xff;
  			if ( rle > 192 ) {
        		rle -= 192 ;
        		color = READ_CHAR ();
  				memset ( buffer + i , color , rle );
        		i += rle ;
     		} else {
				*(buffer + i++) = (char)rle;
			}
		}
	}

	if ( palette ) {
		file_handle.Seek (- (256 * sizeof ( RGB )) , SEEK_END );
		file_handle.Read (palette , 256L * sizeof ( RGB ));

		pal = ( RGB * ) palette;
		for (i = 0 ; i < 256 ; i ++) {
			pal ->red	>>= 2;
			pal ->green	>>= 2;
			pal ->blue	>>= 2;
			pal ++ ;
		}
	}

	file_handle.Close();
	return pic;
}
