/*
**Command & Conquer Red Alert(tm)
**Copyright 2025 Electronic Arts Inc.
**
**This program is free software: you can redistribute it and/or modify
**it under the terms of the GNU General Public License as published by
**the Free Software Foundation, either version 3 of the License, or
**(at your option) any later version.
**
**This program is distributed in the hope that it will be useful,
**but WITHOUT ANY WARRANTY; without even the implied warranty of
**MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**GNU General Public License for more details.
**
**You should have received a copy of the GNU General Public License
**along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DRAWBUFF.H"
#include "FONT.H"
#include "GBUFFER.H"
#include "ICONCACH.H"
#include "TILE.H"
#include "WWSTD.H"
#include <stdint.h>
#include <string.h>

extern "C" {
unsigned char ColorXlat[256];
}

static int FontPaletteInitialized = 0;


static void Ensure_Font_Palette(void)
{
if (!FontPaletteInitialized) {
memset(ColorXlat, 0, sizeof(ColorXlat));
for (int index = 0; index < 16; ++index) {
ColorXlat[index] = (unsigned char)index;
ColorXlat[index << 4] = (unsigned char)index;
}
FontPaletteInitialized = 1;
}
}


static inline unsigned char * View_Data(GraphicViewPortClass *view)
{
return((unsigned char *)(uintptr_t)view->Get_Offset());
}


static inline unsigned char const * View_Data(GraphicViewPortClass const *view)
{
return((unsigned char const *)(uintptr_t)view->Get_Offset());
}


static inline int View_Row_Bytes(GraphicViewPortClass const *view)
{
return(view->Get_Width() + view->Get_XAdd() + view->Get_Pitch());
}


static uint16_t Read_Font_Word(unsigned char const *ptr)
{
uint16_t value;
memcpy(&value, ptr, sizeof(value));
return(value);
}


static int Clip_Copy_Rect(int src_width,
int src_height,
int dst_width,
int dst_height,
int *src_x,
int *src_y,
int *dst_x,
int *dst_y,
int *copy_width,
int *copy_height)
{
if (*src_x < 0) {
*dst_x -= *src_x;
*copy_width += *src_x;
*src_x = 0;
}
if (*src_y < 0) {
*dst_y -= *src_y;
*copy_height += *src_y;
*src_y = 0;
}
if (*dst_x < 0) {
*src_x -= *dst_x;
*copy_width += *dst_x;
*dst_x = 0;
}
if (*dst_y < 0) {
*src_y -= *dst_y;
*copy_height += *dst_y;
*dst_y = 0;
}

if (*src_x + *copy_width > src_width) {
*copy_width = src_width - *src_x;
}
if (*src_y + *copy_height > src_height) {
*copy_height = src_height - *src_y;
}
if (*dst_x + *copy_width > dst_width) {
*copy_width = dst_width - *dst_x;
}
if (*dst_y + *copy_height > dst_height) {
*copy_height = dst_height - *dst_y;
}

return(*copy_width > 0 && *copy_height > 0);
}


static int Regions_Overlap(unsigned char const *src,
int src_row_bytes,
unsigned char const *dst,
int dst_row_bytes,
int width,
int height)
{
unsigned char const *src_end;
unsigned char const *dst_end;

if (width <= 0 || height <= 0) {
return(0);
}

src_end = src + (height - 1) * src_row_bytes + width;
dst_end = dst + (height - 1) * dst_row_bytes + width;
return(src < dst_end && dst < src_end);
}


static int Resolve_Icon(IControl_Type const *control, int icon)
{
if (!control) {
return(-1);
}

if (control->Map) {
icon = *(((unsigned char const *)control) + control->Map + icon);
}

if (icon < 0 || icon >= control->Count) {
return(-1);
}

return(icon);
}


extern "C" long __cdecl Buffer_Size_Of_Region(void *thisptr, int w, int h)
{
(void)thisptr;
return((long)w * (long)h);
}


extern "C" void __cdecl Set_Font_Palette_Range(void const *palette, INT start_idx, INT end_idx)
{
unsigned char const *colors;
int start;
int end;

Ensure_Font_Palette();
if (!palette) {
return;
}

colors = (unsigned char const *)palette;
start = start_idx & 0x0F;
end = end_idx & 0x0F;
if (end < start) {
return;
}

for (int index = start; index <= end; ++index) {
unsigned char color = colors[index - start];
ColorXlat[index] = color;
ColorXlat[index << 4] = color;
}
}


extern "C" void * __cdecl Get_Font_Palette_Ptr(void)
{
Ensure_Font_Palette();
return(ColorXlat);
}


extern "C" void __cdecl Buffer_Put_Pixel(void *thisptr, int x, int y, unsigned char color)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;

if (!view || x < 0 || y < 0 || x >= view->Get_Width() || y >= view->Get_Height()) {
return;
}

View_Data(view)[y * View_Row_Bytes(view) + x] = color;
}


extern "C" int __cdecl Buffer_Get_Pixel(void *thisptr, int x, int y)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;

if (!view || x < 0 || y < 0 || x >= view->Get_Width() || y >= view->Get_Height()) {
return(0);
}

return(View_Data(view)[y * View_Row_Bytes(view) + x]);
}


extern "C" void __cdecl Buffer_Clear(void *thisptr, unsigned char color)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
unsigned char *dest;
int row_bytes;

if (!view) {
return;
}

dest = View_Data(view);
row_bytes = View_Row_Bytes(view);
for (int row = 0; row < view->Get_Height(); ++row) {
memset(dest, color, view->Get_Width());
dest += row_bytes;
}
}


extern "C" void __cdecl Buffer_Fill_Rect(void *thisptr, int sx, int sy, int dx, int dy, unsigned char color)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
unsigned char *dest;
int x0;
int y0;
int x1;
int y1;
int row_bytes;

if (!view) {
return;
}

x0 = MIN(sx, dx);
y0 = MIN(sy, dy);
x1 = MAX(sx, dx);
y1 = MAX(sy, dy);
if (x1 < 0 || y1 < 0 || x0 >= view->Get_Width() || y0 >= view->Get_Height()) {
return;
}

if (x0 < 0) x0 = 0;
if (y0 < 0) y0 = 0;
if (x1 >= view->Get_Width()) x1 = view->Get_Width() - 1;
if (y1 >= view->Get_Height()) y1 = view->Get_Height() - 1;
if (x1 < x0 || y1 < y0) {
return;
}

row_bytes = View_Row_Bytes(view);
dest = View_Data(view) + y0 * row_bytes + x0;
for (int row = y0; row <= y1; ++row) {
memset(dest, color, x1 - x0 + 1);
dest += row_bytes;
}
}


extern "C" void __cdecl Buffer_Remap(void *thisptr, int sx, int sy, int width, int height, void *remap)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
unsigned char *dest;
unsigned char const *table = (unsigned char const *)remap;
int x0 = sx;
int y0 = sy;
int x1 = sx + width;
int y1 = sy + height;
int row_bytes;

if (!view || !table || width <= 0 || height <= 0) {
return;
}

if (x1 <= 0 || y1 <= 0 || x0 >= view->Get_Width() || y0 >= view->Get_Height()) {
return;
}

if (x0 < 0) x0 = 0;
if (y0 < 0) y0 = 0;
if (x1 > view->Get_Width()) x1 = view->Get_Width();
if (y1 > view->Get_Height()) y1 = view->Get_Height();
if (x1 <= x0 || y1 <= y0) {
return;
}

row_bytes = View_Row_Bytes(view);
dest = View_Data(view) + y0 * row_bytes + x0;
for (int row = y0; row < y1; ++row) {
for (int column = 0; column < (x1 - x0); ++column) {
dest[column] = table[dest[column]];
}
dest += row_bytes;
}
}


extern "C" long __cdecl Buffer_To_Buffer(void *thisptr, int x, int y, int w, int h, void *buff, long size)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
unsigned char const *src;
unsigned char *dest;
int source_x = x;
int source_y = y;
int dest_x = 0;
int dest_y = 0;
int buffer_width = w;
int buffer_height = h;
int row_bytes;

if (!view || !buff || w <= 0 || h <= 0) {
return(0);
}

if ((long)buffer_width * (long)buffer_height > size) {
return(0);
}

if (source_x < 0) {
dest_x = -source_x;
w += source_x;
source_x = 0;
}
if (source_y < 0) {
dest_y = -source_y;
h += source_y;
source_y = 0;
}
if (source_x + w > view->Get_Width()) {
w = view->Get_Width() - source_x;
}
if (source_y + h > view->Get_Height()) {
h = view->Get_Height() - source_y;
}
if (w <= 0 || h <= 0) {
return(0);
}

row_bytes = View_Row_Bytes(view);
src = View_Data(view) + source_y * row_bytes + source_x;
dest = (unsigned char *)buff + dest_y * buffer_width + dest_x;
for (int row = 0; row < h; ++row) {
memcpy(dest, src, w);
dest += buffer_width;
src += row_bytes;
}

return((long)w * (long)h);
}


extern "C" long __cdecl Buffer_To_Page(int x, int y, int w, int h, void *Buffer, void *viewptr)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)viewptr;
unsigned char const *src;
unsigned char *dest;
int src_x = 0;
int src_y = 0;
int dest_x = x;
int dest_y = y;
int source_width = w;
int row_bytes;

if (!view || !Buffer || w <= 0 || h <= 0) {
return(0);
}

if (dest_x < 0) {
src_x = -dest_x;
w += dest_x;
dest_x = 0;
}
if (dest_y < 0) {
src_y = -dest_y;
h += dest_y;
dest_y = 0;
}
if (dest_x + w > view->Get_Width()) {
w = view->Get_Width() - dest_x;
}
if (dest_y + h > view->Get_Height()) {
h = view->Get_Height() - dest_y;
}
if (w <= 0 || h <= 0) {
return(0);
}

row_bytes = View_Row_Bytes(view);
src = (unsigned char const *)Buffer + src_y * source_width + src_x;
dest = View_Data(view) + dest_y * row_bytes + dest_x;
for (int row = 0; row < h; ++row) {
memcpy(dest, src, w);
dest += row_bytes;
src += source_width;
}

return((long)w * (long)h);
}


extern "C" BOOL __cdecl Linear_Blit_To_Linear(void *thisptr,
void *destptr,
int x_pixel,
int y_pixel,
int dx_pixel,
int dy_pixel,
int pixel_width,
int pixel_height,
BOOL trans)
{
GraphicViewPortClass *source_view = (GraphicViewPortClass *)thisptr;
GraphicViewPortClass *dest_view = (GraphicViewPortClass *)destptr;
unsigned char const *source;
unsigned char *dest;
unsigned char *scratch = NULL;
int source_x = x_pixel;
int source_y = y_pixel;
int dest_x = dx_pixel;
int dest_y = dy_pixel;
int width = pixel_width;
int height = pixel_height;
int source_row_bytes;
int dest_row_bytes;
int overlapped;

if (!source_view || !dest_view) {
return(FALSE);
}

if (!Clip_Copy_Rect(source_view->Get_Width(), source_view->Get_Height(), dest_view->Get_Width(), dest_view->Get_Height(),
&source_x, &source_y, &dest_x, &dest_y, &width, &height)) {
return(FALSE);
}

source_row_bytes = View_Row_Bytes(source_view);
dest_row_bytes = View_Row_Bytes(dest_view);
source = View_Data(source_view) + source_y * source_row_bytes + source_x;
dest = View_Data(dest_view) + dest_y * dest_row_bytes + dest_x;
overlapped = Regions_Overlap(source, source_row_bytes, dest, dest_row_bytes, width, height);

if (overlapped) {
scratch = new unsigned char[width * height];
if (scratch) {
for (int row = 0; row < height; ++row) {
memcpy(scratch + row * width, source + row * source_row_bytes, width);
}
source = scratch;
source_row_bytes = width;
}
}

if (!trans) {
for (int row = 0; row < height; ++row) {
memmove(dest + row * dest_row_bytes, source + row * source_row_bytes, width);
}
} else {
for (int row = 0; row < height; ++row) {
unsigned char const *src_row = source + row * source_row_bytes;
unsigned char *dst_row = dest + row * dest_row_bytes;
for (int column = 0; column < width; ++column) {
unsigned char value = src_row[column];
if (value != 0) {
dst_row[column] = value;
}
}
}
}

delete[] scratch;
return(TRUE);
}


extern "C" BOOL __cdecl Linear_Scale_To_Linear(void *thisptr,
void *destptr,
int src_x,
int src_y,
int dst_x,
int dst_y,
int src_width,
int src_height,
int dst_width,
int dst_height,
BOOL trans,
char *remap)
{
GraphicViewPortClass *source_view = (GraphicViewPortClass *)thisptr;
GraphicViewPortClass *dest_view = (GraphicViewPortClass *)destptr;
double source_x0 = (double)src_x;
double source_y0 = (double)src_y;
double source_x1 = (double)(src_x + src_width);
double source_y1 = (double)(src_y + src_height);
double dest_x0 = (double)dst_x;
double dest_y0 = (double)dst_y;
double dest_x1 = (double)(dst_x + dst_width);
double dest_y1 = (double)(dst_y + dst_height);
unsigned char const *source_data;
unsigned char *dest_data;
int source_row_bytes;
int dest_row_bytes;
int start_x;
int start_y;
int end_x;
int end_y;
double clipped_dest_width;
double clipped_dest_height;
double clipped_source_width;
double clipped_source_height;

if (!source_view || !dest_view || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
return(FALSE);
}

if (source_x0 < 0.0) {
double ratio = (0.0 - source_x0) / (source_x1 - source_x0);
dest_x0 += (dest_x1 - dest_x0) * ratio;
source_x0 = 0.0;
}
if (source_y0 < 0.0) {
double ratio = (0.0 - source_y0) / (source_y1 - source_y0);
dest_y0 += (dest_y1 - dest_y0) * ratio;
source_y0 = 0.0;
}
if (source_x1 > source_view->Get_Width()) {
double ratio = (source_x1 - source_view->Get_Width()) / (source_x1 - source_x0);
dest_x1 -= (dest_x1 - dest_x0) * ratio;
source_x1 = (double)source_view->Get_Width();
}
if (source_y1 > source_view->Get_Height()) {
double ratio = (source_y1 - source_view->Get_Height()) / (source_y1 - source_y0);
dest_y1 -= (dest_y1 - dest_y0) * ratio;
source_y1 = (double)source_view->Get_Height();
}
if (dest_x0 < 0.0) {
double ratio = (0.0 - dest_x0) / (dest_x1 - dest_x0);
source_x0 += (source_x1 - source_x0) * ratio;
dest_x0 = 0.0;
}
if (dest_y0 < 0.0) {
double ratio = (0.0 - dest_y0) / (dest_y1 - dest_y0);
source_y0 += (source_y1 - source_y0) * ratio;
dest_y0 = 0.0;
}
if (dest_x1 > dest_view->Get_Width()) {
double ratio = (dest_x1 - dest_view->Get_Width()) / (dest_x1 - dest_x0);
source_x1 -= (source_x1 - source_x0) * ratio;
dest_x1 = (double)dest_view->Get_Width();
}
if (dest_y1 > dest_view->Get_Height()) {
double ratio = (dest_y1 - dest_view->Get_Height()) / (dest_y1 - dest_y0);
source_y1 -= (source_y1 - source_y0) * ratio;
dest_y1 = (double)dest_view->Get_Height();
}

clipped_dest_width = dest_x1 - dest_x0;
clipped_dest_height = dest_y1 - dest_y0;
clipped_source_width = source_x1 - source_x0;
clipped_source_height = source_y1 - source_y0;
if (clipped_dest_width <= 0.0 || clipped_dest_height <= 0.0 || clipped_source_width <= 0.0 || clipped_source_height <= 0.0) {
return(FALSE);
}

start_x = (int)dest_x0;
start_y = (int)dest_y0;
end_x = (int)dest_x1;
end_y = (int)dest_y1;
if (start_x < 0) start_x = 0;
if (start_y < 0) start_y = 0;
if (end_x > dest_view->Get_Width()) end_x = dest_view->Get_Width();
if (end_y > dest_view->Get_Height()) end_y = dest_view->Get_Height();
if (end_x <= start_x || end_y <= start_y) {
return(FALSE);
}

source_data = View_Data(source_view);
dest_data = View_Data(dest_view);
source_row_bytes = View_Row_Bytes(source_view);
dest_row_bytes = View_Row_Bytes(dest_view);

for (int y = start_y; y < end_y; ++y) {
double source_row_position = source_y0 + ((double)(y - dest_y0) * clipped_source_height / clipped_dest_height);
int source_row_index = (int)source_row_position;
unsigned char const *source_row;
unsigned char *dest_row;

if (source_row_index < 0) {
source_row_index = 0;
}
if (source_row_index >= source_view->Get_Height()) {
source_row_index = source_view->Get_Height() - 1;
}

source_row = source_data + source_row_index * source_row_bytes;
dest_row = dest_data + y * dest_row_bytes;
for (int x = start_x; x < end_x; ++x) {
double source_column_position = source_x0 + ((double)(x - dest_x0) * clipped_source_width / clipped_dest_width);
int source_column_index = (int)source_column_position;
unsigned char value;

if (source_column_index < 0) {
source_column_index = 0;
}
if (source_column_index >= source_view->Get_Width()) {
source_column_index = source_view->Get_Width() - 1;
}

value = source_row[source_column_index];
if (remap) {
value = ((unsigned char *)remap)[value];
}
if (!trans || value != 0) {
dest_row[x] = value;
}
}
}

return(TRUE);
}


extern "C" LONG __cdecl Buffer_Print(void *thisptr, const char *str, int x, int y, int fcolor, int bcolor)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
unsigned char const *font;
unsigned char const *info_block;
unsigned char const *width_block;
unsigned char const *height_block;
unsigned char const *offset_block;
unsigned char *buffer;
unsigned long current_draw;
unsigned long current_line;
int row_bytes;
int max_height;
int current_x;
int current_y;
int original_x;

if (!view || !str || !FontPtr) {
return(0);
}

font = (unsigned char const *)FontPtr;
info_block = font + Read_Font_Word(font + FONTINFOBLOCK);
width_block = font + Read_Font_Word(font + FONTWIDTHBLOCK);
height_block = font + Read_Font_Word(font + FONTHEIGHTBLOCK);
offset_block = font + Read_Font_Word(font + FONTOFFSETBLOCK);
max_height = info_block[FONTINFOMAXHEIGHT];
if (y + max_height > view->Get_Height()) {
return(0);
}

Ensure_Font_Palette();
ColorXlat[1] = (unsigned char)fcolor;
ColorXlat[16] = (unsigned char)fcolor;
ColorXlat[0] = (unsigned char)bcolor;

buffer = View_Data(view);
row_bytes = View_Row_Bytes(view);
current_x = x;
current_y = y;
original_x = x;
current_line = (unsigned long)(uintptr_t)(buffer + current_y * row_bytes);
current_draw = current_line + (unsigned long)current_x;

while (*str) {
unsigned char ch = (unsigned char)*str++;

if (ch == '\n' || ch == '\r') {
int next_y = current_y + max_height + FontYSpacing;
if (next_y + max_height > view->Get_Height()) {
return(0);
}
current_y = next_y;
current_line = (unsigned long)(uintptr_t)(buffer + current_y * row_bytes);
current_x = (ch == '\n') ? 0 : original_x;
current_draw = current_line + (unsigned long)current_x;
continue;
}

{
int width = width_block[ch];
int advance = width + FontXSpacing;
unsigned char const *data_ptr;
unsigned char const *char_height_ptr;
unsigned char top_blank;
unsigned char char_height;
int bottom_blank;
unsigned char *draw_ptr;

if (current_x + advance > view->Get_Width()) {
int next_y = current_y + max_height + FontYSpacing;
if (next_y + max_height > view->Get_Height()) {
return(0);
}
current_y = next_y;
current_line = (unsigned long)(uintptr_t)(buffer + current_y * row_bytes);
current_x = 0;
current_draw = current_line;
}

data_ptr = font + Read_Font_Word(offset_block + ch * 2);
char_height_ptr = height_block + ch * 2;
top_blank = char_height_ptr[0];
char_height = char_height_ptr[1];
bottom_blank = max_height - top_blank - char_height;
draw_ptr = (unsigned char *)(uintptr_t)current_draw;

if (ColorXlat[0] != 0) {
for (int row = 0; row < top_blank; ++row) {
memset(draw_ptr, ColorXlat[0], width);
draw_ptr += row_bytes;
}
} else {
draw_ptr += top_blank * row_bytes;
}

for (int row = 0; row < char_height; ++row) {
unsigned char *pixel_ptr = draw_ptr;
int column = 0;
while (column < width) {
unsigned char pair = *data_ptr++;
unsigned char lo = ColorXlat[pair & 0x0F];
if (lo != 0) {
*pixel_ptr = lo;
}
++pixel_ptr;
++column;
if (column < width) {
unsigned char hi = ColorXlat[(pair >> 4) & 0x0F];
if (hi != 0) {
*pixel_ptr = hi;
}
++pixel_ptr;
++column;
}
}
draw_ptr += row_bytes;
}

if (ColorXlat[0] != 0) {
for (int row = 0; row < bottom_blank; ++row) {
memset(draw_ptr, ColorXlat[0], width);
draw_ptr += row_bytes;
}
}

current_x += advance;
current_draw += (unsigned long)advance;
}
}

return((LONG)current_draw);
}


extern "C" void __cdecl Buffer_Draw_Stamp(void const *thisptr, void const *icondata, int icon, int x_pixel, int y_pixel, void const *remap)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;

if (!view) {
return;
}

Buffer_Draw_Stamp_Clip(thisptr, icondata, icon, x_pixel, y_pixel, remap, 0, 0, view->Get_Width(), view->Get_Height());
}


extern "C" void __cdecl Buffer_Draw_Stamp_Clip(void const *thisptr,
void const *icondata,
int icon,
int x_pixel,
int y_pixel,
void const *remap,
int min_x,
int min_y,
int max_x,
int max_y)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
IControl_Type const *control = (IControl_Type const *)icondata;
unsigned char const *icon_base;
unsigned char const *trans_table;
unsigned char *dest_base;
int draw_icon;
int icon_width;
int icon_height;
int icon_size;
int clip_x0;
int clip_y0;
int clip_x1;
int clip_y1;
int dest_x;
int dest_y;
int source_x = 0;
int source_y = 0;
int draw_width;
int draw_height;
int row_bytes;
int transparent;

if (!view || !control) {
return;
}

draw_icon = Resolve_Icon(control, icon);
if (draw_icon < 0) {
return;
}

icon_width = control->Width;
icon_height = control->Height;
icon_size = icon_width * icon_height;
icon_base = ((unsigned char const *)control) + control->Icons + draw_icon * icon_size;
trans_table = control->TransFlag ? (((unsigned char const *)control) + control->TransFlag) : NULL;
transparent = trans_table ? (trans_table[draw_icon] != 0) : TRUE;

clip_x0 = min_x;
clip_y0 = min_y;
clip_x1 = min_x + max_x;
clip_y1 = min_y + max_y;
dest_x = x_pixel + min_x;
dest_y = y_pixel + min_y;
draw_width = icon_width;
draw_height = icon_height;

if (dest_x >= clip_x1 || dest_y >= clip_y1 || dest_x + draw_width <= clip_x0 || dest_y + draw_height <= clip_y0) {
return;
}

if (dest_x < clip_x0) {
source_x += clip_x0 - dest_x;
draw_width -= clip_x0 - dest_x;
dest_x = clip_x0;
}
if (dest_y < clip_y0) {
source_y += clip_y0 - dest_y;
draw_height -= clip_y0 - dest_y;
dest_y = clip_y0;
}
if (dest_x + draw_width > clip_x1) {
draw_width = clip_x1 - dest_x;
}
if (dest_y + draw_height > clip_y1) {
draw_height = clip_y1 - dest_y;
}

if (dest_x < 0) {
source_x += -dest_x;
draw_width += dest_x;
dest_x = 0;
}
if (dest_y < 0) {
source_y += -dest_y;
draw_height += dest_y;
dest_y = 0;
}
if (dest_x + draw_width > view->Get_Width()) {
draw_width = view->Get_Width() - dest_x;
}
if (dest_y + draw_height > view->Get_Height()) {
draw_height = view->Get_Height() - dest_y;
}
if (draw_width <= 0 || draw_height <= 0) {
return;
}

row_bytes = View_Row_Bytes(view);
dest_base = View_Data(view) + dest_y * row_bytes + dest_x;
for (int row = 0; row < draw_height; ++row) {
unsigned char const *src_row = icon_base + (source_y + row) * icon_width + source_x;
unsigned char *dst_row = dest_base + row * row_bytes;

if (remap) {
unsigned char const *remap_table = (unsigned char const *)remap;
for (int column = 0; column < draw_width; ++column) {
unsigned char value = remap_table[src_row[column]];
if (value != 0) {
dst_row[column] = value;
}
}
} else if (!transparent) {
memcpy(dst_row, src_row, draw_width);
} else {
for (int column = 0; column < draw_width; ++column) {
unsigned char value = src_row[column];
if (value != 0) {
dst_row[column] = value;
}
}
}
}
}


extern "C" VOID __cdecl Buffer_Draw_Line(void *thisptr, int sx, int sy, int dx, int dy, unsigned char color)
{
GraphicViewPortClass *view = (GraphicViewPortClass *)thisptr;
int delta_x;
int delta_y;
int step_x;
int step_y;
int error;

if (!view) {
return;
}

delta_x = ABS(dx - sx);
step_x = (sx < dx) ? 1 : -1;
delta_y = -ABS(dy - sy);
step_y = (sy < dy) ? 1 : -1;
error = delta_x + delta_y;

for (;;) {
if (sx >= 0 && sy >= 0 && sx < view->Get_Width() && sy < view->Get_Height()) {
View_Data(view)[sy * View_Row_Bytes(view) + sx] = color;
}
if (sx == dx && sy == dy) {
break;
}

{
int twice_error = error + error;
if (twice_error >= delta_y) {
error += delta_y;
sx += step_x;
}
if (twice_error <= delta_x) {
error += delta_x;
sy += step_y;
}
}
}
}


extern "C" int Is_Icon_Cached(void const *icon_data, int icon)
{
(void)icon_data;
(void)icon;
return(-1);
}


extern "C" void Cache_Copy_Icon(void const *icon_ptr, void *surface, int pitch)
{
unsigned char const *source = (unsigned char const *)icon_ptr;
unsigned char *dest = (unsigned char *)surface;

if (!source || !dest || pitch < 24) {
return;
}

for (int row = 0; row < 24; ++row) {
memcpy(dest, source, 24);
source += 24;
dest += pitch;
}
}
