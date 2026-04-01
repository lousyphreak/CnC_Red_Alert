#ifndef RA_DIRECT_WRAPPER_H
#define RA_DIRECT_WRAPPER_H

#include <cctype>
#include <cstring>
#include <limits.h>

#include "sdl_fs.h"

inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	WWFS_SplitPath(path, drive, dir, fname, ext);
}

inline int _mkdir(const char* path)
{
	return WWFS_MakeDirectory(path);
}

inline char* _getcwd(char* buffer, int max_length)
{
	return WWFS_GetCurrentDirectory(buffer, max_length);
}

inline int _chdir(const char* path)
{
	return WWFS_ChangeDirectory(path);
}

inline unsigned WWFS_GetCurrentDriveNumber()
{
	char cwd[PATH_MAX];
	if (WWFS_GetCurrentDirectory(cwd, sizeof(cwd)) != nullptr) {
		const unsigned char drive = static_cast<unsigned char>(cwd[0]);
		if (std::isalpha(drive) && cwd[1] == ':') {
			return static_cast<unsigned>(std::toupper(drive) - 'A' + 1);
		}
	}
	return 3;
}

inline unsigned WWFS_GetDriveCount()
{
	return 26;
}

inline void WWFS_ChangeToDrive(unsigned drive)
{
	if (drive >= 1 && drive <= WWFS_GetDriveCount()) {
		char root[4] = {'A', ':', '\\', '\0'};
		root[0] = static_cast<char>('A' + drive - 1);
		WWFS_ChangeDirectory(root);
	}
}

inline void _makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
	path[0] = '\0';

	if (drive && drive[0] != '\0') {
		std::strcat(path, drive);
	}

	if (dir && dir[0] != '\0') {
		std::strcat(path, dir);
	}

	if (fname && fname[0] != '\0') {
		std::strcat(path, fname);
	}

	if (ext && ext[0] != '\0') {
		if (ext[0] != '.') {
			std::strcat(path, ".");
		}
		std::strcat(path, ext);
	}
}

#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif

#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif

#ifndef _MAX_DIR
#define _MAX_DIR 256
#endif

#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

#endif
