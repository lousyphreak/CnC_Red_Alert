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

//*******************************************************************
//
//  file.c
//
//  Source file for Device-Independent Bitmap (DIB) API.  Provides
//  the following functions:
//
//  SaveDIB()           - Saves the specified dib in a file
//  LoadDIB()           - Loads a DIB from a file
//  DestroyDIB()        - Deletes DIB when finished using it
//
// Development Team: Mark Bader
//                   Patrick Schreiber
//                   Garrett McAuliffe
//                   Eric Flo
//                   Tony Claflin
//
// Written by Microsoft Product Support Services, Developer Support.
// COPYRIGHT:
//
//   (C) Copyright Microsoft Corp. 1993.  All rights reserved.
//
//   You have a royalty-free right to use, modify, reproduce and
//   distribute the Sample Files (and/or any modified version) in
//   any way you find useful, provided that you agree that
//   Microsoft has no warranty obligations or liability for any
//   Sample Application Files which are modified.
//
//*******************************************************************

#include "win32_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DIBAPI.H"

//#include "WOLDEBUG.H"

/*
 * Dib Header Marker - used in writing DIBs to files
 */
#define DIB_HEADER_MARKER   ((WORD) ('M' << 8) | 'B')

/*********************************************************************
 *
 * Local Function Prototypes
 *
 *********************************************************************/


namespace {

struct BITMAPCOREHEADER {
   DWORD bcSize;
   WORD bcWidth;
   WORD bcHeight;
   WORD bcPlanes;
   WORD bcBitCount;
};

constexpr DWORD BI_RLE8_VALUE = 1;
constexpr DWORD BI_RLE4_VALUE = 2;

bool ReadExact(SDL_IOStream* stream, void* buffer, size_t size)
{
   return stream != NULL && SDL_ReadIO(stream, buffer, size) == size;
}

bool WriteExact(SDL_IOStream* stream, const void* buffer, size_t size)
{
   return stream != NULL && SDL_WriteIO(stream, buffer, size) == size;
}

} // namespace

HDIB ReadDIBFile(SDL_IOStream*);
BOOL MyRead(SDL_IOStream*, LPSTR, DWORD);
DWORD PASCAL MyWrite(SDL_IOStream*, const VOID FAR *, DWORD);

/*************************************************************************
 *
 * LoadDIB()
 *
 * Loads the specified DIB from a file, allocates memory for it,
 * and reads the disk file into the memory.
 *
 *
 * Parameters:
 *
 * LPSTR lpFileName - specifies the file to load a DIB from
 *
 * Returns: A handle to a DIB, or NULL if unsuccessful.
 *
 * NOTE: The DIB API were not written to handle OS/2 DIBs; This
 * function will reject any file that is not a Windows DIB.
 *
 * History:   Date      Author       Reason
 *            9/15/91   Mark Bader   Based on DIBVIEW
 *
 *************************************************************************/


HDIB FAR LoadDIB(LPSTR lpFileName)
{
   HDIB hDIB;
   SDL_IOStream* hFile;

   /*
    * Set the cursor to a hourglass, in case the loading operation
    * takes more than a sec, the user will know what's going on.
    */

   SetCursor(LoadCursor(NULL, NULL));
   if ((hFile = SDL_IOFromFile(lpFileName, "rb")) != NULL)
   {
      hDIB = ReadDIBFile(hFile);
      SDL_CloseIO(hFile);
      SetCursor(LoadCursor(NULL, NULL));
      return hDIB;
   }
   else
   {
//      DIBError(ERR_FILENOTFOUND);
      SetCursor(LoadCursor(NULL, NULL));
      return NULL;
   }
}


/*************************************************************************
 *
 * SaveDIB()
 *
 * Saves the specified DIB into the specified file name on disk.  No
 * error checking is done, so if the file already exists, it will be
 * written over.
 *
 * Parameters:
 *
 * HDIB hDib - Handle to the dib to save
 *
 * LPSTR lpFileName - pointer to full pathname to save DIB under
 *
 * Return value: 0 if successful, or one of:
 *        ERR_INVALIDHANDLE
 *        ERR_OPEN
 *        ERR_LOCK
 *
 * History:
 *
 * NOTE: The DIB API were not written to handle OS/2 DIBs, so this
 * function will not save a file if it is not a Windows DIB.
 *
 * History:   Date      Author       Reason
 *            9/15/91   Mark Bader   Taken from DIBVIEW (which was taken
 *                                      from SHOWDIB)
 *            1/30/92   Mark Bader   Fixed problem of writing too many 
 *                                      bytes to the file
 *            6/24/92   Mark Bader   Added check for OS/2 DIB
 *
 *************************************************************************/


WORD FAR SaveDIB(HDIB hDib, LPSTR lpFileName)
{
   BITMAPFILEHEADER bmfHdr; // Header for Bitmap file
   LPBITMAPINFOHEADER lpBI;   // Pointer to DIB info structure
   SDL_IOStream* fh;     // file handle for opened file
   DWORD dwDIBSize;
   DWORD dwError;   // Error return from MyWrite

   if (!hDib)
      return ERR_INVALIDHANDLE;
   fh = SDL_IOFromFile(lpFileName, "wb");
   if (fh == NULL)
      return ERR_OPEN;

   /*
    * Get a pointer to the DIB memory, the first of which contains
    * a BITMAPINFO structure
    */
   lpBI = (LPBITMAPINFOHEADER)GlobalLock(hDib);
   if (!lpBI) {
      SDL_CloseIO(fh);
      return ERR_LOCK;
   }

   // Check to see if we're dealing with an OS/2 DIB.  If so, don't
   // save it because our functions aren't written to deal with these
   // DIBs.

   if (lpBI->biSize != sizeof(BITMAPINFOHEADER))
   {
      GlobalUnlock(hDib);
      SDL_CloseIO(fh);
      return ERR_NOT_DIB;
   }

   /*
    * Fill in the fields of the file header
    */

   /* Fill in file type (first 2 bytes must be "BM" for a bitmap) */
   bmfHdr.bfType = DIB_HEADER_MARKER;  // "BM"

   // Calculating the size of the DIB is a bit tricky (if we want to
   // do it right).  The easiest way to do this is to call GlobalSize()
   // on our global handle, but since the size of our global memory may have
   // been padded a few bytes, we may end up writing out a few too
   // many bytes to the file (which may cause problems with some apps,
   // like HC 3.0).
   //
   // So, instead let's calculate the size manually.
   //
   // To do this, find size of header plus size of color table.  Since the
   // first DWORD in both BITMAPINFOHEADER and BITMAPCOREHEADER conains
   // the size of the structure, let's use this.

   dwDIBSize = *(LPDWORD)lpBI + PaletteSize((LPSTR)lpBI);  // Partial Calculation

   // Now calculate the size of the image

   if ((lpBI->biCompression == BI_RLE8_VALUE) || (lpBI->biCompression == BI_RLE4_VALUE)) {

      // It's an RLE bitmap, we can't calculate size, so trust the
      // biSizeImage field

      dwDIBSize += lpBI->biSizeImage;
      }
   else {
      DWORD dwBmBitsSize;  // Size of Bitmap Bits only

      // It's not RLE, so size is Width (DWORD aligned) * Height

      dwBmBitsSize = WIDTHBYTES((lpBI->biWidth)*((DWORD)lpBI->biBitCount)) * lpBI->biHeight;

      dwDIBSize += dwBmBitsSize;
      
      // Now, since we have calculated the correct size, why don't we
      // fill in the biSizeImage field (this will fix any .BMP files which 
      // have this field incorrect).

      lpBI->biSizeImage = dwBmBitsSize;
      }


   // Calculate the file size by adding the DIB size to sizeof(BITMAPFILEHEADER)
                   
   bmfHdr.bfSize = dwDIBSize + sizeof(BITMAPFILEHEADER);
   bmfHdr.bfReserved1 = 0;
   bmfHdr.bfReserved2 = 0;

   /*
    * Now, calculate the offset the actual bitmap bits will be in
    * the file -- It's the Bitmap file header plus the DIB header,
    * plus the size of the color table.
    */
   bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + lpBI->biSize +
                      PaletteSize((LPSTR)lpBI);

    /* Write the file header */
   dwError = WriteExact(fh, &bmfHdr, sizeof(BITMAPFILEHEADER))
       ? MyWrite(fh, lpBI, dwDIBSize)
       : 0;

   /*
    * Write the DIB header and the bits -- use local version of
    * MyWrite, so we can write more than 32767 bytes of data
    */
   GlobalUnlock(hDib);
   if (!SDL_CloseIO(fh))
      dwError = 0;

   if (dwError == 0)
     return ERR_OPEN; // oops, something happened in the write
   else
     return 0; // Success code
}


/*************************************************************************
 *
 * DestroyDIB ()
 *
 * Purpose:  Frees memory associated with a DIB
 *
 * Returns:  Nothing
 *
 * History:   Date      Author       Reason
 *            9/15/91   Mark Bader   Created
 *
 *************************************************************************/


WORD FAR DestroyDIB(HDIB hDib)
{
   GlobalFree(hDib);
   return 0;
}


//************************************************************************
//
// Auxiliary Functions which the above procedures use
//
//************************************************************************


/*************************************************************************
 *
 * Function:  ReadDIBFile (int)
 *
 *  Purpose:  Reads in the specified DIB file into a global chunk of
 *            memory.
 *
 *  Returns:  A handle to a dib (hDIB) if successful.
 *            NULL if an error occurs.
 *
 * Comments:  BITMAPFILEHEADER is stripped off of the DIB.  Everything
 *            from the end of the BITMAPFILEHEADER structure on is
 *            returned in the global memory handle.
 *
 *
 * NOTE: The DIB API were not written to handle OS/2 DIBs, so this
 * function will reject any file that is not a Windows DIB.
 *
 * History:   Date      Author       Reason
 *            9/15/91   Mark Bader   Based on DIBVIEW
 *            6/25/92   Mark Bader   Added check for OS/2 DIB
 *            7/21/92   Mark Bader   Added code to deal with bfOffBits
 *                                     field in BITMAPFILEHEADER      
 *            9/11/92   Mark Bader   Fixed Realloc Code to free original mem
 *
 *************************************************************************/

HDIB ReadDIBFile(SDL_IOStream* hFile)
{
   BITMAPFILEHEADER bmfHeader;
   UINT nNumColors;   // Number of colors in table
   HANDLE hDIB;        
   HANDLE hDIBtmp;    // Used for GlobalRealloc() //MPB
   LPBITMAPINFOHEADER lpbi;
   DWORD offBits;
   BITMAPINFOHEADER header_copy;

   // Allocate memory for header & color table.	We'll enlarge this
   // memory as needed.

   hDIB = GlobalAlloc(GMEM_MOVEABLE,
       (DWORD)(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)));
   
   if (!hDIB) return NULL;

   lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIB);
   if (!lpbi) 
   {
     GlobalFree(hDIB);
     return NULL;
   }

   // read the BITMAPFILEHEADER from our file

   if (!ReadExact(hFile, &bmfHeader, sizeof(BITMAPFILEHEADER)))
      goto ErrExit;

   if (bmfHeader.bfType != 0x4d42)	/* 'BM' */
     goto ErrExit;

   // read the BITMAPINFOHEADER

   if (!ReadExact(hFile, lpbi, sizeof(BITMAPINFOHEADER)))
      goto ErrExit;

   // Check to see that it's a Windows DIB -- an OS/2 DIB would cause
   // strange problems with the rest of the DIB API since the fields
   // in the header are different and the color table entries are
   // smaller.
   //
   // If it's not a Windows DIB (e.g. if biSize is wrong), return NULL.

   if (lpbi->biSize == sizeof(BITMAPCOREHEADER))
     goto ErrExit;

   // Now determine the size of the color table and read it.  Since the
   // bitmap bits are offset in the file by bfOffBits, we need to do some
   // special processing here to make sure the bits directly follow
   // the color table (because that's the format we are susposed to pass
   // back)
   nNumColors = (UINT)lpbi->biClrUsed;
   if (!nNumColors)
    {
      // no color table for 24-bit, default size otherwise
      if (lpbi->biBitCount != 24)
        nNumColors = 1 << lpbi->biBitCount; /* standard size table */
    }

   // fill in some default values if they are zero
   if (lpbi->biClrUsed == 0)
     lpbi->biClrUsed = nNumColors;

   if (lpbi->biSizeImage == 0)
   {
     lpbi->biSizeImage = ((((lpbi->biWidth * (DWORD)lpbi->biBitCount) + 31) & ~31) >> 3)
			 * lpbi->biHeight;
   }

   // get a proper-sized buffer for header, color table and bits
   header_copy = *lpbi;
   GlobalUnlock(hDIB);
   hDIBtmp = GlobalAlloc(GMEM_MOVEABLE, header_copy.biSize +
                        nNumColors * sizeof(RGBQUAD) +
                        header_copy.biSizeImage);

   if (!hDIBtmp) // can't resize buffer for loading
      goto ErrExitNoUnlock; //MPB

   lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDIBtmp);
   if (!lpbi) {
      GlobalFree(hDIBtmp);
      goto ErrExitNoUnlock;
   }
   memcpy(lpbi, &header_copy, sizeof(header_copy));
   GlobalFree(hDIB);
   hDIB = hDIBtmp;

   // read the color table
   if (!ReadExact(hFile, (LPSTR)(lpbi) + lpbi->biSize, nNumColors * sizeof(RGBQUAD)))
      goto ErrExit;

   // offset to the bits from start of DIB header
   offBits = lpbi->biSize + nNumColors * sizeof(RGBQUAD);

   // If the bfOffBits field is non-zero, then the bits might *not* be
   // directly following the color table in the file.  Use the value in
   // bfOffBits to seek the bits.

   if (bmfHeader.bfOffBits != 0L) {
      if (SDL_SeekIO(hFile, bmfHeader.bfOffBits, SDL_IO_SEEK_SET) < 0)
         goto ErrExit;
   }
   
   if (MyRead(hFile, (LPSTR)lpbi + offBits, lpbi->biSizeImage))
     goto OKExit;


ErrExit:
    GlobalUnlock(hDIB);    
ErrExitNoUnlock:    
    GlobalFree(hDIB);
    return NULL;

OKExit:
    GlobalUnlock(hDIB);
    return hDIB;
}

/*************************************************************************

  Function:  MyRead (SDL_IOStream*, LPSTR, DWORD)

   Purpose:  Routine to read files greater than 64K in size.

   Returns:  TRUE if successful.
             FALSE if an error occurs.

  
  History:   Date      Author       Reason
             9/15/91   Mark Bader   Based on DIBVIEW
 
*************************************************************************/


BOOL MyRead(SDL_IOStream* hFile, LPSTR lpBuffer, DWORD dwSize)
{
   char *lpInBuf = lpBuffer;
   int nBytes;

   /*
    * Read in the data in 32767 byte chunks (or a smaller amount if it's
    * the last chunk of data read)
    */

   while (dwSize)
   {
      nBytes = (int)(dwSize > (DWORD)32767 ? 32767 : LOWORD (dwSize));
      if (!ReadExact(hFile, lpInBuf, (size_t)nBytes))
         return FALSE;
      dwSize -= nBytes;
      lpInBuf += nBytes;
   }
   return TRUE;
}


/****************************************************************************

 FUNCTION   : MyWrite(SDL_IOStream* fh, const VOID FAR *pv, DWORD ul)

 PURPOSE    : Writes data in steps of 32k till all the data is written.
              This preserves the old chunked-write behavior while using
              SDL I/O instead of the legacy `_lwrite` API.

 RETURNS    : 0 - If write did not proceed correctly.
              number of bytes written otherwise.
 
  History:   Date      Author       Reason
             9/15/91   Mark Bader   Based on DIBVIEW

 ****************************************************************************/


DWORD PASCAL MyWrite(SDL_IOStream* iFileHandle, const VOID FAR *lpBuffer, DWORD dwBytes)
{
   DWORD dwBytesTmp = dwBytes;       // Save # of bytes for return value
   const BYTE *hpBuffer = (const BYTE *)lpBuffer;   // make a pointer to the data

   /*
    * Write out the data in 32767 byte chunks.
    */

   while (dwBytes > 32767)
   {
      if (!WriteExact(iFileHandle, hpBuffer, 32767))
         return 0;
      dwBytes -= 32767;
      hpBuffer += 32767;
   }

   /* Write out the last chunk (which is < 32767 bytes) */
   if (!WriteExact(iFileHandle, hpBuffer, dwBytes))
      return 0;
   return dwBytesTmp;
}
