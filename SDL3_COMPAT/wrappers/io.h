#ifndef RA_IO_WRAPPER_H
#define RA_IO_WRAPPER_H

#include "win32_compat.h"

#include <fcntl.h>
#include <unistd.h>

inline int filelength(int fd)
{
    off_t current = lseek(fd, 0, SEEK_CUR);
    off_t end = lseek(fd, 0, SEEK_END);
    lseek(fd, current, SEEK_SET);
    return static_cast<int>(end);
}

#endif
