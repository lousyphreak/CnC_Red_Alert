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

#include "DIBAPI.H"

#include <cstring>

namespace {

struct BITMAPCOREHEADER {
    DWORD bcSize;
    WORD bcWidth;
    WORD bcHeight;
    WORD bcPlanes;
    WORD bcBitCount;
};

struct RGBTRIPLE {
    BYTE rgbtBlue;
    BYTE rgbtGreen;
    BYTE rgbtRed;
};

constexpr DWORD BI_RGB_VALUE = 0;

bool Is_Win30_DIB(LPCSTR lpDIB)
{
    if (lpDIB == NULL) {
        return false;
    }

    DWORD header_size = 0;
    std::memcpy(&header_size, lpDIB, sizeof(header_size));
    return header_size == sizeof(BITMAPINFOHEADER);
}

DWORD Image_Size(const BITMAPINFOHEADER& header)
{
    if (header.biSizeImage != 0) {
        return header.biSizeImage;
    }

    const DWORD height = header.biHeight < 0 ? static_cast<DWORD>(-header.biHeight) : static_cast<DWORD>(header.biHeight);
    return WIDTHBYTES(static_cast<DWORD>(header.biWidth) * static_cast<DWORD>(header.biBitCount)) * height;
}

} // namespace


LPSTR FAR FindDIBBits(LPCSTR lpDIB)
{
    return const_cast<LPSTR>(lpDIB + *(reinterpret_cast<const DWORD*>(lpDIB)) + PaletteSize(lpDIB));
}


DWORD FAR DIBWidth(LPCSTR lpDIB)
{
    if (lpDIB == NULL) {
        return 0;
    }

    if (Is_Win30_DIB(lpDIB)) {
        return static_cast<DWORD>(reinterpret_cast<const BITMAPINFOHEADER*>(lpDIB)->biWidth);
    }

    return static_cast<DWORD>(reinterpret_cast<const BITMAPCOREHEADER*>(lpDIB)->bcWidth);
}


DWORD FAR DIBHeight(LPCSTR lpDIB)
{
    if (lpDIB == NULL) {
        return 0;
    }

    if (Is_Win30_DIB(lpDIB)) {
        return static_cast<DWORD>(reinterpret_cast<const BITMAPINFOHEADER*>(lpDIB)->biHeight);
    }

    return static_cast<DWORD>(reinterpret_cast<const BITMAPCOREHEADER*>(lpDIB)->bcHeight);
}


WORD FAR DIBNumColors(LPCSTR lpDIB)
{
    if (lpDIB == NULL) {
        return 0;
    }

    if (Is_Win30_DIB(lpDIB)) {
        const BITMAPINFOHEADER* const header = reinterpret_cast<const BITMAPINFOHEADER*>(lpDIB);
        if (header->biClrUsed != 0) {
            return static_cast<WORD>(header->biClrUsed);
        }

        switch (header->biBitCount) {
        case 1:
            return 2;
        case 4:
            return 16;
        case 8:
            return 256;
        default:
            return 0;
        }
    }

    switch (reinterpret_cast<const BITMAPCOREHEADER*>(lpDIB)->bcBitCount) {
    case 1:
        return 2;
    case 4:
        return 16;
    case 8:
        return 256;
    default:
        return 0;
    }
}


WORD FAR PaletteSize(LPCSTR lpDIB)
{
    if (Is_Win30_DIB(lpDIB)) {
        return static_cast<WORD>(DIBNumColors(lpDIB) * sizeof(RGBQUAD));
    }

    return static_cast<WORD>(DIBNumColors(lpDIB) * sizeof(RGBTRIPLE));
}


WORD FAR DestroyDIB(HDIB hDib)
{
    if (hDib != NULL) {
        GlobalFree(hDib);
    }

    return 0;
}


HDIB LoadDIB_FromMemory(const unsigned char* pData, DWORD dwBitsSize)
{
    if (pData == NULL || dwBitsSize < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
        return NULL;
    }

    const unsigned char* const pDataStart = pData;
    const unsigned char* const pDataEnd = pData + dwBitsSize;

    BITMAPFILEHEADER file_header;
    std::memcpy(&file_header, pData, sizeof(file_header));
    if (file_header.bfType != 0x4d42) {
        return NULL;
    }

    pData += sizeof(BITMAPFILEHEADER);
    if (pData + sizeof(BITMAPINFOHEADER) > pDataEnd) {
        return NULL;
    }

    BITMAPINFOHEADER file_info;
    std::memcpy(&file_info, pData, sizeof(file_info));
    if (file_info.biSize != sizeof(BITMAPINFOHEADER) || file_info.biCompression != BI_RGB_VALUE) {
        return NULL;
    }

    DWORD num_colors = file_info.biClrUsed;
    if (num_colors == 0) {
        switch (file_info.biBitCount) {
        case 1:
            num_colors = 2;
            break;
        case 4:
            num_colors = 16;
            break;
        case 8:
            num_colors = 256;
            break;
        default:
            break;
        }
    }
    if (file_info.biClrUsed == 0) {
        file_info.biClrUsed = num_colors;
    }
    file_info.biSizeImage = Image_Size(file_info);

    const DWORD dib_offset = file_info.biSize + num_colors * sizeof(RGBQUAD);
    const DWORD dib_size = dib_offset + file_info.biSizeImage;

    if (pData + file_info.biSize + num_colors * sizeof(RGBQUAD) > pDataEnd) {
        return NULL;
    }

    const unsigned char* bits_data = pData + file_info.biSize + num_colors * sizeof(RGBQUAD);
    if (file_header.bfOffBits != 0) {
        bits_data = pDataStart + file_header.bfOffBits;
    }
    if (bits_data + file_info.biSizeImage > pDataEnd) {
        return NULL;
    }

    HDIB hDIB = GlobalAlloc(GMEM_MOVEABLE, dib_size);
    if (hDIB == NULL) {
        return NULL;
    }

    unsigned char* const dib_data = static_cast<unsigned char*>(GlobalLock(hDIB));
    if (dib_data == NULL) {
        GlobalFree(hDIB);
        return NULL;
    }

    std::memcpy(dib_data, &file_info, sizeof(file_info));
    std::memcpy(dib_data + file_info.biSize, pData + file_info.biSize, num_colors * sizeof(RGBQUAD));
    std::memcpy(dib_data + dib_offset, bits_data, file_info.biSizeImage);

    GlobalUnlock(hDIB);
    return hDIB;
}
