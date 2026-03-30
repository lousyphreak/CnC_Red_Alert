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

#include "MOUSE.H"
#include "IFF.H"
#include "SHAPE.H"
#include <stdint.h>
#include <new>
#include <string.h>


static inline unsigned char *View_Data(GraphicViewPortClass *view)
{
	return((unsigned char *)(uintptr_t)view->Get_Offset());
}


static inline int View_Row_Bytes(GraphicViewPortClass const *view)
{
	return(view->Get_Width() + view->Get_XAdd() + view->Get_Pitch());
}


static unsigned char const *Prepare_Cursor_Data(void const *cursor, unsigned char **storage)
{
	Shape_Type const *shape = (Shape_Type const *)cursor;
	unsigned char *buffer;
	int header_size;

	*storage = NULL;
	if (!cursor) {
		return(NULL);
	}

	header_size = (shape->ShapeType & MAKESHAPE_COMPACT) ? 26 : 10;
	if (shape->ShapeType & MAKESHAPE_NOCOMP) {
		return((unsigned char const *)cursor);
	}

	buffer = new(std::nothrow) unsigned char[header_size + Get_Shape_Uncomp_Size(cursor)];
	if (!buffer) {
		return(NULL);
	}

	memcpy(buffer, cursor, header_size);
	((Shape_Type *)buffer)->ShapeType = (unsigned short)(shape->ShapeType | MAKESHAPE_NOCOMP);
	LCW_Uncompress((void *)((unsigned char const *)cursor + header_size), buffer + header_size, (unsigned long)Get_Shape_Uncomp_Size(cursor));
	*storage = buffer;
	return(buffer);
}


extern "C" void __cdecl Mouse_Shadow_Buffer(void *thisptr, GraphicViewPortClass *srcdst, void *buffer, int x, int y, int hotx, int hoty, int store)
{
	WWMouseClass *mouse = (WWMouseClass *)thisptr;
	unsigned char *view_ptr;
	unsigned char *buffer_ptr;
	int x0;
	int y0;
	int width;
	int height;
	int row_bytes;

	if (!mouse || !srcdst || !buffer || mouse->CursorWidth <= 0 || mouse->CursorHeight <= 0) {
		return;
	}

	x0 = x - hotx;
	y0 = y - hoty;
	if (x0 >= srcdst->Get_Width() || y0 >= srcdst->Get_Height() || (x0 + mouse->CursorWidth) <= 0 || (y0 + mouse->CursorHeight) <= 0) {
		return;
	}

	width = mouse->CursorWidth;
	height = mouse->CursorHeight;
	buffer_ptr = (unsigned char *)buffer;
	if (x0 < 0) {
		buffer_ptr += -x0;
		width += x0;
		x0 = 0;
	}
	if (y0 < 0) {
		buffer_ptr += (-y0) * mouse->CursorWidth;
		height += y0;
		y0 = 0;
	}
	if (x0 + width > srcdst->Get_Width()) {
		width = srcdst->Get_Width() - x0;
	}
	if (y0 + height > srcdst->Get_Height()) {
		height = srcdst->Get_Height() - y0;
	}
	if (width <= 0 || height <= 0) {
		return;
	}

	row_bytes = View_Row_Bytes(srcdst);
	view_ptr = View_Data(srcdst) + y0 * row_bytes + x0;
	for (int row = 0; row < height; ++row) {
		if (store) {
			memcpy(buffer_ptr, view_ptr, width);
		} else {
			memcpy(view_ptr, buffer_ptr, width);
		}
		view_ptr += row_bytes;
		buffer_ptr += mouse->CursorWidth;
	}
}


extern "C" void __cdecl Draw_Mouse(void *thisptr, GraphicViewPortClass *srcdst, int x, int y)
{
	WWMouseClass *mouse = (WWMouseClass *)thisptr;
	unsigned char const *cursor_ptr;
	unsigned char *view_ptr;
	int x0;
	int y0;
	int width;
	int height;
	int row_bytes;

	if (!mouse || !srcdst || mouse->CursorWidth <= 0 || mouse->CursorHeight <= 0 || !mouse->MouseCursor) {
		return;
	}

	x0 = x - mouse->MouseXHot;
	y0 = y - mouse->MouseYHot;
	if (x0 >= srcdst->Get_Width() || y0 >= srcdst->Get_Height() || (x0 + mouse->CursorWidth) <= 0 || (y0 + mouse->CursorHeight) <= 0) {
		return;
	}

	width = mouse->CursorWidth;
	height = mouse->CursorHeight;
	cursor_ptr = (unsigned char const *)mouse->MouseCursor;
	if (x0 < 0) {
		cursor_ptr += -x0;
		width += x0;
		x0 = 0;
	}
	if (y0 < 0) {
		cursor_ptr += (-y0) * mouse->CursorWidth;
		height += y0;
		y0 = 0;
	}
	if (x0 + width > srcdst->Get_Width()) {
		width = srcdst->Get_Width() - x0;
	}
	if (y0 + height > srcdst->Get_Height()) {
		height = srcdst->Get_Height() - y0;
	}
	if (width <= 0 || height <= 0) {
		return;
	}

	row_bytes = View_Row_Bytes(srcdst);
	view_ptr = View_Data(srcdst) + y0 * row_bytes + x0;
	for (int row = 0; row < height; ++row) {
		for (int column = 0; column < width; ++column) {
			unsigned char pixel = cursor_ptr[column];
			if (pixel != 0) {
				view_ptr[column] = pixel;
			}
		}
		cursor_ptr += mouse->CursorWidth;
		view_ptr += row_bytes;
	}
}


extern "C" void * __cdecl ASM_Set_Mouse_Cursor(void *thisptr, int hotspotx, int hotspoty, void *cursor)
{
	WWMouseClass *mouse = (WWMouseClass *)thisptr;
	char *previous;
	unsigned char *storage;
	unsigned char const *shape_data;
	Shape_Type const *shape;
	unsigned char const *cursor_data;
	int width;
	int height;
	int remaining;

	if (!mouse) {
		return(NULL);
	}

	previous = mouse->PrevCursor;
	mouse->PrevCursor = (char *)cursor;
	if (!cursor) {
		return(previous);
	}

	width = Get_Shape_Width(cursor);
	height = Get_Shape_Original_Height(cursor);
	if (width > mouse->MaxWidth || height > mouse->MaxHeight || width <= 0 || height <= 0) {
		return(previous);
	}

	shape_data = Prepare_Cursor_Data(cursor, &storage);
	if (!shape_data) {
		return(previous);
	}

	shape = (Shape_Type const *)shape_data;
	cursor_data = shape_data + ((shape->ShapeType & MAKESHAPE_COMPACT) ? 26 : 10);
	remaining = width * height;
	if (shape->ShapeType & MAKESHAPE_COMPACT) {
		unsigned char const *lookup = shape->Colortable;
		unsigned char *dest = (unsigned char *)mouse->MouseCursor;

		while (remaining > 0) {
			unsigned char value = *cursor_data++;
			if (value != 0) {
				*dest++ = lookup[value];
				--remaining;
			} else {
				int zeros = *cursor_data++;
				memset(dest, 0, zeros);
				dest += zeros;
				remaining -= zeros;
			}
		}
	} else {
		unsigned char *dest = (unsigned char *)mouse->MouseCursor;

		while (remaining > 0) {
			unsigned char value = *cursor_data++;
			if (value != 0) {
				*dest++ = value;
				--remaining;
			} else {
				int zeros = *cursor_data++;
				memset(dest, 0, zeros);
				dest += zeros;
				remaining -= zeros;
			}
		}
	}

	mouse->MouseXHot = hotspotx;
	mouse->MouseYHot = hotspoty;
	mouse->CursorWidth = width;
	mouse->CursorHeight = height;

	delete[] storage;
	return(previous);
}
