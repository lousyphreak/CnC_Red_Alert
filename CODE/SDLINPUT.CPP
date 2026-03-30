#include "SDLINPUT.H"

#include "KEY.H"

#ifdef WIN32
#define RA_INPUT_RESTORE_WIN32 1
#undef WIN32
#endif

#ifdef _WIN32
#define RA_INPUT_RESTORE__WIN32 1
#undef _WIN32
#endif

#include <SDL3/SDL.h>

#ifdef RA_INPUT_RESTORE_WIN32
#define WIN32 1
#undef RA_INPUT_RESTORE_WIN32
#endif

#ifdef RA_INPUT_RESTORE__WIN32
#define _WIN32 1
#undef RA_INPUT_RESTORE__WIN32
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

extern HWND MainWindow;
extern BOOL GameInFocus;
extern BOOL BootstrapFocusSeen;

namespace {

constexpr SHORT KEY_DOWN_MASK = static_cast<SHORT>(0x8000);
constexpr SHORT TOGGLE_MASK = static_cast<SHORT>(0x0001);
constexpr uint32_t MOUSE_LEFT_MASK = 0x01u;
constexpr uint32_t MOUSE_RIGHT_MASK = 0x02u;
constexpr uint32_t MOUSE_MIDDLE_MASK = 0x04u;

std::mutex g_input_mutex;
std::unordered_map<int, int> g_pressed_keys;
std::array<uint16_t, 256> g_virtual_key_down_counts{};
std::array<uint8_t, 256> g_toggle_state{};
std::array<uint8_t, 256> g_async_latches{};
int g_mouse_x = 0;
int g_mouse_y = 0;
uint32_t g_mouse_buttons = 0;
std::atomic<bool> g_main_thread_set{false};
std::thread::id g_main_thread_id;
std::atomic<bool> g_is_pumping{false};
bool g_cursor_clip_active = false;
RECT g_cursor_clip{};

bool valid_virtual_key(int virtual_key)
{
	return (virtual_key >= 0 && virtual_key <= 0xFF);
}

bool is_toggle_key(int virtual_key)
{
	return (virtual_key == VK_CAPITAL || virtual_key == VK_NUMLOCK || virtual_key == VK_SCROLL);
}

uint32_t mouse_button_mask(int button)
{
	switch (button) {
		case SDL_BUTTON_LEFT:
			return (MOUSE_LEFT_MASK);
		case SDL_BUTTON_RIGHT:
			return (MOUSE_RIGHT_MASK);
		case SDL_BUTTON_MIDDLE:
			return (MOUSE_MIDDLE_MASK);
		default:
			return (0);
	}
}

uint32_t translate_mouse_button_state(uint32_t button_state)
{
	uint32_t state = 0;
	if ((button_state & SDL_BUTTON_LMASK) != 0) {
		state |= MOUSE_LEFT_MASK;
	}
	if ((button_state & SDL_BUTTON_RMASK) != 0) {
		state |= MOUSE_RIGHT_MASK;
	}
	if ((button_state & SDL_BUTTON_MMASK) != 0) {
		state |= MOUSE_MIDDLE_MASK;
	}
	return (state);
}

int mouse_button_to_virtual_key(int button)
{
	switch (button) {
		case SDL_BUTTON_LEFT:
			return (VK_LBUTTON);
		case SDL_BUTTON_RIGHT:
			return (VK_RBUTTON);
		case SDL_BUTTON_MIDDLE:
			return (VK_MBUTTON);
		default:
			return (0);
	}
}

void clamp_mouse_position_locked(int &x, int &y)
{
	if (!g_cursor_clip_active) {
		return;
	}

	const int max_x = std::max<int>(g_cursor_clip.left, g_cursor_clip.right - 1);
	const int max_y = std::max<int>(g_cursor_clip.top, g_cursor_clip.bottom - 1);
	x = std::clamp(x, static_cast<int>(g_cursor_clip.left), max_x);
	y = std::clamp(y, static_cast<int>(g_cursor_clip.top), max_y);
}

void update_mouse_position_locked(int x, int y)
{
	clamp_mouse_position_locked(x, y);
	g_mouse_x = x;
	g_mouse_y = y;
}

void note_async_press_locked(int virtual_key)
{
	if (valid_virtual_key(virtual_key)) {
		g_async_latches[virtual_key] = 1;
	}
}

void press_virtual_key_locked(int virtual_key)
{
	if (!valid_virtual_key(virtual_key)) {
		return;
	}

	++g_virtual_key_down_counts[virtual_key];
	note_async_press_locked(virtual_key);
}

void release_virtual_key_locked(int virtual_key)
{
	if (!valid_virtual_key(virtual_key)) {
		return;
	}

	if (g_virtual_key_down_counts[virtual_key] != 0) {
		--g_virtual_key_down_counts[virtual_key];
	}
}

void clear_held_input_locked(void)
{
	g_pressed_keys.clear();
	g_virtual_key_down_counts.fill(0);
	g_mouse_buttons = 0;
}

SHORT key_state_locked(int virtual_key, bool consume_async)
{
	if (!valid_virtual_key(virtual_key)) {
		return (0);
	}

	SHORT state = 0;
	if (g_virtual_key_down_counts[virtual_key] != 0) {
		state = static_cast<SHORT>(state | KEY_DOWN_MASK);
	}

	if (consume_async) {
		if (g_async_latches[virtual_key] != 0) {
			state = static_cast<SHORT>(state | TOGGLE_MASK);
			g_async_latches[virtual_key] = 0;
		}
	} else if (is_toggle_key(virtual_key) && g_toggle_state[virtual_key] != 0) {
		state = static_cast<SHORT>(state | TOGGLE_MASK);
	}

	return (state);
}

int sdl_keycode_to_virtual_key(SDL_Keycode keycode)
{
	if (keycode >= SDLK_A && keycode <= SDLK_Z) {
		return (VK_A + static_cast<int>(keycode - SDLK_A));
	}
	if (keycode >= SDLK_0 && keycode <= SDLK_9) {
		return (static_cast<int>(keycode));
	}

	switch (keycode) {
		case SDLK_BACKSPACE: return (VK_BACK);
		case SDLK_TAB: return (VK_TAB);
		case SDLK_CLEAR: return (VK_CLEAR);
		case SDLK_RETURN:
		case SDLK_RETURN2:
		case SDLK_KP_ENTER: return (VK_RETURN);
		case SDLK_PAUSE: return (VK_PAUSE);
		case SDLK_CAPSLOCK: return (VK_CAPITAL);
		case SDLK_ESCAPE: return (VK_ESCAPE);
		case SDLK_SPACE: return (VK_SPACE);
		case SDLK_PAGEUP: return (VK_PRIOR);
		case SDLK_PAGEDOWN: return (VK_NEXT);
		case SDLK_END: return (VK_END);
		case SDLK_HOME: return (VK_HOME);
		case SDLK_LEFT: return (VK_LEFT);
		case SDLK_UP: return (VK_UP);
		case SDLK_RIGHT: return (VK_RIGHT);
		case SDLK_DOWN: return (VK_DOWN);
		case SDLK_PRINTSCREEN: return (VK_SNAPSHOT);
		case SDLK_INSERT: return (VK_INSERT);
		case SDLK_DELETE: return (VK_DELETE);
		case SDLK_LSHIFT:
		case SDLK_RSHIFT: return (VK_SHIFT);
		case SDLK_LCTRL:
		case SDLK_RCTRL: return (VK_CONTROL);
		case SDLK_LALT:
		case SDLK_RALT: return (VK_MENU);
		case SDLK_SCROLLLOCK: return (VK_SCROLL);
		case SDLK_NUMLOCKCLEAR: return (VK_NUMLOCK);
		case SDLK_F1: return (VK_F1);
		case SDLK_F2: return (VK_F2);
		case SDLK_F3: return (VK_F3);
		case SDLK_F4: return (VK_F4);
		case SDLK_F5: return (VK_F5);
		case SDLK_F6: return (VK_F6);
		case SDLK_F7: return (VK_F7);
		case SDLK_F8: return (VK_F8);
		case SDLK_F9: return (VK_F9);
		case SDLK_F10: return (VK_F10);
		case SDLK_F11: return (VK_F11);
		case SDLK_F12: return (VK_F12);
		case SDLK_KP_0: return (VK_NUMPAD0);
		case SDLK_KP_1: return (VK_NUMPAD1);
		case SDLK_KP_2: return (VK_NUMPAD2);
		case SDLK_KP_3: return (VK_NUMPAD3);
		case SDLK_KP_4: return (VK_NUMPAD4);
		case SDLK_KP_5: return (VK_NUMPAD5);
		case SDLK_KP_6: return (VK_NUMPAD6);
		case SDLK_KP_7: return (VK_NUMPAD7);
		case SDLK_KP_8: return (VK_NUMPAD8);
		case SDLK_KP_9: return (VK_NUMPAD9);
		case SDLK_KP_MULTIPLY: return (VK_MULTIPLY);
		case SDLK_KP_PLUS: return (VK_ADD);
		case SDLK_KP_MINUS: return (VK_SUBTRACT);
		case SDLK_KP_PERIOD: return (VK_DECIMAL);
		case SDLK_KP_DIVIDE: return (VK_DIVIDE);
		case SDLK_SEMICOLON: return (VK_OEM_1);
		case SDLK_EQUALS: return (VK_OEM_PLUS);
		case SDLK_COMMA: return (VK_OEM_COMMA);
		case SDLK_MINUS: return (VK_OEM_MINUS);
		case SDLK_PERIOD: return (VK_OEM_PERIOD);
		case SDLK_SLASH: return (VK_OEM_2);
		case SDLK_GRAVE: return (VK_OEM_3);
		case SDLK_LEFTBRACKET: return (VK_OEM_4);
		case SDLK_BACKSLASH: return (VK_OEM_5);
		case SDLK_RIGHTBRACKET: return (VK_OEM_6);
		case SDLK_APOSTROPHE: return (VK_OEM_7);
		default: return (0);
	}
}

int sdl_keyboard_event_to_virtual_key(const SDL_KeyboardEvent &event)
{
	SDL_Keycode keycode = SDL_GetKeyFromScancode(event.scancode, SDL_KMOD_NONE, true);
	int virtual_key = sdl_keycode_to_virtual_key(keycode);
	if (virtual_key != 0) {
		return (virtual_key);
	}
	return (sdl_keycode_to_virtual_key(event.key));
}

void handle_focus_event(bool focused)
{
	{
		std::scoped_lock lock(g_input_mutex);
		if (!focused) {
			clear_held_input_locked();
		}
	}

	const bool first_focus_gain = focused && !BootstrapFocusSeen;

	if (!MainWindow) {
		if (focused) {
			BootstrapFocusSeen = TRUE;
		}
		GameInFocus = focused ? TRUE : FALSE;
		return;
	}

	if (!first_focus_gain && BootstrapFocusSeen) {
		/*
		** Keep the legacy Win32 focus latch active after bootstrap so the
		** SDL3 window can keep rendering/audio/movie playback alive while
		** unfocused.
		*/
		GameInFocus = TRUE;
		return;
	}

	PostMessage(MainWindow, WM_ACTIVATEAPP, focused ? TRUE : FALSE, 0);
}

void handle_close_event(void)
{
	if (MainWindow) {
		PostMessage(MainWindow, WM_CLOSE, 0, 0);
	} else {
		PostQuitMessage(0);
	}
}

void initialize_mouse_snapshot_locked(void)
{
	float x = 0.0f;
	float y = 0.0f;
	uint32_t buttons = static_cast<uint32_t>(SDL_GetMouseState(&x, &y));
	update_mouse_position_locked(static_cast<int>(x), static_cast<int>(y));
	g_mouse_buttons = translate_mouse_button_state(buttons);
}

void handle_sdl_event(const SDL_Event &event)
{
	switch (event.type) {
		case SDL_EVENT_QUIT:
			handle_close_event();
			break;

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			handle_close_event();
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			handle_focus_event(true);
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			handle_focus_event(false);
			break;

		case SDL_EVENT_KEY_DOWN:
		{
			const int virtual_key = sdl_keyboard_event_to_virtual_key(event.key);
			if (virtual_key != 0) {
				SDL_GameInput_Handle_Key(virtual_key, static_cast<int>(event.key.scancode), event.key.repeat, false);
			}
			break;
		}

		case SDL_EVENT_KEY_UP:
		{
			const int virtual_key = sdl_keyboard_event_to_virtual_key(event.key);
			if (virtual_key != 0) {
				SDL_GameInput_Handle_Key(virtual_key, static_cast<int>(event.key.scancode), false, true);
			}
			break;
		}

		case SDL_EVENT_MOUSE_MOTION:
			SDL_GameInput_Handle_Mouse_Motion(static_cast<int>(event.motion.x), static_cast<int>(event.motion.y),
				static_cast<uint32_t>(event.motion.state));
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			SDL_GameInput_Handle_Mouse_Button(static_cast<int>(event.button.button), static_cast<int>(event.button.x),
				static_cast<int>(event.button.y), false);
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			SDL_GameInput_Handle_Mouse_Button(static_cast<int>(event.button.button), static_cast<int>(event.button.x),
				static_cast<int>(event.button.y), true);
			break;

		default:
			break;
	}
}

}	// namespace

void SDL_GameInput_Set_Main_Thread(void)
{
	g_main_thread_id = std::this_thread::get_id();

	std::scoped_lock lock(g_input_mutex);
	initialize_mouse_snapshot_locked();
	SDL_Keymod mod_state = SDL_GetModState();
	g_toggle_state[VK_CAPITAL] = ((mod_state & SDL_KMOD_CAPS) != 0) ? 1 : 0;
	g_toggle_state[VK_NUMLOCK] = ((mod_state & SDL_KMOD_NUM) != 0) ? 1 : 0;

	g_main_thread_set.store(true);
}

void SDL_GameInput_Handle_Key(int virtual_key, int physical_key, bool repeat, bool release)
{
	if (!valid_virtual_key(virtual_key)) {
		return;
	}

	{
		std::scoped_lock lock(g_input_mutex);
		if (release) {
			auto it = g_pressed_keys.find(physical_key);
			int mapped_key = virtual_key;
			if (it != g_pressed_keys.end()) {
				mapped_key = it->second;
				g_pressed_keys.erase(it);
			}
			release_virtual_key_locked(mapped_key);
		} else {
			auto it = g_pressed_keys.find(physical_key);
			if (it == g_pressed_keys.end()) {
				g_pressed_keys.emplace(physical_key, virtual_key);
				press_virtual_key_locked(virtual_key);
				if (!repeat && is_toggle_key(virtual_key)) {
					g_toggle_state[virtual_key] = g_toggle_state[virtual_key] ? 0 : 1;
				}
			} else if (it->second != virtual_key) {
				release_virtual_key_locked(it->second);
				it->second = virtual_key;
				press_virtual_key_locked(virtual_key);
			} else {
				note_async_press_locked(virtual_key);
			}
		}
	}

	Keyboard_Handle_Key_Event(static_cast<unsigned short>(virtual_key), release);
}

void SDL_GameInput_Handle_Mouse_Motion(int x, int y, uint32_t button_state)
{
	int mouse_x = x;
	int mouse_y = y;
	{
		std::scoped_lock lock(g_input_mutex);
		update_mouse_position_locked(x, y);
		g_mouse_buttons = translate_mouse_button_state(button_state);
		mouse_x = g_mouse_x;
		mouse_y = g_mouse_y;
	}

	Keyboard_Handle_Mouse_Motion(mouse_x, mouse_y);
}

void SDL_GameInput_Handle_Mouse_Button(int button, int x, int y, bool release)
{
	const int virtual_key = mouse_button_to_virtual_key(button);
	if (virtual_key == 0) {
		return;
	}

	int mouse_x = x;
	int mouse_y = y;
	{
		std::scoped_lock lock(g_input_mutex);
		update_mouse_position_locked(x, y);
		uint32_t mask = mouse_button_mask(button);
		if (!release) {
			g_mouse_buttons |= mask;
			note_async_press_locked(virtual_key);
		} else {
			g_mouse_buttons &= ~mask;
		}
		mouse_x = g_mouse_x;
		mouse_y = g_mouse_y;
	}

	Keyboard_Handle_Mouse_Button_Event(static_cast<unsigned short>(virtual_key), mouse_x, mouse_y, release);
}

void SDL_GameInput_Pump(void)
{
	if (g_is_pumping.exchange(true)) {
		return;
	}

	if (!g_main_thread_set.load()) {
		SDL_GameInput_Set_Main_Thread();
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		handle_sdl_event(event);
	}

	g_is_pumping.store(false);
}

BOOL SDL_GameInput_Wait(void)
{
	if (g_main_thread_set.load() && std::this_thread::get_id() != g_main_thread_id) {
		return (FALSE);
	}

	if (g_is_pumping.exchange(true)) {
		return (FALSE);
	}

	if (!g_main_thread_set.load()) {
		SDL_GameInput_Set_Main_Thread();
	}

	SDL_Event event;
	if (!SDL_WaitEvent(&event)) {
		g_is_pumping.store(false);
		return (FALSE);
	}

	handle_sdl_event(event);
	while (SDL_PollEvent(&event)) {
		handle_sdl_event(event);
	}

	g_is_pumping.store(false);
	return (TRUE);
}

void SDL_GameInput_Maybe_Pump(void)
{
	if (!g_main_thread_set.load() || std::this_thread::get_id() != g_main_thread_id) {
		return;
	}

	SDL_GameInput_Pump();
}

SHORT SDL_GameInput_GetKeyState(int virtual_key)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return (key_state_locked(virtual_key, false));
}

SHORT SDL_GameInput_GetAsyncKeyState(int virtual_key)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return (key_state_locked(virtual_key, true));
}

BOOL SDL_GameInput_GetCursorPos(POINT* point)
{
	if (point == NULL) {
		return (FALSE);
	}

	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	point->x = g_mouse_x;
	point->y = g_mouse_y;
	return (TRUE);
}

int SDL_GameInput_GetGlobalMouseX(void)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return (g_mouse_x);
}

int SDL_GameInput_GetGlobalMouseY(void)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return (g_mouse_y);
}

void SDL_GameInput_SetCursorClip(const RECT* rect)
{
	std::scoped_lock lock(g_input_mutex);
	if (rect == NULL) {
		g_cursor_clip_active = false;
		return;
	}

	g_cursor_clip_active = true;
	g_cursor_clip = *rect;
	update_mouse_position_locked(g_mouse_x, g_mouse_y);
}
