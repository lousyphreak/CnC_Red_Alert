#ifndef RA_DOS_WRAPPER_H
#define RA_DOS_WRAPPER_H

#include "windows.h"

#include <cstdint>
#include <unistd.h>

#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif

#ifndef _A_NORMAL
#define _A_NORMAL 0x00
#endif
#ifndef _A_RDONLY
#define _A_RDONLY 0x01
#endif
#ifndef _A_HIDDEN
#define _A_HIDDEN 0x02
#endif
#ifndef _A_SYSTEM
#define _A_SYSTEM 0x04
#endif
#ifndef _A_VOLID
#define _A_VOLID 0x08
#endif
#ifndef _A_SUBDIR
#define _A_SUBDIR 0x10
#endif
#ifndef _A_ARCH
#define _A_ARCH 0x20
#endif

#ifndef _HARDERR_IGNORE
#define _HARDERR_IGNORE 0
#endif
#ifndef _HARDERR_RETRY
#define _HARDERR_RETRY 1
#endif
#ifndef _HARDERR_ABORT
#define _HARDERR_ABORT 2
#endif
#ifndef _HARDERR_FAIL
#define _HARDERR_FAIL 3
#endif

struct find_t {
    unsigned char reserved[21];
    unsigned char attrib = 0;
    uint16_t wr_time = 0;
    uint16_t wr_date = 0;
    uint32_t size = 0;
    char name[_MAX_PATH] = {0};
    void* __handle = nullptr;
};

struct diskfree_t {
    uint32_t total_clusters = 0;
    uint32_t avail_clusters = 0;
    uint32_t sectors_per_cluster = 0;
    uint32_t bytes_per_sector = 0;
};

inline void delay(unsigned milliseconds)
{
    Sleep(milliseconds);
}

int _dos_findfirst(const char* path, unsigned attrib, struct find_t* block);
int _dos_findnext(struct find_t* block);
int _dos_getdrive(unsigned* drive);
int _dos_setdrive(unsigned drive, unsigned* numdrives);
int _dos_getdiskfree(unsigned drive, struct diskfree_t* diskspace);
void _harderr(int (*handler)(unsigned int, unsigned int, unsigned int*));
void _hardresume(int result);
inline void hardresume(int result)
{
    _hardresume(result);
}

#endif
