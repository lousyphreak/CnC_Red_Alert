#ifndef RA_BIOS_WRAPPER_H
#define RA_BIOS_WRAPPER_H

#include "windows.h"

union WORDREGS {
    struct {
        WORD ax;
        WORD bx;
        WORD cx;
        WORD dx;
        WORD si;
        WORD di;
        WORD cflag;
        WORD flags;
    } x;
    struct {
        BYTE al, ah, bl, bh, cl, ch, dl, dh;
    } h;
};

union REGS {
    WORDREGS w;
    struct {
        DWORD eax;
        DWORD ebx;
        DWORD ecx;
        DWORD edx;
        DWORD esi;
        DWORD edi;
        DWORD cflag;
        DWORD eflags;
    } x;
};

struct SREGS {
    WORD es;
    WORD cs;
    WORD ss;
    WORD ds;
    WORD fs;
    WORD gs;
};

inline int bioskey(int)
{
    return 0;
}

inline void segread(struct SREGS* regs)
{
    if (regs) {
        regs->es = regs->cs = regs->ss = regs->ds = regs->fs = regs->gs = 0;
    }
}

#endif
