#include <stdint.h>
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


/*=========================================================================*/
/* The following prototypes are for the file: SOUNDIO.CPP						*/
/*=========================================================================*/

int16_t Decompress_Frame(void * source, void * dest, int16_t size);
int  Stream_Sample_Vol(void *buffer, int32_t size, int32_t (*callback);
int  Stream_Sample(void *buffer, int32_t size, int32_t (*callback);
int  File_Stream_Sample(char const *filename);
int  File_Stream_Sample_Vol(char const *filename, int volume);
void    Sound_Callback(void);
void     maintenance_callback(void);
void * Load_Sample(char const *filename);
int32_t  Load_Sample_Into_Buffer(char const *filename, void *buffer, int32_t size);
int32_t  Sample_Read(int fh, void *buffer, int32_t size);
void  Free_Sample(void const *sample);
int32_t  Sound_Init(int sfx, int score, int sample);
void  VQA_TimerCallback(void);
int32_t Audio_Init(int sample, int address, int inter, int dma);
void  Sound_End(void);
void  Stop_Sample(int handle);
int32_t  Sample_Status(int handle);
int32_t  Is_Sample_Playing(void const * sample);
void  Stop_Sample_Playing(void const * sample);
int  Play_Sample(void const *sample);
int  Play_Sample_Vol(void const *sample, int priority, int volume);
int  Set_Sound_Vol(int volume);
int  Set_Score_Vol(int volume);
void  Fade_Sample(int handle, int ticks);
