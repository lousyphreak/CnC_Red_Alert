#include "SDLINPUT.H"

#include "GBUFFER.H"
#include "KEY.H"
#include "../WIN32LIB/INCLUDE/MOUSE.H"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <stdint.h>
extern RAWindow* MainWindow;
extern void Main_Window_Handle_Focus_Change(bool focused);
extern void Main_Window_Handle_Close_Request(void);
extern int AllDone;
extern int32_t GameInFocus;
extern int32_t BootstrapFocusSeen;
extern GraphicViewPortClass SeenBuff;
extern WWMouseClass* WWMouse;
extern void WWDraw_Request_Present(void);
extern void WWDraw_Flush_Present(void);

namespace {

constexpr uint32_t MOUSE_LEFT_MASK = 0x01u;
constexpr uint32_t MOUSE_RIGHT_MASK = 0x02u;
constexpr uint32_t MOUSE_MIDDLE_MASK = 0x04u;
constexpr uint16_t LEGACY_MIDDLE_MOUSE = 0x04;
constexpr uint16_t LEGACY_NUMPAD0 = 0x60;
constexpr uint16_t LEGACY_NUMPAD5 = 0x65;
constexpr uint16_t LEGACY_DECIMAL = 0x6E;

std::mutex g_input_mutex;
std::array<uint16_t, SDL_SCANCODE_COUNT> g_scancode_down_counts{};
SDL_Keymod g_mod_state = SDL_KMOD_NONE;
int g_mouse_x = 0;
int g_mouse_y = 0;
uint32_t g_mouse_buttons = 0;
std::atomic<bool> g_main_thread_set{false};
std::thread::id g_main_thread_id;
std::atomic<bool> g_is_pumping{false};
std::atomic<bool> g_app_callbacks_mode{false};
std::mutex g_callback_event_mutex;
std::deque<SDL_Event> g_callback_events;
bool g_callback_event_watch_installed = false;
bool g_cursor_clip_active = false;
RECT g_cursor_clip{};
bool g_mission_mouse_capture_requested = false;
uint32_t g_mouse_capture_suppression_count = 0;
bool g_mission_mouse_capture_applied = false;

bool valid_scancode(SDL_Scancode scancode)
{
	return (scancode >= SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT);
}

bool get_viewport_rect(RECT& rect)
{
	if (SeenBuff.Get_Width() <= 0 || SeenBuff.Get_Height() <= 0) {
		return false;
	}

	rect.left = SeenBuff.Get_XPos();
	rect.top = SeenBuff.Get_YPos();
	rect.right = rect.left + SeenBuff.Get_Width();
	rect.bottom = rect.top + SeenBuff.Get_Height();
	return true;
}

bool sdl_uses_wayland_video_driver()
{
	const char* video_driver = SDL_GetCurrentVideoDriver();
	return video_driver != NULL && SDL_strcasecmp(video_driver, "wayland") == 0;
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

uint16_t mouse_button_to_legacy_key(int button)
{
	switch (button) {
		case SDL_BUTTON_LEFT:
			return (KN_LMOUSE);
		case SDL_BUTTON_RIGHT:
			return (KN_RMOUSE);
		case SDL_BUTTON_MIDDLE:
			return (LEGACY_MIDDLE_MOUSE);
		default:
			return (0);
	}
}

void translate_window_mouse_position(int& x, int& y)
{
	int translated_x = x;
	int translated_y = y;
	if (RA_WindowToGamePoint(MainWindow, static_cast<float>(x), static_cast<float>(y), &translated_x, &translated_y)) {
		RECT viewport{};
		if (get_viewport_rect(viewport)) {
			translated_x -= viewport.left;
			translated_y -= viewport.top;
			translated_x = std::clamp(translated_x, 0, std::max(0, SeenBuff.Get_Width() - 1));
			translated_y = std::clamp(translated_y, 0, std::max(0, SeenBuff.Get_Height() - 1));
		}
		x = translated_x;
		y = translated_y;
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

void initialize_mouse_snapshot_locked(void);

bool should_apply_mission_mouse_capture(void)
{
	return g_mission_mouse_capture_requested
		&& g_mouse_capture_suppression_count == 0
		&& MainWindow != NULL
		&& MainWindow->sdl_window != NULL;
}

bool get_window_point_for_current_game_cursor(float& window_x, float& window_y)
{
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		return false;
	}

	int cursor_x = 0;
	int cursor_y = 0;
	{
		std::scoped_lock lock(g_input_mutex);
		cursor_x = g_mouse_x;
		cursor_y = g_mouse_y;
	}

	RECT viewport{};
	if (SDL_GameInput_GetViewportRect(&viewport)) {
		cursor_x += viewport.left;
		cursor_y += viewport.top;
	}

	RECT cursor_rect{};
	cursor_rect.left = cursor_x;
	cursor_rect.top = cursor_y;
	cursor_rect.right = cursor_x + 1;
	cursor_rect.bottom = cursor_y + 1;

	SDL_Rect window_rect{};
	if (!RA_GameRectToWindowRect(MainWindow, &cursor_rect, &window_rect) || window_rect.w <= 0 || window_rect.h <= 0) {
		return false;
	}

	window_x = static_cast<float>(window_rect.x) + (static_cast<float>(window_rect.w) * 0.5f);
	window_y = static_cast<float>(window_rect.y) + (static_cast<float>(window_rect.h) * 0.5f);
	return true;
}

#if defined(__EMSCRIPTEN__)
// When running in the browser, SDL3's Emscripten backend does not implement
// ShowScreenKeyboard, so SDL_StartTextInput alone never opens a mobile soft
// keyboard. We instead rely on a repo-owned JS bridge in web/shell.html that
// focuses a real DOM editor from a trusted user gesture and feeds committed
// characters back into the legacy key queue. The JS side decides whether the
// bridge should do anything on the current device, so desktop hardware-keyboard
// users keep the native SDL keydown path unchanged.
EM_JS(void, ra_set_web_soft_keyboard_active, (int enabled), {
    if (typeof Module === 'undefined' || !Module.raSoftKeyboard || !Module.raSoftKeyboard.setActive) {
        return;
    }
    Module.raSoftKeyboard.setActive(!!enabled);
});

static void push_web_soft_key(uint16_t legacy_key)
{
	Keyboard_Handle_Key_Event(legacy_key, false);
}

extern "C" EMSCRIPTEN_KEEPALIVE void SDL_GameInput_WebCommitTextChar(uint32_t codepoint)
{
	if (codepoint > 255u) {
		return;
	}

	const int16_t legacy_key = SDL_GameInput_AsciiToLegacyKey(static_cast<uint8_t>(codepoint));
	if (legacy_key != 0) {
		push_web_soft_key(static_cast<uint16_t>(legacy_key));
	}
}

extern "C" EMSCRIPTEN_KEEPALIVE void SDL_GameInput_WebCommitBackspace(void)
{
	push_web_soft_key(KN_BACKSPACE);
}

extern "C" EMSCRIPTEN_KEEPALIVE void SDL_GameInput_WebCommitReturn(void)
{
	push_web_soft_key(KN_RETURN);
}

extern "C" EMSCRIPTEN_KEEPALIVE void SDL_GameInput_WebRequestRedraw(void)
{
	SDL_GameInput_RequestRedraw();
}
#endif

void apply_mission_mouse_capture(void)
{
	const bool should_capture = should_apply_mission_mouse_capture();
	if (g_mission_mouse_capture_applied == should_capture) {
		if (should_capture && WWMouse != NULL) {
			WWMouse->Set_Cursor_Clip();
		}
		return;
	}

	if (!should_capture) {
		if (MainWindow != NULL && MainWindow->sdl_window != NULL && SDL_GetWindowRelativeMouseMode(MainWindow->sdl_window)) {
			float window_x = 0.0f;
			float window_y = 0.0f;
			if (get_window_point_for_current_game_cursor(window_x, window_y)) {
				SDL_WarpMouseInWindow(MainWindow->sdl_window, window_x, window_y);
			}
			SDL_SetWindowRelativeMouseMode(MainWindow->sdl_window, false);
		}
		if (WWMouse != NULL) {
			WWMouse->Clear_Cursor_Clip();
		}
		{
			std::scoped_lock lock(g_input_mutex);
			initialize_mouse_snapshot_locked();
		}
		g_mission_mouse_capture_applied = false;
		return;
	}

	if (WWMouse != NULL) {
		WWMouse->Set_Cursor_Clip();
	}
	if (MainWindow != NULL && MainWindow->sdl_window != NULL) {
		(void)SDL_SetWindowRelativeMouseMode(MainWindow->sdl_window, true);
	}
	g_mission_mouse_capture_applied = true;
}

uint16_t legacy_key_from_scancode(SDL_Scancode scancode)
{
	switch (scancode) {
		case SDL_SCANCODE_LSHIFT:
		case SDL_SCANCODE_RSHIFT:
			return KN_LSHIFT;
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL:
			return KN_LCTRL;
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:
			return KN_LALT;
		case SDL_SCANCODE_RETURN:
			return KN_RETURN;
		case SDL_SCANCODE_ESCAPE:
			return KN_ESC;
		case SDL_SCANCODE_BACKSPACE:
			return KN_BACKSPACE;
		case SDL_SCANCODE_TAB:
			return KN_TAB;
		case SDL_SCANCODE_DELETE:
			return KN_DELETE;
		case SDL_SCANCODE_INSERT:
			return KN_INSERT;
		case SDL_SCANCODE_HOME:
			return KN_HOME;
		case SDL_SCANCODE_END:
			return KN_END;
		case SDL_SCANCODE_PAGEUP:
			return KN_PGUP;
		case SDL_SCANCODE_PAGEDOWN:
			return KN_PGDN;
		case SDL_SCANCODE_UP:
			return KN_UP;
		case SDL_SCANCODE_DOWN:
			return KN_DOWN;
		case SDL_SCANCODE_LEFT:
			return KN_LEFT;
		case SDL_SCANCODE_RIGHT:
			return KN_RIGHT;
		case SDL_SCANCODE_CAPSLOCK:
			return KN_CAPSLOCK;
		case SDL_SCANCODE_SCROLLLOCK:
			return KN_SCROLLLOCK;
		case SDL_SCANCODE_NUMLOCKCLEAR:
			return KN_NUMLOCK;
		case SDL_SCANCODE_PAUSE:
			return KN_PAUSE;
		case SDL_SCANCODE_F1:
			return KN_F1;
		case SDL_SCANCODE_F2:
			return KN_F2;
		case SDL_SCANCODE_F3:
			return KN_F3;
		case SDL_SCANCODE_F4:
			return KN_F4;
		case SDL_SCANCODE_F5:
			return KN_F5;
		case SDL_SCANCODE_F6:
			return KN_F6;
		case SDL_SCANCODE_F7:
			return KN_F7;
		case SDL_SCANCODE_F8:
			return KN_F8;
		case SDL_SCANCODE_F9:
			return KN_F9;
		case SDL_SCANCODE_F10:
			return KN_F10;
		case SDL_SCANCODE_F11:
			return KN_F11;
		case SDL_SCANCODE_F12:
			return KN_F12;
		case SDL_SCANCODE_KP_0:
			return LEGACY_NUMPAD0;
		case SDL_SCANCODE_KP_1:
			return KN_E_END;
		case SDL_SCANCODE_KP_2:
			return KN_E_DOWN;
		case SDL_SCANCODE_KP_3:
			return KN_E_PGDN;
		case SDL_SCANCODE_KP_4:
			return KN_E_LEFT;
		case SDL_SCANCODE_KP_5:
			return LEGACY_NUMPAD5;
		case SDL_SCANCODE_KP_6:
			return KN_E_RIGHT;
		case SDL_SCANCODE_KP_7:
			return KN_E_HOME;
		case SDL_SCANCODE_KP_8:
			return KN_E_UP;
		case SDL_SCANCODE_KP_9:
			return KN_E_PGUP;
		case SDL_SCANCODE_KP_DIVIDE:
			return KN_KEYPAD_SLASH;
		case SDL_SCANCODE_KP_MULTIPLY:
			return KN_KEYPAD_ASTERISK;
		case SDL_SCANCODE_KP_MINUS:
			return KN_KEYPAD_MINUS;
		case SDL_SCANCODE_KP_PLUS:
			return KN_KEYPAD_PLUS;
		case SDL_SCANCODE_KP_ENTER:
			return KN_KEYPAD_RETURN;
		case SDL_SCANCODE_KP_PERIOD:
			return LEGACY_DECIMAL;
		default:
			return 0;
	}
}

uint16_t legacy_key_from_keycode(SDL_Keycode keycode)
{
	if (keycode >= SDLK_A && keycode <= SDLK_Z) {
		return static_cast<uint16_t>(KN_A + static_cast<uint16_t>(keycode - SDLK_A));
	}
	if (keycode >= SDLK_0 && keycode <= SDLK_9) {
		return static_cast<uint16_t>(KN_0 + static_cast<uint16_t>(keycode - SDLK_0));
	}

	switch (keycode) {
		case SDLK_BACKSPACE: return KN_BACKSPACE;
		case SDLK_TAB: return KN_TAB;
		case SDLK_CLEAR: return KN_CENTER;
		case SDLK_RETURN:
		case SDLK_RETURN2:
		case SDLK_KP_ENTER: return KN_RETURN;
		case SDLK_PAUSE: return KN_PAUSE;
		case SDLK_CAPSLOCK: return KN_CAPSLOCK;
		case SDLK_ESCAPE: return KN_ESC;
		case SDLK_SPACE: return KN_SPACE;
		case SDLK_PAGEUP: return KN_PGUP;
		case SDLK_PAGEDOWN: return KN_PGDN;
		case SDLK_END: return KN_END;
		case SDLK_HOME: return KN_HOME;
		case SDLK_LEFT: return KN_LEFT;
		case SDLK_UP: return KN_UP;
		case SDLK_RIGHT: return KN_RIGHT;
		case SDLK_DOWN: return KN_DOWN;
		case SDLK_PRINTSCREEN: return KN_PRNTSCRN;
		case SDLK_INSERT: return KN_INSERT;
		case SDLK_DELETE: return KN_DELETE;
		case SDLK_LSHIFT:
		case SDLK_RSHIFT: return KN_LSHIFT;
		case SDLK_LCTRL:
		case SDLK_RCTRL: return KN_LCTRL;
		case SDLK_LALT:
		case SDLK_RALT: return KN_LALT;
		case SDLK_SCROLLLOCK: return KN_SCROLLLOCK;
		case SDLK_NUMLOCKCLEAR: return KN_NUMLOCK;
		case SDLK_F1: return KN_F1;
		case SDLK_F2: return KN_F2;
		case SDLK_F3: return KN_F3;
		case SDLK_F4: return KN_F4;
		case SDLK_F5: return KN_F5;
		case SDLK_F6: return KN_F6;
		case SDLK_F7: return KN_F7;
		case SDLK_F8: return KN_F8;
		case SDLK_F9: return KN_F9;
		case SDLK_F10: return KN_F10;
		case SDLK_F11: return KN_F11;
		case SDLK_F12: return KN_F12;
		case SDLK_KP_0: return LEGACY_NUMPAD0;
		case SDLK_KP_1: return KN_E_END;
		case SDLK_KP_2: return KN_E_DOWN;
		case SDLK_KP_3: return KN_E_PGDN;
		case SDLK_KP_4: return KN_E_LEFT;
		case SDLK_KP_5: return LEGACY_NUMPAD5;
		case SDLK_KP_6: return KN_E_RIGHT;
		case SDLK_KP_7: return KN_E_HOME;
		case SDLK_KP_8: return KN_E_UP;
		case SDLK_KP_9: return KN_E_PGUP;
		case SDLK_KP_MULTIPLY: return KN_KEYPAD_ASTERISK;
		case SDLK_KP_PLUS: return KN_KEYPAD_PLUS;
		case SDLK_KP_MINUS: return KN_KEYPAD_MINUS;
		case SDLK_KP_PERIOD: return LEGACY_DECIMAL;
		case SDLK_KP_DIVIDE: return KN_KEYPAD_SLASH;
		case SDLK_SEMICOLON: return KN_SEMICOLON;
		case SDLK_EQUALS: return KN_EQUAL;
		case SDLK_COMMA: return KN_COMMA;
		case SDLK_MINUS: return KN_MINUS;
		case SDLK_PERIOD: return KN_PERIOD;
		case SDLK_SLASH: return KN_SLASH;
		case SDLK_GRAVE: return KN_GRAVE;
		case SDLK_LEFTBRACKET: return KN_LBRACKET;
		case SDLK_BACKSLASH: return KN_BACKSLASH;
		case SDLK_RIGHTBRACKET: return KN_RBRACKET;
		case SDLK_APOSTROPHE: return KN_SQUOTE;
		default: return 0;
	}
}

void update_modifier_state_locked(SDL_Keymod mod_state)
{
	g_mod_state = mod_state;
}

void note_key_event_locked(SDL_Scancode scancode, SDL_Keymod mod_state, bool repeat, bool release)
{
	update_modifier_state_locked(mod_state);
	if (!valid_scancode(scancode)) {
		return;
	}
	if (release) {
		if (g_scancode_down_counts[scancode] != 0) {
			--g_scancode_down_counts[scancode];
		}
		return;
	}
	if (!repeat) {
		++g_scancode_down_counts[scancode];
	}
}

void clear_held_input_locked(void)
{
	g_scancode_down_counts.fill(0);
	g_mod_state = static_cast<SDL_Keymod>(g_mod_state & (SDL_KMOD_CAPS | SDL_KMOD_NUM | SDL_KMOD_SCROLL));
	g_mouse_buttons = 0;
}

bool scancode_down_locked(SDL_Scancode scancode)
{
	return valid_scancode(scancode) && g_scancode_down_counts[scancode] != 0;
}

bool modifier_pressed_locked(SDL_Keymod mod_mask)
{
	return (g_mod_state & mod_mask) != 0;
}

bool mouse_button_down_locked(uint8_t button)
{
	switch (button) {
		case SDL_BUTTON_LEFT:
			return (g_mouse_buttons & MOUSE_LEFT_MASK) != 0;
		case SDL_BUTTON_RIGHT:
			return (g_mouse_buttons & MOUSE_RIGHT_MASK) != 0;
		case SDL_BUTTON_MIDDLE:
			return (g_mouse_buttons & MOUSE_MIDDLE_MASK) != 0;
		default:
			return false;
	}
}

SDL_Scancode scancode_from_legacy_key(uint16_t key)
{
	key &= 0xFFu;

	if (key >= KN_A && key <= KN_Z) {
		return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (key - KN_A));
	}
	if (key >= KN_0 && key <= KN_9) {
		return static_cast<SDL_Scancode>(SDL_SCANCODE_0 + (key - KN_0));
	}

	switch (key) {
		case KN_SPACE: return SDL_SCANCODE_SPACE;
		case KN_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
		case KN_EQUAL: return SDL_SCANCODE_EQUALS;
		case KN_COMMA: return SDL_SCANCODE_COMMA;
		case KN_MINUS: return SDL_SCANCODE_MINUS;
		case KN_PERIOD: return SDL_SCANCODE_PERIOD;
		case KN_SLASH: return SDL_SCANCODE_SLASH;
		case KN_GRAVE: return SDL_SCANCODE_GRAVE;
		case KN_LBRACKET: return SDL_SCANCODE_LEFTBRACKET;
		case KN_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
		case KN_RBRACKET: return SDL_SCANCODE_RIGHTBRACKET;
		case KN_SQUOTE: return SDL_SCANCODE_APOSTROPHE;
		default: return SDL_SCANCODE_UNKNOWN;
	}
}

bool legacy_key_down_locked(uint16_t key)
{
	const SDL_Scancode direct_scancode = scancode_from_legacy_key(key);
	if (direct_scancode != SDL_SCANCODE_UNKNOWN) {
		return scancode_down_locked(direct_scancode);
	}

	switch (key & 0xFFu) {
		case KN_LMOUSE:
			return mouse_button_down_locked(SDL_BUTTON_LEFT);
		case KN_RMOUSE:
			return mouse_button_down_locked(SDL_BUTTON_RIGHT);
		case LEGACY_MIDDLE_MOUSE:
			return mouse_button_down_locked(SDL_BUTTON_MIDDLE);
		case KN_LSHIFT:
			return scancode_down_locked(SDL_SCANCODE_LSHIFT) || scancode_down_locked(SDL_SCANCODE_RSHIFT);
		case KN_LCTRL:
			return scancode_down_locked(SDL_SCANCODE_LCTRL) || scancode_down_locked(SDL_SCANCODE_RCTRL);
		case KN_LALT:
			return scancode_down_locked(SDL_SCANCODE_LALT) || scancode_down_locked(SDL_SCANCODE_RALT);
		case KN_RETURN:
			return scancode_down_locked(SDL_SCANCODE_RETURN) || scancode_down_locked(SDL_SCANCODE_KP_ENTER);
		case KN_ESC:
			return scancode_down_locked(SDL_SCANCODE_ESCAPE);
		case KN_BACKSPACE:
			return scancode_down_locked(SDL_SCANCODE_BACKSPACE);
		case KN_TAB:
			return scancode_down_locked(SDL_SCANCODE_TAB);
		case KN_DELETE:
			return scancode_down_locked(SDL_SCANCODE_DELETE);
		case KN_INSERT:
			return scancode_down_locked(SDL_SCANCODE_INSERT);
		case KN_HOME:
			return scancode_down_locked(SDL_SCANCODE_HOME);
		case KN_END:
			return scancode_down_locked(SDL_SCANCODE_END);
		case KN_PGUP:
			return scancode_down_locked(SDL_SCANCODE_PAGEUP);
		case KN_PGDN:
			return scancode_down_locked(SDL_SCANCODE_PAGEDOWN);
		case KN_UP:
			return scancode_down_locked(SDL_SCANCODE_UP);
		case KN_DOWN:
			return scancode_down_locked(SDL_SCANCODE_DOWN);
		case KN_LEFT:
			return scancode_down_locked(SDL_SCANCODE_LEFT);
		case KN_RIGHT:
			return scancode_down_locked(SDL_SCANCODE_RIGHT);
		case KN_CAPSLOCK:
			return scancode_down_locked(SDL_SCANCODE_CAPSLOCK);
		case KN_SCROLLLOCK:
			return scancode_down_locked(SDL_SCANCODE_SCROLLLOCK);
		case KN_NUMLOCK:
			return scancode_down_locked(SDL_SCANCODE_NUMLOCKCLEAR);
		case KN_PAUSE:
			return scancode_down_locked(SDL_SCANCODE_PAUSE);
		case KN_F1:
			return scancode_down_locked(SDL_SCANCODE_F1);
		case KN_F2:
			return scancode_down_locked(SDL_SCANCODE_F2);
		case KN_F3:
			return scancode_down_locked(SDL_SCANCODE_F3);
		case KN_F4:
			return scancode_down_locked(SDL_SCANCODE_F4);
		case KN_F5:
			return scancode_down_locked(SDL_SCANCODE_F5);
		case KN_F6:
			return scancode_down_locked(SDL_SCANCODE_F6);
		case KN_F7:
			return scancode_down_locked(SDL_SCANCODE_F7);
		case KN_F8:
			return scancode_down_locked(SDL_SCANCODE_F8);
		case KN_F9:
			return scancode_down_locked(SDL_SCANCODE_F9);
		case KN_F10:
			return scancode_down_locked(SDL_SCANCODE_F10);
		case KN_F11:
			return scancode_down_locked(SDL_SCANCODE_F11);
		case KN_F12:
			return scancode_down_locked(SDL_SCANCODE_F12);
		case KN_E_UP:
			return scancode_down_locked(SDL_SCANCODE_KP_8);
		case KN_E_DOWN:
			return scancode_down_locked(SDL_SCANCODE_KP_2);
		case KN_E_LEFT:
			return scancode_down_locked(SDL_SCANCODE_KP_4);
		case KN_E_RIGHT:
			return scancode_down_locked(SDL_SCANCODE_KP_6);
		case KN_E_HOME:
			return scancode_down_locked(SDL_SCANCODE_KP_7);
		case KN_E_END:
			return scancode_down_locked(SDL_SCANCODE_KP_1);
		case KN_E_PGUP:
			return scancode_down_locked(SDL_SCANCODE_KP_9);
		case KN_E_PGDN:
			return scancode_down_locked(SDL_SCANCODE_KP_3);
		case KN_CENTER:
			return scancode_down_locked(SDL_SCANCODE_CLEAR);
		case LEGACY_NUMPAD0:
			return scancode_down_locked(SDL_SCANCODE_KP_0);
		case LEGACY_NUMPAD5:
			return scancode_down_locked(SDL_SCANCODE_KP_5);
		case LEGACY_DECIMAL:
			return scancode_down_locked(SDL_SCANCODE_KP_PERIOD);
		case KN_KEYPAD_SLASH:
			return scancode_down_locked(SDL_SCANCODE_KP_DIVIDE);
		case KN_KEYPAD_ASTERISK:
			return scancode_down_locked(SDL_SCANCODE_KP_MULTIPLY);
		case KN_KEYPAD_MINUS:
			return scancode_down_locked(SDL_SCANCODE_KP_MINUS);
		case KN_KEYPAD_PLUS:
			return scancode_down_locked(SDL_SCANCODE_KP_PLUS);
		default:
			return false;
	}
}

char translate_ascii_from_legacy_key(uint16_t key)
{
	if ((key & WWKEY_RLS_BIT) != 0) {
		return '\0';
	}

	const bool shift = (key & WWKEY_SHIFT_BIT) != 0;
	const bool ctrl = (key & WWKEY_CTRL_BIT) != 0;
	const bool alt = (key & WWKEY_ALT_BIT) != 0;
	const bool caps = modifier_pressed_locked(SDL_KMOD_CAPS);

	if (ctrl || alt) {
		return '\0';
	}

	const uint16_t base_key = key & 0xFFu;
	if (base_key >= KN_A && base_key <= KN_Z) {
		return static_cast<char>((shift ^ caps) ? ('A' + (base_key - KN_A)) : ('a' + (base_key - KN_A)));
	}
	if (base_key >= KN_0 && base_key <= KN_9) {
		static constexpr char SHIFTED_DIGITS[] = ")!@#$%^&*(";
		return shift ? SHIFTED_DIGITS[base_key - KN_0] : static_cast<char>('0' + (base_key - KN_0));
	}

	switch (base_key) {
		case KN_SPACE: return ' ';
		case KN_TAB: return '\t';
		case KN_RETURN: return '\r';
		case KN_BACKSPACE: return '\b';
		case KN_ESC: return 27;
		case KN_SEMICOLON: return shift ? ':' : ';';
		case KN_EQUAL: return shift ? '+' : '=';
		case KN_COMMA: return shift ? '<' : ',';
		case KN_MINUS: return shift ? '_' : '-';
		case KN_PERIOD: return shift ? '>' : '.';
		case KN_SLASH: return shift ? '?' : '/';
		case KN_GRAVE: return shift ? '~' : '`';
		case KN_LBRACKET: return shift ? '{' : '[';
		case KN_BACKSLASH: return shift ? '|' : '\\';
		case KN_RBRACKET: return shift ? '}' : ']';
		case KN_SQUOTE: return shift ? '"' : '\'';
		default: return '\0';
	}
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

	Main_Window_Handle_Focus_Change(focused);
}

void handle_close_event(void)
{
	if (MainWindow) {
		Main_Window_Handle_Close_Request();
	} else {
		AllDone = TRUE;
	}
}

void initialize_mouse_snapshot_locked(void)
{
	float x = 0.0f;
	float y = 0.0f;
	uint32_t buttons = static_cast<uint32_t>(SDL_GetMouseState(&x, &y));
	int mouse_x = static_cast<int>(x);
	int mouse_y = static_cast<int>(y);
	translate_window_mouse_position(mouse_x, mouse_y);
	update_mouse_position_locked(mouse_x, mouse_y);
	g_mouse_buttons = translate_mouse_button_state(buttons);
}

void handle_sdl_event(const SDL_Event &event);

bool should_capture_callback_event(const SDL_Event& event)
{
	switch (event.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			return true;

		default:
			return false;
	}
}

bool SDLCALL callback_event_watch(void*, SDL_Event* event)
{
	if (event == NULL || !g_app_callbacks_mode.load() || !should_capture_callback_event(*event)) {
		return true;
	}

	std::scoped_lock lock(g_callback_event_mutex);
	g_callback_events.push_back(*event);
	return true;
}

void clear_callback_events_locked(void)
{
	g_callback_events.clear();
}

void drain_callback_events(void)
{
	std::deque<SDL_Event> pending_events;
	{
		std::scoped_lock lock(g_callback_event_mutex);
		pending_events.swap(g_callback_events);
	}

	for (const SDL_Event& event : pending_events) {
		handle_sdl_event(event);
	}
}

void flush_callback_events_from_sdl_queue(void)
{
	SDL_FlushEvent(SDL_EVENT_QUIT);
	SDL_FlushEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
	SDL_FlushEvent(SDL_EVENT_WINDOW_FOCUS_GAINED);
	SDL_FlushEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
	SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
	SDL_FlushEvent(SDL_EVENT_KEY_UP);
	SDL_FlushEvent(SDL_EVENT_MOUSE_MOTION);
	SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
	SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_UP);
}

void set_callback_event_watch(bool enabled)
{
	if (enabled) {
		if (!g_callback_event_watch_installed) {
			SDL_AddEventWatch(callback_event_watch, NULL);
			g_callback_event_watch_installed = true;
		}
		return;
	}

	if (g_callback_event_watch_installed) {
		SDL_RemoveEventWatch(callback_event_watch, NULL);
		g_callback_event_watch_installed = false;
	}

	std::scoped_lock lock(g_callback_event_mutex);
	clear_callback_events_locked();
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
			SDL_GameInput_Handle_Key(event.key.scancode, event.key.key, static_cast<SDL_Keymod>(event.key.mod),
				event.key.repeat, false);
			break;
		}

		case SDL_EVENT_KEY_UP:
		{
			SDL_GameInput_Handle_Key(event.key.scancode, event.key.key, static_cast<SDL_Keymod>(event.key.mod),
				false, true);
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
	update_modifier_state_locked(SDL_GetModState());

	g_main_thread_set.store(true);
}

void SDL_GameInput_Set_AppCallbacks_Mode(bool enabled)
{
	if (enabled && !g_main_thread_set.load()) {
		SDL_GameInput_Set_Main_Thread();
	}

	g_app_callbacks_mode.store(enabled);
	set_callback_event_watch(enabled);
}

bool SDL_GameInput_Is_AppCallbacks_Mode(void)
{
	return (g_app_callbacks_mode.load());
}

void SDL_GameInput_SetTextInputActive(bool enabled)
{
#if defined(__EMSCRIPTEN__)
	// SDL3's Emscripten backend unconditionally registers a default
	// keyboard (SDL_emscriptenvideo.c SDL_AddKeyboard at window creation),
	// so SDL_HasKeyboard() is not a reliable way to detect touch-only
	// devices in the browser. Hand the decision to the JS bridge, which
	// uses navigator.maxTouchPoints / 'ontouchstart' to tell phones and
	// tablets apart from desktops. The bridge is a no-op on devices with
	// a real keyboard, so there is never any keystroke duplication.
	ra_set_web_soft_keyboard_active(enabled ? 1 : 0);
#else
	(void)enabled;
#endif
}

void SDL_GameInput_RequestRedraw(void)
{
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		return;
	}

	WWDraw_Request_Present();
	WWDraw_Flush_Present();
}

void SDL_GameInput_SetMissionMouseCapture(bool enabled)
{
	g_mission_mouse_capture_requested = enabled;
	apply_mission_mouse_capture();
}

void SDL_GameInput_PushMouseCaptureSuppression(void)
{
	++g_mouse_capture_suppression_count;
	apply_mission_mouse_capture();
}

void SDL_GameInput_PopMouseCaptureSuppression(void)
{
	if (g_mouse_capture_suppression_count > 0) {
		--g_mouse_capture_suppression_count;
	}
	apply_mission_mouse_capture();
}

void SDL_GameInput_Handle_Event(const SDL_Event& event)
{
	handle_sdl_event(event);
}

void SDL_GameInput_Delay(uint32_t milliseconds)
{
#if defined(__EMSCRIPTEN__)
	if (g_app_callbacks_mode.load()) {
		emscripten_sleep(static_cast<int>(milliseconds));
		return;
	}
#endif

	SDL_Delay(static_cast<Uint32>(milliseconds));
}

void SDL_GameInput_Handle_Key(SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod mod_state, bool repeat, bool release)
{
	uint16_t legacy_key = legacy_key_from_scancode(scancode);
	if (legacy_key == 0) {
		legacy_key = legacy_key_from_keycode(keycode);
	}

	{
		std::scoped_lock lock(g_input_mutex);
		note_key_event_locked(scancode, mod_state, repeat, release);
	}

	if (legacy_key != 0) {
		Keyboard_Handle_Key_Event(legacy_key, release);
	}
}

void SDL_GameInput_Handle_Mouse_Motion(int x, int y, uint32_t button_state)
{
	translate_window_mouse_position(x, y);
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
	const uint16_t legacy_key = mouse_button_to_legacy_key(button);
	if (legacy_key == 0) {
		return;
	}

	translate_window_mouse_position(x, y);
	int mouse_x = x;
	int mouse_y = y;
	{
		std::scoped_lock lock(g_input_mutex);
		update_mouse_position_locked(x, y);
		uint32_t mask = mouse_button_mask(button);
		if (!release) {
			g_mouse_buttons |= mask;
		} else {
			g_mouse_buttons &= ~mask;
		}
		mouse_x = g_mouse_x;
		mouse_y = g_mouse_y;
	}

	Keyboard_Handle_Mouse_Button_Event(legacy_key, mouse_x, mouse_y, release);
}

void SDL_GameInput_Pump(void)
{
	if (!g_main_thread_set.load()) {
		SDL_GameInput_Set_Main_Thread();
	}

	if (g_main_thread_set.load() && std::this_thread::get_id() != g_main_thread_id) {
		return;
	}

	if (g_is_pumping.exchange(true)) {
		return;
	}

	if (g_app_callbacks_mode.load()) {
		/*
		** Legacy menus, movies, and dialogs still call Call_Back()/Message_Loop()
		** from inside long-lived loops. In callback mode, gather OS events here,
		** translate the copied watch events into the legacy input path, and flush
		** the SDL queue entries so SDL_AppEvent doesn't double-handle them later.
		*/
		SDL_PumpEvents();
		drain_callback_events();
		flush_callback_events_from_sdl_queue();
		g_is_pumping.store(false);
		return;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		handle_sdl_event(event);
	}

	g_is_pumping.store(false);
}

int32_t SDL_GameInput_Wait(void)
{
	if (g_app_callbacks_mode.load()) {
		return (FALSE);
	}

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

int32_t SDL_GameInput_GetCursorPos(POINT* point)
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

void* SDL_GameInput_SetCursor(void* cursor)
{
	return (cursor);
}

int SDL_GameInput_ShowCursor(int32_t show)
{
	const bool was_visible = SDL_CursorVisible();
	if (show) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
	return (was_visible ? 1 : 0);
}

int32_t SDL_GameInput_ClipCursor(const RECT* rect)
{
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		SDL_GameInput_SetCursorClip(rect);
		return (rect == NULL ? TRUE : FALSE);
	}

	const bool wayland_windowed = sdl_uses_wayland_video_driver()
		&& (SDL_GetWindowFlags(MainWindow->sdl_window) & SDL_WINDOW_FULLSCREEN) == 0;

	if (rect == NULL) {
		if (wayland_windowed) {
			SDL_SetWindowMouseRect(MainWindow->sdl_window, NULL);
			SDL_GameInput_SetCursorClip(NULL);
			return (TRUE);
		}

		const int32_t result = SDL_SetWindowMouseRect(MainWindow->sdl_window, NULL) ? TRUE : FALSE;
		if (result) {
			SDL_GameInput_SetCursorClip(NULL);
		}
		return (result);
	}

	RECT surface_rect = *rect;
	RECT viewport_rect{};
	if (SDL_GameInput_GetViewportRect(&viewport_rect)) {
		surface_rect.left += viewport_rect.left;
		surface_rect.top += viewport_rect.top;
		surface_rect.right += viewport_rect.left;
		surface_rect.bottom += viewport_rect.top;
	}

	SDL_Rect mouse_rect{};
	if (!RA_GameRectToWindowRect(MainWindow, &surface_rect, &mouse_rect)) {
		mouse_rect.x = static_cast<int>(surface_rect.left);
		mouse_rect.y = static_cast<int>(surface_rect.top);
		mouse_rect.w = std::max<int>(0, surface_rect.right - surface_rect.left);
		mouse_rect.h = std::max<int>(0, surface_rect.bottom - surface_rect.top);
	}

	if (mouse_rect.w <= 0 || mouse_rect.h <= 0) {
		SDL_SetWindowMouseRect(MainWindow->sdl_window, NULL);
		SDL_GameInput_SetCursorClip(rect);
		return (TRUE);
	}

	if (wayland_windowed) {
		/*
		** Hyprland/Wayland compositor move/resize gestures rely on the real
		** pointer remaining unconstrained. Keep only the game's logical clip.
		*/
		SDL_SetWindowMouseRect(MainWindow->sdl_window, NULL);
		SDL_GameInput_SetCursorClip(rect);
		return (TRUE);
	}

	if (!SDL_SetWindowMouseRect(MainWindow->sdl_window, &mouse_rect)) {
		return (FALSE);
	}

	float x = 0.0f;
	float y = 0.0f;
	SDL_GetMouseState(&x, &y);
	const float clamped_x = std::clamp(x, static_cast<float>(mouse_rect.x), static_cast<float>(mouse_rect.x + mouse_rect.w - (mouse_rect.w > 0 ? 1 : 0)));
	const float clamped_y = std::clamp(y, static_cast<float>(mouse_rect.y), static_cast<float>(mouse_rect.y + mouse_rect.h - (mouse_rect.h > 0 ? 1 : 0)));
	if (clamped_x != x || clamped_y != y) {
		SDL_WarpMouseInWindow(MainWindow->sdl_window, clamped_x, clamped_y);
	}

	SDL_GameInput_SetCursorClip(rect);
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

int32_t SDL_GameInput_GetViewportRect(RECT* rect)
{
	if (rect == NULL) {
		return (FALSE);
	}

	*rect = RECT{};
	return (get_viewport_rect(*rect) ? TRUE : FALSE);
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

bool SDL_GameInput_IsModifierPressed(SDL_Keymod mod_mask)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return modifier_pressed_locked(mod_mask);
}

bool SDL_GameInput_IsModifierToggled(SDL_Keymod mod_mask)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return (g_mod_state & mod_mask) != 0;
}

bool SDL_GameInput_IsScancodePressed(SDL_Scancode scancode)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return scancode_down_locked(scancode);
}

bool SDL_GameInput_IsMouseButtonPressed(uint8_t button)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return mouse_button_down_locked(button);
}

bool SDL_GameInput_IsLegacyKeyPressed(uint16_t key)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return legacy_key_down_locked(key);
}

char SDL_GameInput_KeyToASCII(uint16_t key)
{
	SDL_GameInput_Maybe_Pump();
	std::scoped_lock lock(g_input_mutex);
	return translate_ascii_from_legacy_key(key);
}

int16_t SDL_GameInput_AsciiToLegacyKey(uint8_t ch)
{
	if (ch >= 'a' && ch <= 'z') {
		return static_cast<int16_t>(KN_A + static_cast<uint16_t>(ch - 'a'));
	}
	if (ch >= 'A' && ch <= 'Z') {
		return static_cast<int16_t>(WWKEY_SHIFT_BIT | (KN_A + static_cast<uint16_t>(ch - 'A')));
	}
	if (ch >= '0' && ch <= '9') {
		return static_cast<int16_t>(KN_0 + static_cast<uint16_t>(ch - '0'));
	}

	switch (ch) {
		case ' ': return KN_SPACE;
		case ';': return KN_SEMICOLON;
		case ':': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_SEMICOLON);
		case '=': return KN_EQUAL;
		case '+': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_EQUAL);
		case ',': return KN_COMMA;
		case '<': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_COMMA);
		case '-': return KN_MINUS;
		case '_': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_MINUS);
		case '.': return KN_PERIOD;
		case '>': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_PERIOD);
		case '/': return KN_SLASH;
		case '?': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_SLASH);
		case '`': return KN_GRAVE;
		case '~': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_GRAVE);
		case '[': return KN_LBRACKET;
		case '{': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_LBRACKET);
		case '\\': return KN_BACKSLASH;
		case '|': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_BACKSLASH);
		case ']': return KN_RBRACKET;
		case '}': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_RBRACKET);
		case '\'': return KN_SQUOTE;
		case '"': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_SQUOTE);
		case '!': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_1);
		case '@': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_2);
		case '#': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_3);
		case '$': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_4);
		case '%': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_5);
		case '^': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_6);
		case '&': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_7);
		case '*': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_8);
		case '(': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_9);
		case ')': return static_cast<int16_t>(WWKEY_SHIFT_BIT | KN_0);
		default: return static_cast<int16_t>(ch);
	}
}
