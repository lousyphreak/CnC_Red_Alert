#include "dos.h"

#include <cstdint>

int _dos_getdrive(unsigned* drive)
{
    if (drive != nullptr) {
        *drive = 3;
    }
    return 0;
}

int _dos_setdrive(unsigned, unsigned* numdrives)
{
    if (numdrives != nullptr) {
        *numdrives = 26;
    }
    return 0;
}

int _dos_getdiskfree(unsigned, struct diskfree_t* diskspace)
{
    if (diskspace == nullptr) {
        return -1;
    }

    /*
    **	SDL3 does not provide a disk space query API.
    **	Return reasonable defaults that indicate plenty of space available.
    */
    diskspace->bytes_per_sector = 512U;
    diskspace->sectors_per_cluster = 8U;
    diskspace->avail_clusters = 1024U * 1024U;
    diskspace->total_clusters = 1024U * 1024U * 2U;
    return 0;
}

void _harderr(int (*)(unsigned int, unsigned int, unsigned int*))
{
}

void _hardresume(int)
{
}
