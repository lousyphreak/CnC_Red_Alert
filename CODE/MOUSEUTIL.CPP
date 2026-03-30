#include "KEY.H"
#include "SDLINPUT.H"

int Get_Global_Mouse_X(void)
{
	return (SDL_GameInput_GetGlobalMouseX());
}

int Get_Global_Mouse_Y(void)
{
	return (SDL_GameInput_GetGlobalMouseY());
}
