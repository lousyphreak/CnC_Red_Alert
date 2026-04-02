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



#ifndef MCGAPRIM_H
#include <stdint.h>
#define MCGAPRIM_H


#ifndef WWSTD_H
#include "WWSTD.H"
#endif

class GraphicViewPortClass;
class GraphicBufferClass;
class VideoBufferClass;
/*=========================================================================*/
/* Define functions which have not under-gone name mangling						*/
/*=========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
	/*======================================================================*/
	/* Externs for all of the common functions between the video buffer		*/
	/*		class and the graphic buffer class.											*/
	/*======================================================================*/
	extern int32_t MCGA_Size_Of_Region(void *thisptr, int w, int h);

	extern void	MCGA_Put_Pixel(void * thisptr, int x, int y, uint8_t color);
	extern int	MCGA_Get_Pixel(void * thisptr, int x, int y);
	extern void	MCGA_Clear(void *thisptr, uint8_t color);
	extern int32_t	MCGA_To_Buffer(void *thisptr, int x, int y, int w, int h, void *buff, int32_t size);
	extern int32_t MCGA_Buffer_To_Page(int x, int y, int w, int h, void *Buffer, void *view);
	extern int32_t	Linear_Blit_To_Linear(	void *thisptr, void * dest, int x_pixel, int y_pixel, int dx_pixel, 
													int dy_pixel, int pixel_width, int pixel_height, int32_t trans);
	extern int32_t Linear_Scale_To_Linear(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);

	extern void	Vesa_Put_Pixel(void * thisptr, int x, int y, uint8_t color);
	extern int	Vesa_Get_Pixel(void * thisptr, int x, int y);
	extern void	Vesa_Clear(void *thisptr, uint8_t color);
	extern int32_t	Vesa_To_Buffer(void *thisptr, int x, int y, int w, int h, void *buff, int32_t size);
	extern int32_t Vesa_Buffer_To_Page(int x, int y, int w, int h, void *Buffer, void *view);

	extern int32_t	Linear_Blit_To_Vesa(		void *thisptr, void * dest, int x_pixel, int y_pixel, int dx_pixel, 
													int dy_pixel, int pixel_width, int pixel_height, int32_t trans);
	extern int32_t	Vesa_Blit_To_Linear(		void *thisptr, void * dest, int x_pixel, int y_pixel, int dx_pixel, 
													int dy_pixel, int pixel_width, int pixel_height, int32_t trans);
	extern int32_t	Vesa_Blit_To_Vesa(		void *thisptr, void * dest, int x_pixel, int y_pixel, int dx_pixel, 
													int dy_pixel, int pixel_width, int pixel_height, int32_t trans);
	extern int32_t Linear_Scale_To_Vesa(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
	extern int32_t Vesa_Scale_To_Linear(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
	extern int32_t Vesa_Scale_To_Vesa(		void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
	extern int32_t MCGA_Print(					void *thisptr, const char *str, int x, int y, int fcolor, int bcolor);
	extern int32_t Vesa_Print(					void *thisptr, const char *str, int x, int y, int fcolor, int bcolor);

	/*======================================================================*/
	/* Externs for all of the graphic buffer class only functions				*/
	/*======================================================================*/
	extern void MCGA_Draw_Line(void *thisptr, int sx, int sy, int dx, int dy, uint8_t color);
	extern void MCGA_Fill_Rect(void *thisptr, int sx, int sy, int dx, int dy, uint8_t color);
	extern void MCGA_Remap(void * thisptr, int sx, int sy, int width, int height, void *remap);	
	extern void MCGA_Fill_Quad(void * thisptr, void *span_buff, int x0, int y0, int x1, int y1,
							int x2, int y2, int x3, int y3, int color);
	extern void MCGA_Draw_Stamp(void const *thisptr, void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap);

	extern void Shadow_Blit(int32_t xpix, int32_t ypix, int32_t width, int32_t height, GraphicViewPortClass &src, VideoBufferClass &dst, void *shadowbuff);

//	extern int Get_Standard_Selector(void);
//	extern void Set_Selector(uint16_t sel);
#ifdef __cplusplus
}
#endif

extern int32_t (*VVPC_Blit_to_GVPC_Func)(void *, void *, int, int, int, int, int, int, int32_t);
extern int32_t (*VVPC_Blit_to_VVPC_Func)(void *, void *, int, int, int, int, int, int, int32_t);
extern void (*VVPC_Clear_Func)(void *, uint8_t);
extern int32_t (*VVPC_To_Buffer_Func)(void *,int x, int y, int w, int h, void *buff, int32_t size);
extern void (*VVPC_Put_Pixel_Func)(void *,int x, int y, uint8_t color);
extern int  (*VVPC_Get_Pixel_Func)(void *, int x, int y);
extern int32_t (*VVPC_Buffer_To_Page)(int x, int y, int w, int h, void *buffer, void *view);
extern int32_t (*GVPC_Blit_to_VVPC_Func)(void *, void *, int, int, int, int, int, int, int32_t);
extern int32_t (*VVPC_Blit_to_GVPC_Func)(void *, void *, int, int, int, int, int, int, int32_t);
extern int32_t (*VVPC_Blit_to_VVPC_Func)(void *, void *, int, int, int, int, int, int, int32_t);
extern int32_t (*VVPC_Scale_To_GVPC)(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
extern int32_t (*VVPC_Scale_To_VVPC)(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
extern int32_t (*GVPC_Scale_To_VVPC)(	void *, void *, int, int, int, int, int, int, int, int, int32_t, char *);
extern int32_t (*VVPC_Print_Func)(void *, const char *, int, int, int, int);
extern GraphicBufferClass *LogicPage;
#endif
