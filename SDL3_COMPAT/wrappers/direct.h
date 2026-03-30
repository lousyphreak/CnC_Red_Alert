#ifndef RA_DIRECT_WRAPPER_H
#define RA_DIRECT_WRAPPER_H

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	if (drive) drive[0] = '\0';
	if (dir) dir[0] = '\0';
	if (fname) fname[0] = '\0';
	if (ext) ext[0] = '\0';
	if (!path) return;

	const char* start = path;
	const char* colon = std::strchr(path, ':');
	if (colon) {
		if (drive) {
			const size_t drive_len = static_cast<size_t>(colon - path + 1);
			std::memcpy(drive, path, drive_len);
			drive[drive_len] = '\0';
		}
		start = colon + 1;
	}

	const char* slash = std::strrchr(start, '/');
	const char* backslash = std::strrchr(start, '\\');
	if (!slash || (backslash && backslash > slash)) {
		slash = backslash;
	}

	const char* dot = std::strrchr(start, '.');
	if (dot && slash && dot < slash) {
		dot = nullptr;
	}

	if (dir && slash) {
		const size_t dir_len = static_cast<size_t>(slash - start + 1);
		std::memcpy(dir, start, dir_len);
		dir[dir_len] = '\0';
	}

	const char* fname_start = slash ? slash + 1 : start;
	const char* fname_end = dot ? dot : path + std::strlen(path);
	if (fname && fname_end >= fname_start) {
		const size_t fname_len = static_cast<size_t>(fname_end - fname_start);
		std::memcpy(fname, fname_start, fname_len);
		fname[fname_len] = '\0';
	}

	if (ext && dot) {
		std::strcpy(ext, dot);
	}
}

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
