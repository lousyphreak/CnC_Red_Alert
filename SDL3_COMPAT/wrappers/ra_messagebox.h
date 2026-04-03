#ifndef RA_MESSAGEBOX_H
#define RA_MESSAGEBOX_H

#include "win32_compat.h"

#include <SDL3/SDL_messagebox.h>

inline int RA_ShowMessageBox(const char* text, const char* caption, uint32_t type)
{
    Uint32 flags = SDL_MESSAGEBOX_INFORMATION;
    switch (type & 0x000000F0U) {
    case MB_ICONSTOP:
        flags = SDL_MESSAGEBOX_ERROR;
        break;
    case MB_ICONEXCLAMATION:
        flags = SDL_MESSAGEBOX_WARNING;
        break;
    default:
        break;
    }

    if ((type & MB_YESNO) == MB_YESNO) {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDYES, "Yes"},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDNO, "No"},
        };
        const SDL_MessageBoxData data = {
            flags,
            nullptr,
            caption ? caption : "Red Alert",
            text ? text : "",
            2,
            buttons,
            nullptr,
        };
        int button_id = IDNO;
        if (SDL_ShowMessageBox(&data, &button_id) == 0) {
            return button_id;
        }
        return IDNO;
    }

    SDL_ShowSimpleMessageBox(flags, caption ? caption : "Red Alert", text ? text : "", nullptr);
    return IDOK;
}

#endif
