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
 **      C O N F I D E N T I A L --- W E S T W O O D   S T U D I O S      **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood 32 bit Library                  *
 *                                                                         *
 *                    File Name : AUDIO.H                                  *
 *                                                                         *
 *                   Programmer : Phil W. Gorrow                           *
 *                                                                         *
 *                   Start Date : March 10, 1995                           *
 *                                                                         *
 *                  Last Update : March 10, 1995   [PWG]                   *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "WWSTD.H"
#include <stdint.h>

/*=========================================================================*/
/* AUD file header type																		*/
/*=========================================================================*/
#define	AUD_FLAG_STEREO	1
#define	AUD_FLAG_16BIT		2

// PWG 3-14-95: This structure used to have bit fields defined for Stereo
//   and Bits.  These were removed because watcom packs them into a 32 bit
//   flag entry even though they could have fit in a 8 bit entry.
#pragma pack(push, 1)
typedef struct {
	uint16_t	Rate;				// Playback rate (hertz).
	int32_t	Size;				// Size of data (bytes).
	int32_t	UncompSize;				// Size of data (bytes).
	uint8_t Flags;	// Holds flags for info
								//  1: Is the sample stereo?
								//  2: Is the sample 16 bits?
	uint8_t Compression;	// What kind of compression for this sample?
} AUDHeaderType;
#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(AUDHeaderType) == 12, "AUDHeaderType must match the 12-byte on-disk layout");
#endif


/*=========================================================================*/
/*	There can be a different sound driver for sound effects, digitized		*/
/*	samples, and musical scores.  Each one must be of these specified			*/
/*	types.																						*/
/*=========================================================================*/
typedef enum {
	SAMPLE_NONE,		// No digitized sounds will be played.
	SAMPLE_SB,			// Sound Blaster digitized driver.
	SAMPLE_SBPRO,		// Sound Blaster Pro digitized driver.
	SAMPLE_PAS,			// Pro-Audio Spectrum digitized driver.
	SAMPLE_ADLIBG,		// Adlib-Gold digitized driver.
	SAMPLE_TANDY,		// Tandy 'compatible' driver.
	SAMPLE_PCSPKR,		// PC speaker digitized driver (The Audio Solution driver).
	SAMPLE_ADLIB,		// Adlib digitized driver (The Audio Solution driver).
	SAMPLE_TEMP=0x1000,
	SAMPLE_LAST
} Sample_Type;

typedef enum {
	SCORE_NONE,			// No scores will be played.
	SCORE_ALFX,			// Westwood's ALFX adlib compatable driver.
	SCORE_WWPCSPKR,	// Westwood's PC-speaker driver (obsolete).
	SCORE_WWTANDY,		// Westwood's PC-speaker driver with Tandy mod (obsolete).
	SCORE_PCSPKR,		// PC speaker XMIDI driver.
	SCORE_TANDY,		// Tandy XMIDI driver.
	SCORE_MT32,			// MT-32 / LAPC-1 Roland XMIDI driver.
	SCORE_CANVAS,		// Sound Canvas SC-55.
	SCORE_ADLIB,		// Adlib XMIDI driver.
	SCORE_ADLIBG,		// Adlib Gold XMIDI driver.
	SCORE_PASFM,		// Pro Audio Spectrum XMIDI driver.
	SCORE_SBFM,			// Sound Blaster XMIDI driver.
	SCORE_SBP1FM,		// Sound Blaster Pro (YM3812) XMIDI driver.
	SCORE_SBP2FM,		// Sound Blaster Pro (OPL3) XMIDI driver (Can't use with SFX_ALFX).
	SCORE_TEMP=0x1000,
	SCORE_LAST
} Score_Type;

typedef enum {
	SFX_NONE,			// No sound effects will be played.
	SFX_ALFX,			// Westwood's ALFX adlib compatable driver.
	SFX_WWPCSPKR,		// Westwood's PC-speaker driver.
	SFX_WWTANDY,		// Westwood's PC-speaker driver with Tandy mod.
	SFX_PCSPKR,			// PC speaker XMIDI driver.
	SFX_TANDY,			// Tandy XMIDI driver.
	SFX_MT32,			// MT-32 / LAPC-1 Roland XMIDI driver.
	SFX_CANVAS,			// Sound Canvas SC-55.
	SFX_ADLIB,			// Adlib XMIDI driver.
	SFX_ADLIBG,			// Adlib Gold XMIDI driver.
	SFX_PASFM,			// Pro Audio Spectrum XMIDI driver.
	SFX_SBFM,			// Sound Blaster XMIDI driver.
	SFX_SBP1FM,			// Sound Blaster Pro (YM3812) XMIDI driver.
	SFX_SBP2FM,			// Sound Blaster Pro (OPL3) XMIDI driver.
	SFX_TEMP=0x1000,
	SFX_LAST
} SFX_Type;



/*=========================================================================*/
/* The following prototypes are for the file: SOUNDIO.CPP						*/
/*=========================================================================*/
int32_t File_Stream_Sample(char const *filename, int32_t real_time_start = FALSE);
int32_t File_Stream_Sample_Vol(char const *filename, int32_t volume, int32_t real_time_start = FALSE);
void  Sound_Callback(void);
void   maintenance_callback(void);
void *Load_Sample(char const *filename);
int32_t Load_Sample_Into_Buffer(char const *filename, void *buffer, int32_t size);
int32_t Sample_Read(int32_t fh, void *buffer, int32_t size);
void Free_Sample(void const *sample);
int32_t Audio_Init( HWND window , int32_t bits_per_sample, int32_t stereo , int32_t rate , int32_t reverse_channels);
void Sound_End(void);
void Stop_Sample(int32_t handle);
int32_t Sample_Status(int32_t handle);
int32_t Is_Sample_Playing(void const * sample);
void Stop_Sample_Playing(void const * sample);
int32_t Play_Sample(void const *sample, int32_t priority=0xFF, int32_t volume=0xFF, int16_t panloc = 0x0);
int32_t Play_Sample_Handle(void const *sample, int32_t priority, int32_t volume, int16_t panloc, int32_t id);
int32_t Set_Sound_Vol(int32_t volume);
int32_t Set_Score_Vol(int32_t volume);
void Fade_Sample(int32_t handle, int32_t ticks);
int32_t Get_Free_Sample_Handle(int32_t priority);
int32_t Get_Digi_Handle(void);
int32_t Sample_Length(void const *sample);
void Restore_Sound_Buffers (void);
int32_t Set_Primary_Buffer_Format(void);
int32_t Start_Primary_Sound_Buffer (int32_t forced);
void Stop_Primary_Sound_Buffer (void);

/*
** Function to call if we detect focus loss
*/
extern	void (*Audio_Focus_Loss_Function)(void);


extern int32_t Misc;
extern SFX_Type SoundType;
extern Sample_Type SampleType;

extern int32_t StreamLowImpact;
