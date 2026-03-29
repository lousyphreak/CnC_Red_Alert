#ifndef RA_DIRECT_WRAPPER_H
#define RA_DIRECT_WRAPPER_H

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

inline int _mkdir(const char* path)
{
	return mkdir(path, 0777);
}

inline char* _getcwd(char* buffer, int max_length)
{
	return getcwd(buffer, max_length);
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

#ifndef _MAX_EXT
#define _MAX_EXT 256
#endif

#endif
