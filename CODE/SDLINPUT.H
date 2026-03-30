#ifndef SDLINPUT_H
#define SDLINPUT_H

#include <windows.h>
#include <cstdint>

void SDL_GameInput_Set_Main_Thread(void);
void SDL_GameInput_Pump(void);
BOOL SDL_GameInput_Wait(void);
void SDL_GameInput_Maybe_Pump(void);
void SDL_GameInput_Handle_Key(int virtual_key, int physical_key, bool repeat, bool release);
void SDL_GameInput_Handle_Mouse_Motion(int x, int y, uint32_t button_state);
void SDL_GameInput_Handle_Mouse_Button(int button, int x, int y, bool release);
SHORT SDL_GameInput_GetKeyState(int virtual_key);
SHORT SDL_GameInput_GetAsyncKeyState(int virtual_key);
BOOL SDL_GameInput_GetCursorPos(POINT* point);
int SDL_GameInput_GetGlobalMouseX(void);
int SDL_GameInput_GetGlobalMouseY(void);
void SDL_GameInput_SetCursorClip(const RECT* rect);

#endif
