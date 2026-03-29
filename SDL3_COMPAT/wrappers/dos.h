#ifndef RA_DOS_WRAPPER_H
#define RA_DOS_WRAPPER_H

#include "windows.h"

#include <unistd.h>

inline void delay(unsigned milliseconds)
{
    Sleep(milliseconds);
}

#endif
