#ifndef RA_OBJBASE_WRAPPER_H
#define RA_OBJBASE_WRAPPER_H

#include "windows.h"

inline HRESULT CoInitialize(LPVOID)
{
    return 0;
}

inline void CoUninitialize(void)
{
}

inline HRESULT OleInitialize(LPVOID reserved)
{
    return CoInitialize(reserved);
}

inline void OleUninitialize(void)
{
    CoUninitialize();
}

#endif
