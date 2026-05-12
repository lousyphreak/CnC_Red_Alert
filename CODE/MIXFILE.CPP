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

/* $Header: /CounterStrike/MIXFILE.CPP 2     3/13/97 2:06p Steve_tall $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MIXFILE.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : August 8, 1994                                               *
 *                                                                                             *
 *                  Last Update : July 12, 1996 [JLB]                                          *
 *                                                                                             *
 *                                                                                             *
 *                  Modified by Vic Grippi for WwXlat Tool 10/14/96                            *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   MixFileClass::Cache -- Caches the named mixfile into RAM.                                 *
 *   MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                     *
 *   MixFileClass::Finder -- Finds the mixfile object that matches the name specified.         *
 *   MixFileClass::Free -- Uncaches a cached mixfile.                                          *
 *   MixFileClass::MixFileClass -- Constructor for mixfile object.                             *
 *   MixFileClass::Offset -- Searches in mixfile for matching file and returns offset if found.*
 *   MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                 *
 *   MixFileClass::~MixFileClass -- Destructor for the mixfile object.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include	"BUFF.H"
#include	"FUNCTION.H"
#include "RATRACE.H"
#include "sdl_fs.h"
#include	<cstdarg>
#include	<cstring>
#include	<cstdio>
#include	<cstdlib>
#include	<errno.h>
#include "ENDIAN.H"
#include <SDL3/SDL_log.h>
#include	"MIXFILE.H"
extern MFCD temp;

namespace
{
bool mix_trace_lookup_target(const char* filename)
{
	return filename != NULL
		&& (SDL_strcasecmp(filename, "TEMPERAT.PAL") == 0 || SDL_strcasecmp(filename, "DEBUG.ENG") == 0);
}

int32_t mix_filename_crc(char const* filename)
{
	size_t filename_length = std::strlen(filename);
	char uppercase_filename[512];
	if (filename_length >= sizeof(uppercase_filename)) {
		filename_length = sizeof(uppercase_filename) - 1;
	}
	std::memcpy(uppercase_filename, filename, filename_length);
	uppercase_filename[filename_length] = '\0';
	int32_t crc = static_cast<int32_t>(Calculate_CRC(SDL_strupr(uppercase_filename), filename_length));
	return crc;
}

bool mix_trace_raw_header_target(const char* filename)
{
	return filename != NULL
		&& (SDL_strcasecmp(filename, "EXPAND2.MIX") == 0
			|| SDL_strcasecmp(filename, "REDALERT.MIX") == 0
			|| SDL_strcasecmp(filename, "LOCAL.MIX") == 0
			|| SDL_strcasecmp(filename, "HIRES.MIX") == 0);
}
}


//template<class T> int Compare(T const *obj1, T const *obj2) {
//	if (*obj1 < *obj2) return(-1);
//	if (*obj1 > *obj2) return(1);
//	return(0);
//};


/*
**	This is the pointer to the first mixfile in the list of mixfiles registered
**	with the mixfile system.
*/
template<class T>
List<MixFileClass<T> > MixFileClass<T>::MixList;


/***********************************************************************************************
 * MixFileClass::Free -- Uncaches a cached mixfile.                                            *
 *                                                                                             *
 *    Use this routine to uncache a mixfile that has been cached.                              *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the mixfile that is to be uncached.         *
 *                                                                                             *
 * OUTPUT:  bool; Was the mixfile found and freed?                                             *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
template<class T>
bool MixFileClass<T>::Free(char const * filename)
{
	MixFileClass * ptr = Finder(filename);

	if (ptr) {
		ptr->Free();
		return(true);
	}
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Promote -- Moves this mixfile to the front of lookup priority.                 *
 *                                                                                             *
 *    Use this routine when a newly registered mixfile should shadow older registrations with   *
 *    the same logical filenames without destroying those older registrations.                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   04/11/2026 GPT : Created.                                                                 *
 *=============================================================================================*/
template<class T>
void MixFileClass<T>::Promote(void)
{
	if (!this->Is_Valid()) {
		return;
	}

	this->Unlink();
	MixList.Add_Head(this);
}


/***********************************************************************************************
 * MixFileClass::~MixFileClass -- Destructor for the mixfile object.                           *
 *                                                                                             *
 *    This destructor will free all memory allocated by this mixfile and will remove it from   *
 *    the system. A mixfile removed in this fashion must be created anew in order to be        *
 *    subsequent used.                                                                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   01/06/1995 JLB : Puts mixfile header table into EMS.                                      *
 *=============================================================================================*/
template<class T>
MixFileClass<T>::~MixFileClass(void)
{
	/*
	**	Deallocate any allocated memory.
	*/
	if (Filename) {
		free((char *)Filename);
	}
	if (Data != NULL && IsAllocated) {
		delete [] static_cast<char *>(Data);
		IsAllocated = false;
	}
	Data = NULL;

	if (HeaderBuffer != NULL) {
		delete [] HeaderBuffer;
		HeaderBuffer = NULL;
	}

	/*
	**	Unlink this mixfile object from the chain.
	*/
	this->Unlink();
}


/***********************************************************************************************
 * MixFileClass::MixFileClass -- Constructor for mixfile object.                               *
 *                                                                                             *
 *    This is the constructor for the mixfile object. It takes a filename and a memory         *
 *    handler object and registers the mixfile object with the system. The index block is      *
 *    allocated and loaded from disk by this routine.                                          *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the mixfile object.                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   07/12/1996 JLB : Handles compressed file header.                                          *
 *=============================================================================================*/
template<class T>
MixFileClass<T>::MixFileClass(char const * filename, PKey const * key) :
	Filename(0),
	IsDigest(false),
	IsEncrypted(false),
	IsAllocated(false),
	Count(0),
	DataSize(0),
	DataStart(0),
	HeaderBuffer(0),
	Data(0)
{
	/*
	**	Check to see if the file is available. If it isn't, then
	**	no further processing is needed or possible.
	*/
	T file(filename);		// Working file object.
	Filename = strdup(file.File_Name());
	FileStraw fstraw(file);
	PKStraw pstraw(PKStraw::DECRYPT, CryptRandom);
	Straw * straw = &fstraw;

	if (!file.Is_Available()) return;

	/*
	**	Stuctures used to hold the various file headers.
	*/
	FileHeader fileheader;
	struct {
		int16_t First;		// Always zero for extended mixfile format.
		int16_t Second;		// Bitfield of extensions to this mixfile.
	} alternate = {};
	FileHeader raw_fileheader = {};
	const int32_t mixfile_size = file.Size();

	/*
	**	Fetch the first bit of the file. From this bit, it is possible to detect
	**	whether this is an extended mixfile format or the plain format. An
	**	extended format may have extra options or data layout.
	*/
	if (straw->Get(&alternate, sizeof(alternate)) != sizeof(alternate)) {
		RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s failed to read alternate header", filename);
		return;
	}

	const int16_t alternate_first = static_cast<int16_t>(SDL_Swap16LE(static_cast<uint16_t>(alternate.First)));
	const int16_t alternate_second = static_cast<int16_t>(SDL_Swap16LE(static_cast<uint16_t>(alternate.Second)));

	/*
	**	Detect if this is an extended mixfile. If so, then see if it is encrypted
	**	and/or has a message digest attached. Otherwise, just retrieve the
	**	plain mixfile header.
	*/
	if (alternate_first == 0) {
		IsDigest = ((alternate_second & 0x01) != 0);
		IsEncrypted = ((alternate_second & 0x02) != 0);

		if (IsEncrypted) {
			pstraw.Key(key);
			pstraw.Get_From(&fstraw);
			straw = &pstraw;
		}
		if (straw->Get(&raw_fileheader, sizeof(raw_fileheader)) != sizeof(raw_fileheader)) {
			RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s failed to read file header", filename);
			return;
		}

	} else {
		memmove(&raw_fileheader, &alternate, sizeof(alternate));
		if (straw->Get(((char*)&raw_fileheader)+sizeof(alternate), sizeof(raw_fileheader)-sizeof(alternate))
			!= sizeof(raw_fileheader)-sizeof(alternate)) {
			RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s failed to read legacy file header tail", filename);
			return;
		}
	}

	if (mix_trace_raw_header_target(filename)) {
		const uint8_t* raw_header_bytes = reinterpret_cast<const uint8_t*>(&raw_fileheader);
		RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s raw_fileheader=%02x %02x %02x %02x %02x %02x",
			filename,
			raw_header_bytes[0],
			raw_header_bytes[1],
			raw_header_bytes[2],
			raw_header_bytes[3],
			raw_header_bytes[4],
			raw_header_bytes[5]);
	}

	fileheader.count = static_cast<int16_t>(SDL_Swap16LE(static_cast<uint16_t>(raw_fileheader.count)));
	fileheader.size = static_cast<int32_t>(SDL_Swap32LE(static_cast<uint32_t>(raw_fileheader.size)));
	Count = fileheader.count;
	DataSize = fileheader.size;
	RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s alt=(%d,%d) count=%d datasize=%ld fileheader=%zu subblock=%zu encrypted=%d",
		filename,
		alternate_first,
		alternate_second,
		Count,
		DataSize,
		sizeof(fileheader),
		sizeof(SubBlock),
		IsEncrypted);

	if (Count < 0 || DataSize < 0 || Count > (mixfile_size / static_cast<int32_t>(sizeof(SubBlock)))) {
		RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s invalid mix header count=%d datasize=%ld file_size=%ld", filename, Count, DataSize, mixfile_size);
		Count = 0;
		DataSize = 0;
		return;
	}
//BGMono_Printf("Mixfileclass %s DataSize: %08x   \n",filename,DataSize);Get_Key();
	/*
	**	Load up the offset control array. If RAM is exhausted, then the mixfile is invalid.
	*/
	HeaderBuffer = new SubBlock [Count];
	if (HeaderBuffer == NULL) return;
	const int32_t header_bytes = Count * static_cast<int32_t>(sizeof(SubBlock));
	if (straw->Get(HeaderBuffer, header_bytes) != header_bytes) {
		delete [] HeaderBuffer;
		HeaderBuffer = NULL;
		Count = 0;
		DataSize = 0;
		RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s failed to read %d subblocks", filename, Count);
		return;
	}

	if (mix_trace_raw_header_target(filename) && Count > 0) {
		const uint8_t* raw_subblock_bytes = reinterpret_cast<const uint8_t*>(&HeaderBuffer[0]);
		RA_Trace("RA_TRACE_STARTUP", "mixfile", "%s raw_subblock0=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			filename,
			raw_subblock_bytes[0],
			raw_subblock_bytes[1],
			raw_subblock_bytes[2],
			raw_subblock_bytes[3],
			raw_subblock_bytes[4],
			raw_subblock_bytes[5],
			raw_subblock_bytes[6],
			raw_subblock_bytes[7],
			raw_subblock_bytes[8],
			raw_subblock_bytes[9],
			raw_subblock_bytes[10],
			raw_subblock_bytes[11]);
	}

	for (int index = 0; index < Count; ++index) {
		HeaderBuffer[index].CRC = static_cast<int32_t>(SDL_Swap32LE(static_cast<uint32_t>(HeaderBuffer[index].CRC)));
		HeaderBuffer[index].Offset = static_cast<int32_t>(SDL_Swap32LE(static_cast<uint32_t>(HeaderBuffer[index].Offset)));
		HeaderBuffer[index].Size = static_cast<int32_t>(SDL_Swap32LE(static_cast<uint32_t>(HeaderBuffer[index].Size)));

		if (HeaderBuffer[index].Offset < 0 || HeaderBuffer[index].Size < 0
			|| HeaderBuffer[index].Offset > DataSize
			|| HeaderBuffer[index].Size > (DataSize - HeaderBuffer[index].Offset)) {

			delete [] HeaderBuffer;
			HeaderBuffer = NULL;
			Count = 0;
			DataSize = 0;
			return;
		}
	}

	if (SDL_strcasecmp(filename, "HIRES.MIX") == 0 || SDL_strcasecmp(filename, "LOCAL.MIX") == 0) {
		const char* probe_name = (SDL_strcasecmp(filename, "HIRES.MIX") == 0) ? "TEMPERAT.PAL" : "DEBUG.ENG";
		const int32_t probe_crc = mix_filename_crc(probe_name);
		int probe_matches = 0;
		int probe_index = -1;
		for (int index = 0; index < Count; ++index) {
			if (HeaderBuffer[index].CRC == probe_crc) {
				++probe_matches;
				if (probe_index < 0) {
					probe_index = index;
				}
			}
		}
	}

	/*
	**	The start of the embedded mixfile data will be at the current file offset.
	**	This should be true even if the file header has been encrypted because the file
	**	header was cleverly written with just the sufficient number of padding bytes so
	**	that this condition would be true.
	*/
	DataStart = file.Seek(0, SEEK_CUR) + file.BiasStart;
//	DataStart = file.Seek(0, SEEK_CUR);

	/*
	**	Attach to list of mixfiles.
	*/
	MixList.Add_Tail(this);
}


/***********************************************************************************************
 * MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                   *
 *                                                                                             *
 *    This routine will return with a pointer to the specified data file if the file resides   *
 *    in memory. Otherwise, this routine returns NULL. Use this routine to access a resident   *
 *    file directly rather than going through the process of pseudo disk access. This will     *
 *    save both time and RAM.                                                                  *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the data file to retrieve a pointer to.     *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the data file's data. If the file is not in RAM, then    *
 *          NULL is returned.                                                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/23/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
template<class T>
void const * MixFileClass<T>::Retrieve(char const * filename)
{
	void * ptr = 0;
	Offset(filename, &ptr);
	return(ptr);
};


/***********************************************************************************************
 * MixFileClass::Finder -- Finds the mixfile object that matches the name specified.           *
 *                                                                                             *
 *    This routine will scan through all registered mixfiles and return with a pointer to      *
 *    the matching mixfile. If no mixfile could be found that matches the name specified,      *
 *    then NULL is returned.                                                                   *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename to search for.                                 *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the matching mixfile -- if found.                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   06/08/1996 JLB : Only compares filename and extension.                                    *
 *=============================================================================================*/
template<class T>
MixFileClass<T> * MixFileClass<T>::Finder(char const * filename)
{
	MixFileClass<T> * ptr = MixList.First();
	while (ptr->Is_Valid()) {
		char path[260];
		char name[_MAX_FNAME];
		char ext[_MAX_EXT];

		/*
		**	Strip the drive and path (if present) off of the filename
		**	in the mixfile list. This enables a simple comparison to the
		**	filename specified. The filename specified won't have a path attached and
		**	the full pathname in the mixfile list WILL have a path attached. Hence, this
		** stripping of the path is necessary.
		*/
		WWFS_SplitPath(ptr->Filename, NULL, NULL, name, ext);
		WWFS_MakePath(path, NULL, NULL, name, ext);

		if (SDL_strcasecmp(path, filename) == 0) {
			return(ptr);
		}
		ptr = ptr->Next();
	}
	return(0);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Caches the named mixfile into RAM.                                   *
 *                                                                                             *
 *    This routine will cache the mixfile, specified by name, into RAM.                        *
 *                                                                                             *
 * INPUT:   filename -- The name of the mixfile that should be cached.                         *
 *                                                                                             *
 * OUTPUT:  bool; Was the cache successful?                                                    *
 *                                                                                             *
 * WARNINGS:   This routine could go to disk for a very long time.                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
template<class T>
bool MixFileClass<T>::Cache(char const * filename, Buffer const * buffer)
{
	MixFileClass<T> * mixer = Finder(filename);

	if (mixer != NULL) {
		return(mixer->Cache(buffer));
	}
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                       *
 *                                                                                             *
 *    This load the mixfile data into ram for this mixfile object. This is the counterpart     *
 *    to the Free() function.                                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Was the file load successful?  It could fail if there wasn't enough room     *
 *                to allocate the raw data block.                                              *
 *                                                                                             *
 * WARNINGS:   This routine goes to disk for a potentially very long time.                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   07/12/1996 JLB : Handles attached message digest.                                         *
 *=============================================================================================*/
template<class T>
bool MixFileClass<T>::Cache(Buffer const * buffer)
{
	RA_Trace("RA_TRACE_STARTUP", "mixfile", "Cache begin mix=%s this=%p data=%p allocated=%d size=%d start=%d",
		Filename ? Filename : "<null>",
		this,
		Data,
		IsAllocated,
		DataSize,
		DataStart);
	/*
	**	If the mixfile is already cached, then no action needs to be performed.
	*/
	if (Data != NULL) return(true);

	/*
	**	If a buffer was supplied (and it is big enough), then use it as the data block
	**	pointer. Otherwise, the data block must be allocated.
	*/
	if (buffer != NULL) {
		if (buffer->Get_Size() == 0 || buffer->Get_Size() >= DataSize) {
			Data = buffer->Get_Buffer();
		}
	} else {
		Data = new char [DataSize];
		IsAllocated = true;
	}

	/*
	**	If there is a data buffer to fill, then fill it now.
	*/
	if (Data != NULL) {
		T file(Filename);

		FileStraw fstraw(file);
		Straw * straw = &fstraw;

		/*
		**	If a message digest is attached, then link a SHA straw segment to the data
		**	stream so that the actual SHA can be compared with the attached one.
		*/
		SHAStraw sha;
		if (IsDigest) {
			sha.Get_From(fstraw);
			straw = &sha;
		}

		/*
		**	Bias the file to the actual start of the data. This is necessary because the
		**	real data starts some distance (not so easily determined) from the beginning of
		**	the real file.
		*/
		file.Open(READ);
		file.Bias(0);
		file.Bias(DataStart);

		/*
		**	Fetch the whole mixfile data in one step. If the number of bytes retrieved
		**	does not equal that requested, then this indicates a serious error.
		*/
		int32_t actual = straw->Get(Data, DataSize);
		if (actual != DataSize) {
			delete [] static_cast<char *>(Data);
			Data = NULL;
			file.Error(EIO);
			return(false);
		}

		/*
		**	If there is a digest attached to this mixfile, then read it in and
		**	compare it to the generated digest. If they don't match, then
		**	return with the "failure to cache" error code.
		*/
		if (IsDigest) {
			char digest1[20];
			char digest2[20];
			sha.Result(digest2);
			fstraw.Get(digest1, sizeof(digest1));
			if (memcmp(digest1, digest2, sizeof(digest1)) != 0) {
				delete [] static_cast<char *>(Data);
				Data = NULL;
				return(false);
			}
		}

		return(true);
	}
	IsAllocated = false;
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Free -- Frees the allocated raw data block (not the index block).             *
 *                                                                                             *
 *    This routine will free the (presumably large) raw data block, but leave the index        *
 *    block intact. By using this in conjunction with the Cache() function, one can maintain   *
 *    tight control of memory usage. If the index block is desired to be freed, then the       *
 *    mixfile object must be deleted.                                                          *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
template<class T>
void MixFileClass<T>::Free(void)
{
	RA_Trace("RA_TRACE_STARTUP", "mixfile", "Free mix=%s this=%p data=%p allocated=%d", Filename ? Filename : "<null>", this, Data, IsAllocated);
	if (Data != NULL && IsAllocated) {
		delete [] static_cast<char *>(Data);
	}
	Data = NULL;
	IsAllocated = false;
}


int compfunc(void const * ptr1, void const * ptr2)
{
	if (*(int32_t const *)ptr1 < *(int32_t const *)ptr2) return(-1);
	if (*(int32_t const *)ptr1 > *(int32_t const *)ptr2) return(1);
	return(0);
}


/***********************************************************************************************
 * MixFileClass::Offset -- Determines the offset of the requested file from the mixfile system.*
 *                                                                                             *
 *    This routine will take the filename specified and search through the mixfile system      *
 *    looking for it. If the file was found, then the mixfile it was found in, the offset      *
 *    from the start of the mixfile, and the size of the embedded file will be returned.       *
 *    Using this method it is possible for the CCFileClass system to process it as a normal    *
 *    file.                                                                                    *
 *                                                                                             *
 * INPUT:   filename    -- The filename to search for.                                         *
 *                                                                                             *
 *          realptr     -- Stores a pointer to the start of the file in memory here. If the    *
 *                         file is not in memory, then NULL is stored here.                    *
 *                                                                                             *
 *          mixfile     -- The pointer to the corresponding mixfile is placed here. If no      *
 *                         mixfile was found that contains the file, then NULL is stored here. *
 *                                                                                             *
 *          offset      -- The starting offset from the beginning of the parent mixfile is     *
 *                         stored here.                                                        *
 *                                                                                             *
 *          size        -- The size of the embedded file is stored here.                       *
 *                                                                                             *
 * OUTPUT:  bool; Was the file found? The file may or may not be resident, but it does exist   *
 *                 and can be opened.                                                           *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/17/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
template<class T>
bool MixFileClass<T>::Offset(char const * filename, void ** realptr, MixFileClass ** mixfile, int32_t * offset, int32_t * size)
{
	MixFileClass<T> * ptr;

	if (filename == NULL) {
assert(filename != NULL);//BG
		return(false);
	}

	/*
	**	Create the key block that will be used to binary search for the file.
	*/
	int32_t crc = mix_filename_crc(filename);
	SubBlock key;
	key.CRC = crc;
	
	/*
	**	Sweep through all registered mixfiles, trying to find the file in question.
	*/
	ptr = MixList.First();
	while (ptr->Is_Valid()) {
		SubBlock * block;

		/*
		**	Binary search for the file in this mixfile. If it is found, then extract the
		**	appropriate information and store it in the locations provided and then return.
		*/
		block = NULL;
		if (ptr->HeaderBuffer != NULL && ptr->Count > 0) {
			SubBlock *blocks = static_cast<SubBlock *>(ptr->HeaderBuffer);
			int first = 0;
			int last = ptr->Count - 1;
			while (first <= last) {
				int middle = first + ((last - first) / 2);
				if (key.CRC < blocks[middle].CRC) {
					last = middle - 1;
				} else if (key.CRC > blocks[middle].CRC) {
					first = middle + 1;
				} else {
					block = &blocks[middle];
					break;
				}
			}
		}

		if (block != NULL) {
			if (mixfile != NULL) *mixfile = ptr;
			if (size != NULL) *size = block->Size;
			if (realptr != NULL) *realptr = NULL;
			if (offset != NULL) *offset = block->Offset;
			if (realptr != NULL && ptr->Data != NULL) {
				*realptr = (char *)ptr->Data + block->Offset;
			}
			if (ptr->Data == NULL && offset != NULL) {
				*offset += ptr->DataStart;
			}
			return(true);
		}

		/*
		**	Advance to next mixfile.
		*/
		ptr = ptr->Next();
	}

	/*
	**	All the mixfiles have been examined but no match was found. Return with the non success flag.
	*/
assert(1);//BG
	return(false);
}

template class MixFileClass<CCFileClass>;
