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

/***************************************************************************
 **     C O N F I D E N T I A L --- W E S T W O O D   S T U D I O S       **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : 32 bit library                           *
 *                                                                         *
 *                    File Name : VIDEO.H                                  *
 *                                                                         *
 *                   Programmer : Scott K. Bowen                           *
 *                                                                         *
 *                   Start Date : August 3, 1994                           *
 *                                                                         *
 *                  Last Update : August 3, 1994   [SKB]                   *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef VIDEO_H
#include <stdint.h>
#define VIDEO_H

#ifndef WWSTD_H
#include "WWSTD.H"
#endif
					   
/*=========================================================================*/
/*	The machine can be in one of the following graphic modes.  The global	*/
/*	GraphicMode is set to one of these values.										*/
/*=========================================================================*/
#define	CGA_MODE					4			// DOS 320x200 4 color mode
#define	TGA_MODE					9			// TANDY 320x200 16 color mode
#define	EGA_MODE					13			// DOS 320x200 16 color mode
#define	MCGA_MODE				0x13		// DOS 320x200 256 color mode
#define	VGA_MODE					0x13		// DOS 320x200 256 color mode
#define	EEGA_MODE				14			// DOS 640x400 16 color mode
#define	ETGA_MODE				9			// TANDY 640x400 16 color mode
#define	HGA_MODE					7			// DOS 768x400 2 color mode
#define	TXT_MODE					3			// DOS plain old color text mode
#define	VESA_640X400_256		0x100		// VESA 640x400 256 color mode
#define	VESA_640X480_256		0x101		// VESA 640x480 256 color mode
#define	VESA_800X600_256		0x103		// VESA 800x600 256 color mode
#define	VESA_1024X768_256		0x105		//	VESA 1024x768 256 color mode
#define	VESA_1280X400_256		0x107		// VESA 1280x400 256 color mode
#define	VESA_TEXT_80X60		0x108		// VESA 80x60 text mode
#define	VESA_TEXT_132X25		0x109		// VESA 132x25 text mode
#define	VESA_TEXT_132X60		0x10C		// VESA 132x60 text mode
#define	RESET_MODE				-1
#define	UNINITIALIZED_MODE	-1
#define	VESA_MIN 				VESA_640X400_256
#define	VESA_MAX					VESA_TEXT_132X60

/*=========================================================================*/
/* Define the maximum number of bank entries											*/
/*=========================================================================*/
#define	MAX_BANK_ENTRIES		((1280L*1024L)/65536L)


/*=========================================================================*
 * VesaInfoType - General info about this VESA implementation					*
 *                (Filled in by VESA BIOS Function 0)								*
 *																									*
 * Signature    - Will always be 'VESA'												*
 * Version      - Version #																*
 * OEMString    - OEM ID string															*
 * Capabilities - Not defined by VESA yet												*
 * AvailModes   - List of available modes; terminated with -1 (0xffff)		*
 * TotalMemory  - ???																		*
 * Reserved     - Pads structure to 256 bytes total								*
 *=========================================================================*/
typedef struct {
	char    Signature[4];
	int16_t   Version;
	uint32_t OEMString;
	int32_t    Capabilities;
	uint32_t AvailModes;
	int16_t   TotalMemory;
	char    Reserved[236];
} VesaInfoType;


/*=========================================================================*
 * VesaModeInfoType - Info about this VESA mode										*
 *                    (Filled in by VESA BIOS Function 1)						*
 *																									*
 * Attributes       - bit 0: 1 = mode is supported									*
 *                    bit 1: 1 = optional info available							*
 *                    bit 2: 1 = std BIOS output funcs valid in this mode	*
 *                    bit 3: 0 = monochrome, 1 = color							*
 *                    bit 4: 0 = text mode, 1 = graphics							*
 * WinA_Attributes  - bit 0 = win exists, bit 1=readable, bit 2= writable	*
 * WinB_Attributes  - bit 0 = win exists, bit 1=readable, bit 2= writable	*
 * WinGranularity   - smallest address boundary window can be placed upon;	*
 *                    size is in KB (ie 64, 32, 4)									*
 * WinSize          - size of windows in KB (ie 64, 32)							*
 * WinA_Segment     - location of Window A in CPU space (usually 0xa000)	*
 * WinB_Segment     - location of Window B in CPU space (usually 0xb000)	*
 * WinFunc          - address of window-setting function (This is provided *
 *                    as an alternative to Int 10 for speed.)					*
 * BytesPerScanline - # bytes per scan line											*
 * 																								*
 * Optional info (available if bit 1 of Attributes is set):						*
 *																									*
 * XRes             - X-resolution														*
 * YRes             - Y-resolution														*
 * XCharSize        - Horizontal size of char cell									*
 * YCharSize        - Vertical size of char cell									*
 * NumPlanes        - # of memory planes (???)										*
 * BitsPerPixel     - # bites per pixel												*
 * NumBanks         - # of banks (ie planes)											*
 * MemoryModel      - 00h = Text mode													*
 *                    01h = CGA mode													*
 *                    02h = Hercules													*
 *                    03h = 4 plane planar mode										*
 *                    04h = packed pixel mode (1 byte/pixel)					*
 *                    05h = non-chain 4, 256-color mode							*
 *                    06-0Fh = 															*
 *                    10-FFh = OEM-specific											*
 * BankSize         - Bank size in KB													*
 *=========================================================================*/
typedef struct {
	int16_t   Attributes;
	char    WinA_Attributes;
	char    WinB_Attributes;
	int16_t   WinGranularity;
	int16_t   WinSize;
	int16_t   WinA_Segment;
	int16_t   WinB_Segment;
	uint32_t WinFunc;
	int16_t   BytesPerScanline;
	int16_t   XRes;
	int16_t   YRes;
	char    XCharSize;
	char    YCharSize;
	char    NumPlanes;
	char    BitsPerPixel;
	char    NumBanks;
	char    MemoryModel;
	char    BankSize;
	char    NumInputPages;
	char    Reserved;
	char    RedMaskSize;
	char    RedFieldPosition;
	char    GreenMaskSize;
	char    GreenFieldPosition;
	char    BlueMaskSize; 
	char    BlueFieldPosition;
	char    RsvdMaskSize;
	char    RsvdFieldPosition;
	char    DirectColorModeInfo;
	char    pad[216];
} VesaModeInfoType;


extern "C" {
extern	int	GraphicMode;
extern	int32_t	XRes;
extern	int32_t	YRes;

extern int32_t		 BankTable [];
extern uint32_t	 VesaFunc;
extern uint32_t RMVesaVector ;
extern uint32_t RMVesaRegs ;
}

/*=========================================================================*/
/* The following prototypes are for the file: VIDEO.CPP							*/
/*=========================================================================*/

int Get_Video_Mode(void);
int Set_Video_Mode(int mode);
void Vesa_Info(void);
void Vesa_Set_Window(int32_t grain_num);

/*=========================================================================*/
/* The following prototypes are for the file: INITDLAY.CPP	  					*/
/*=========================================================================*/

extern void Init_Delay(void);
extern int32_t VertBlank;

/*=========================================================================*/
/* The following prototypes are for the file: VERTBLNK.ASM	  					*/
/*=========================================================================*/
				 				   
extern "C" {
	extern int16_t Get_Vert_Blank(void);
	extern void Wait_Vert_Blank(int32_t blank);
}

/*=========================================================================*/

#endif // VIDEO_H
