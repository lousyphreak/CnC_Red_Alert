#include "KEY.H"
#include <SDL3/SDL.h>

int Get_Global_Mouse_X(void)
{
	float x = 0.0f;
	float y = 0.0f;
	SDL_GetMouseState(&x, &y);
	(void)y;
	return static_cast<int>(x);
}

int Get_Global_Mouse_Y(void)
{
	float x = 0.0f;
	float y = 0.0f;
	SDL_GetMouseState(&x, &y);
	(void)x;
	return static_cast<int>(y);
}
