#ifndef SDLINPUT_H
#define SDLINPUT_H

#include "win32_compat.h"
#include <SDL3/SDL.h>
#include <cstdint>

void SDL_GameInput_Set_Main_Thread(void);
void SDL_GameInput_Pump(void);
BOOL SDL_GameInput_Wait(void);
void SDL_GameInput_Maybe_Pump(void);
void SDL_GameInput_Handle_Key(SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod_state, bool repeat, bool release);
void SDL_GameInput_Handle_Mouse_Motion(int x, int y, uint32_t button_state);
void SDL_GameInput_Handle_Mouse_Button(int button, int x, int y, bool release);
HANDLE SDL_GameInput_SetCursor(HANDLE cursor);
int SDL_GameInput_ShowCursor(BOOL show);
BOOL SDL_GameInput_ClipCursor(const RECT* rect);
BOOL SDL_GameInput_GetCursorPos(POINT* point);
int SDL_GameInput_GetGlobalMouseX(void);
int SDL_GameInput_GetGlobalMouseY(void);
BOOL SDL_GameInput_GetViewportRect(RECT* rect);
void SDL_GameInput_SetCursorClip(const RECT* rect);
bool SDL_GameInput_IsModifierPressed(SDL_Keymod mod_mask);
bool SDL_GameInput_IsModifierToggled(SDL_Keymod mod_mask);
bool SDL_GameInput_IsScancodePressed(SDL_Scancode scancode);
bool SDL_GameInput_IsMouseButtonPressed(uint8_t button);
bool SDL_GameInput_IsLegacyKeyPressed(unsigned short key);
char SDL_GameInput_KeyToASCII(unsigned short key);
SHORT SDL_GameInput_AsciiToLegacyKey(unsigned char ch);

#endif
