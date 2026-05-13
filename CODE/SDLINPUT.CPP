#include "SDLINPUT.H"

#include "FUNCTION.H"
#include "GBUFFER.H"
#include "KEY.H"
#include "../WIN32LIB/INCLUDE/MOUSE.H"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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
extern bool Debug_Map;
extern bool GameActive;
extern bool InMovie;
extern KeyboardClass* Keyboard;
extern SpecialDialogType SpecialDialog;
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
constexpr int TOUCH_DRAG_SLOP_PIXELS = 6;
constexpr int TOUCH_TAP_SLOP_PIXELS = 18;
constexpr uint64_t TOUCH_DOUBLE_TAP_INTERVAL_MS = 350u;
constexpr int GAMEPAD_STICK_DEADZONE = 8192;
constexpr int GAMEPAD_TRIGGER_THRESHOLD = 16384;
constexpr double GAMEPAD_CURSOR_SPEED_PIXELS_PER_SECOND = 600.0;
constexpr uint64_t GAMEPAD_REPEAT_INITIAL_NS = 250000000ULL;
constexpr uint64_t GAMEPAD_REPEAT_INTERVAL_NS = 85000000ULL;

struct TouchPoint {
	int x = 0;
	int y = 0;
};

struct TouchFingerState {
	bool active = false;
	SDL_FingerID id = 0;
	TouchPoint point{};
	TouchPoint start_point{};
};

enum class TouchGestureMode {
	None,
	SinglePending,
	SingleDrag,
	TwoFingerTap,
	TwoFingerScroll
};

void apply_mission_mouse_capture(void);
bool get_window_point_for_current_game_cursor(float& window_x, float& window_y);
void handle_translated_mouse_motion(int x, int y, uint32_t button_state_mask);
void handle_translated_mouse_button(int button, int x, int y, bool release);
void open_first_available_controller(void);
void reset_controller_runtime_state(bool release_mouse_buttons);

constexpr size_t RAW_JOYSTICK_BUTTON_SNAPSHOT_COUNT = 64u;

enum RawJoystickDpadDirection {
	RAW_JOYSTICK_DPAD_UP = 0,
	RAW_JOYSTICK_DPAD_DOWN,
	RAW_JOYSTICK_DPAD_LEFT,
	RAW_JOYSTICK_DPAD_RIGHT,
	RAW_JOYSTICK_DPAD_COUNT
};

struct RawJoystickProfile {
	int axis_count = 0;
	int button_count = 0;
	int hat_count = 0;
	int left_x_axis = -1;
	int left_y_axis = -1;
	int right_x_axis = -1;
	int right_y_axis = -1;
	int left_trigger_axis = -1;
	int right_trigger_axis = -1;
	int dpad_hat = -1;
	int south_button = -1;
	int east_button = -1;
	int west_button = -1;
	int north_button = -1;
	int back_button = -1;
	int start_button = -1;
	int left_shoulder_button = -1;
	int right_shoulder_button = -1;
	int left_stick_button = -1;
	int right_stick_button = -1;
	int left_trigger_button = -1;
	int right_trigger_button = -1;
	int misc1_button = -1;
	int touchpad_button = -1;
	int dpad_up_button = -1;
	int dpad_down_button = -1;
	int dpad_left_button = -1;
	int dpad_right_button = -1;
};

std::mutex g_input_mutex;
std::array<uint16_t, SDL_SCANCODE_COUNT> g_scancode_down_counts{};
SDL_Keymod g_mod_state = SDL_KMOD_NONE;
int g_mouse_x = 0;
int g_mouse_y = 0;
uint32_t g_mouse_buttons = 0;
std::array<bool, 256> g_virtual_legacy_down{};
double g_relative_mouse_subpixel_x = 0.0;
double g_relative_mouse_subpixel_y = 0.0;
double g_gamepad_cursor_subpixel_x = 0.0;
double g_gamepad_cursor_subpixel_y = 0.0;
bool g_controller_subsystems_initialized = false;
SDL_Gamepad* g_active_gamepad = NULL;
SDL_JoystickID g_active_gamepad_id = 0;
SDL_Joystick* g_active_joystick = NULL;
SDL_JoystickID g_active_joystick_id = 0;
RawJoystickProfile g_active_joystick_profile{};
uint64_t g_last_gamepad_poll_ns = 0;
std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> g_gamepad_button_down{};
std::array<uint64_t, SDL_GAMEPAD_BUTTON_COUNT> g_gamepad_button_repeat_ns{};
std::array<bool, RAW_JOYSTICK_BUTTON_SNAPSHOT_COUNT> g_raw_joystick_button_down{};
std::array<bool, RAW_JOYSTICK_DPAD_COUNT> g_raw_joystick_dpad_down{};
std::array<uint64_t, RAW_JOYSTICK_DPAD_COUNT> g_raw_joystick_dpad_repeat_ns{};
bool g_gamepad_left_trigger_down = false;
bool g_gamepad_right_trigger_down = false;
std::atomic<bool> g_gamepad_activity{false};
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
bool g_mission_relative_mouse_mode_applied = false;
bool g_last_pointer_source_was_direct_touch = false;
uint32_t g_active_direct_touch_contacts = 0;
std::array<TouchFingerState, 4> g_touch_fingers{};
TouchGestureMode g_touch_gesture_mode = TouchGestureMode::None;
SDL_FingerID g_touch_single_finger_id = 0;
bool g_touch_wait_for_all_up = false;
TouchPoint g_touch_multitouch_anchor{};
TouchPoint g_touch_multitouch_point{};
bool g_touch_have_last_movie_tap = false;
uint64_t g_touch_last_movie_tap_tick = 0;
TouchPoint g_touch_last_movie_tap_point{};

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

bool touch_gameplay_gestures_enabled(void)
{
	return GameActive && !Debug_Map && !InMovie;
}

bool gameplay_controller_actions_enabled(void)
{
	return GameActive && SpecialDialog == SDLG_NONE && !Debug_Map && !InMovie;
}

bool virtual_legacy_key_down_locked(uint16_t key)
{
	return g_virtual_legacy_down[key & 0xFFu];
}

void set_virtual_legacy_key_locked(uint16_t key, bool down)
{
	key &= 0xFFu;
	if (key == 0) {
		return;
	}
	g_virtual_legacy_down[key] = down;
}

void clear_virtual_legacy_keys_locked(void)
{
	g_virtual_legacy_down.fill(false);
}

double normalized_gamepad_axis(Sint16 value)
{
	const int axis = static_cast<int>(value);
	const int magnitude = std::abs(axis);
	if (magnitude <= GAMEPAD_STICK_DEADZONE) {
		return 0.0;
	}

	double normalized = static_cast<double>(magnitude - GAMEPAD_STICK_DEADZONE)
		/ static_cast<double>(SDL_JOYSTICK_AXIS_MAX - GAMEPAD_STICK_DEADZONE);
	normalized = std::clamp(normalized, 0.0, 1.0);
	normalized *= normalized;
	return (axis < 0) ? -normalized : normalized;
}

bool initialize_controller_subsystems(void)
{
	if (g_controller_subsystems_initialized) {
		return true;
	}

	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD) && !SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
		return false;
	}

	g_controller_subsystems_initialized = true;
	return true;
}

void queue_gamepad_key_press(uint16_t legacy_key)
{
	legacy_key &= 0xFFu;
	if (legacy_key == 0 || Keyboard == NULL) {
		return;
	}
	Keyboard->Put(legacy_key);
}

void send_gamepad_mouse_button(int button, bool release)
{
	int mouse_x = 0;
	int mouse_y = 0;
	{
		std::scoped_lock lock(g_input_mutex);
		mouse_x = g_mouse_x;
		mouse_y = g_mouse_y;
	}
	handle_translated_mouse_button(button, mouse_x, mouse_y, release);
}

bool raw_joystick_profile_usable(const RawJoystickProfile& profile)
{
	return profile.left_x_axis >= 0 && profile.left_y_axis >= 0
		&& (profile.south_button >= 0 || profile.east_button >= 0 || profile.start_button >= 0
			|| profile.dpad_hat >= 0 || profile.dpad_up_button >= 0);
}

RawJoystickProfile make_generic_raw_joystick_profile(SDL_Joystick* joystick)
{
	RawJoystickProfile profile{};
	if (joystick == NULL) {
		return profile;
	}

	profile.axis_count = SDL_GetNumJoystickAxes(joystick);
	profile.button_count = SDL_GetNumJoystickButtons(joystick);
	profile.hat_count = SDL_GetNumJoystickHats(joystick);

	if (profile.axis_count >= 2) {
		profile.left_x_axis = 0;
		profile.left_y_axis = 1;
	}
	if (profile.axis_count >= 4) {
		profile.right_x_axis = 2;
		profile.right_y_axis = 3;
	}
	if (profile.hat_count > 0) {
		profile.dpad_hat = 0;
	} else if (profile.button_count >= 15) {
		profile.dpad_up_button = 11;
		profile.dpad_down_button = 12;
		profile.dpad_left_button = 13;
		profile.dpad_right_button = 14;
	}
	if (profile.button_count >= 4) {
		profile.south_button = 0;
		profile.east_button = 1;
		profile.west_button = 2;
		profile.north_button = 3;
	}
	if (profile.button_count >= 6) {
		profile.left_shoulder_button = 4;
		profile.right_shoulder_button = 5;
	}
	if (profile.button_count >= 8) {
		profile.left_trigger_button = 6;
		profile.right_trigger_button = 7;
	}
	if (profile.button_count >= 10) {
		profile.back_button = 8;
		profile.start_button = 9;
	}
	if (profile.button_count >= 12) {
		profile.left_stick_button = 10;
		profile.right_stick_button = 11;
	}
	return profile;
}

RawJoystickProfile select_raw_joystick_profile(SDL_JoystickID instance_id, SDL_Joystick* joystick)
{
	RawJoystickProfile profile = make_generic_raw_joystick_profile(joystick);
	if (joystick == NULL) {
		return profile;
	}

	const char* joystick_name = SDL_GetJoystickName(joystick);
	if (joystick_name == NULL) {
		joystick_name = SDL_GetJoystickNameForID(instance_id);
	}

	if (joystick_name != NULL && SDL_strstr(joystick_name, "Wii U Pro Controller") != NULL) {
		profile.axis_count = SDL_GetNumJoystickAxes(joystick);
		profile.button_count = SDL_GetNumJoystickButtons(joystick);
		profile.hat_count = SDL_GetNumJoystickHats(joystick);
		profile.left_x_axis = 0;
		profile.left_y_axis = 1;
		profile.right_x_axis = 2;
		profile.right_y_axis = 3;
		profile.start_button = 6;
		profile.back_button = 7;
		profile.dpad_up_button = 11;
		profile.dpad_down_button = 12;
		profile.dpad_left_button = 13;
		profile.dpad_right_button = 14;
		profile.east_button = 15;
		profile.south_button = 16;
		profile.north_button = 17;
		profile.west_button = 18;
		profile.left_shoulder_button = 19;
		profile.right_shoulder_button = 20;
		profile.left_trigger_button = 21;
		profile.right_trigger_button = 22;
		profile.left_stick_button = 23;
		profile.right_stick_button = 24;
		profile.dpad_hat = -1;
	}

	return profile;
}

bool raw_joystick_button_pressed(SDL_Joystick* joystick, const RawJoystickProfile& profile, int button)
{
	return joystick != NULL && button >= 0 && button < profile.button_count && SDL_GetJoystickButton(joystick, button);
}

bool raw_joystick_button_was_pressed(int button)
{
	return button >= 0 && button < static_cast<int>(RAW_JOYSTICK_BUTTON_SNAPSHOT_COUNT)
		&& g_raw_joystick_button_down[button];
}

Sint16 raw_joystick_axis_value(SDL_Joystick* joystick, const RawJoystickProfile& profile, int axis)
{
	if (joystick == NULL || axis < 0 || axis >= profile.axis_count) {
		return 0;
	}
	return SDL_GetJoystickAxis(joystick, axis);
}

bool raw_joystick_hat_pressed(SDL_Joystick* joystick, const RawJoystickProfile& profile, uint8_t mask)
{
	return joystick != NULL && profile.dpad_hat >= 0 && profile.dpad_hat < profile.hat_count
		&& (SDL_GetJoystickHat(joystick, profile.dpad_hat) & mask) != 0;
}

bool raw_joystick_dpad_pressed(SDL_Joystick* joystick, const RawJoystickProfile& profile, RawJoystickDpadDirection direction)
{
	switch (direction) {
		case RAW_JOYSTICK_DPAD_UP:
			return raw_joystick_hat_pressed(joystick, profile, SDL_HAT_UP)
				|| raw_joystick_button_pressed(joystick, profile, profile.dpad_up_button);
		case RAW_JOYSTICK_DPAD_DOWN:
			return raw_joystick_hat_pressed(joystick, profile, SDL_HAT_DOWN)
				|| raw_joystick_button_pressed(joystick, profile, profile.dpad_down_button);
		case RAW_JOYSTICK_DPAD_LEFT:
			return raw_joystick_hat_pressed(joystick, profile, SDL_HAT_LEFT)
				|| raw_joystick_button_pressed(joystick, profile, profile.dpad_left_button);
		case RAW_JOYSTICK_DPAD_RIGHT:
			return raw_joystick_hat_pressed(joystick, profile, SDL_HAT_RIGHT)
				|| raw_joystick_button_pressed(joystick, profile, profile.dpad_right_button);
		default:
			return false;
	}
}

bool raw_joystick_trigger_pressed(SDL_Joystick* joystick, const RawJoystickProfile& profile, bool left_trigger)
{
	const int button = left_trigger ? profile.left_trigger_button : profile.right_trigger_button;
	if (raw_joystick_button_pressed(joystick, profile, button)) {
		return true;
	}

	const int axis = left_trigger ? profile.left_trigger_axis : profile.right_trigger_axis;
	return axis >= 0 && raw_joystick_axis_value(joystick, profile, axis) >= GAMEPAD_TRIGGER_THRESHOLD;
}

void move_gamepad_cursor(double delta_seconds, Sint16 axis_x, Sint16 axis_y)
{
	if (delta_seconds <= 0.0) {
		return;
	}

	const double normalized_x = normalized_gamepad_axis(axis_x);
	const double normalized_y = normalized_gamepad_axis(axis_y);
	if (normalized_x == 0.0 && normalized_y == 0.0) {
		return;
	}

	int next_x = 0;
	int next_y = 0;
	uint32_t button_state = 0;
	{
		std::scoped_lock lock(g_input_mutex);
		const double delta_x = normalized_x * GAMEPAD_CURSOR_SPEED_PIXELS_PER_SECOND * delta_seconds;
		const double delta_y = normalized_y * GAMEPAD_CURSOR_SPEED_PIXELS_PER_SECOND * delta_seconds;
		const double total_x = g_gamepad_cursor_subpixel_x + delta_x;
		const double total_y = g_gamepad_cursor_subpixel_y + delta_y;
		const int whole_dx = static_cast<int>(total_x);
		const int whole_dy = static_cast<int>(total_y);
		g_gamepad_cursor_subpixel_x = total_x - static_cast<double>(whole_dx);
		g_gamepad_cursor_subpixel_y = total_y - static_cast<double>(whole_dy);
		if (whole_dx == 0 && whole_dy == 0) {
			return;
		}

		next_x = g_mouse_x + whole_dx;
		next_y = g_mouse_y + whole_dy;
		button_state = g_mouse_buttons;
	}

	handle_translated_mouse_motion(next_x, next_y, button_state);
}

void update_gamepad_scroll_state(Sint16 axis_x, Sint16 axis_y)
{
	const bool enable_scroll = gameplay_controller_actions_enabled();
	const bool scroll_left = enable_scroll && axis_x <= -GAMEPAD_STICK_DEADZONE;
	const bool scroll_right = enable_scroll && axis_x >= GAMEPAD_STICK_DEADZONE;
	const bool scroll_up = enable_scroll && axis_y <= -GAMEPAD_STICK_DEADZONE;
	const bool scroll_down = enable_scroll && axis_y >= GAMEPAD_STICK_DEADZONE;

	std::scoped_lock lock(g_input_mutex);
	set_virtual_legacy_key_locked(KN_LEFT, scroll_left);
	set_virtual_legacy_key_locked(KN_RIGHT, scroll_right);
	set_virtual_legacy_key_locked(KN_UP, scroll_up);
	set_virtual_legacy_key_locked(KN_DOWN, scroll_down);
}

uint16_t gamepad_dpad_action(SDL_GamepadButton button)
{
	if (gameplay_controller_actions_enabled()) {
		switch (button) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				return Options.KeySidebarUp;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				return Options.KeySidebarDown;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
				return Options.KeyPrevious;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				return Options.KeyNext;
			default:
				return 0;
		}
	}

	switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP:
			return KN_UP;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
			return KN_DOWN;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
			return KN_LEFT;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
			return KN_RIGHT;
		default:
			return 0;
	}
}

uint16_t raw_joystick_dpad_action(RawJoystickDpadDirection direction)
{
	if (gameplay_controller_actions_enabled()) {
		switch (direction) {
			case RAW_JOYSTICK_DPAD_UP:
				return Options.KeySidebarUp;
			case RAW_JOYSTICK_DPAD_DOWN:
				return Options.KeySidebarDown;
			case RAW_JOYSTICK_DPAD_LEFT:
				return Options.KeyPrevious;
			case RAW_JOYSTICK_DPAD_RIGHT:
				return Options.KeyNext;
			default:
				return 0;
		}
	}

	switch (direction) {
		case RAW_JOYSTICK_DPAD_UP:
			return KN_UP;
		case RAW_JOYSTICK_DPAD_DOWN:
			return KN_DOWN;
		case RAW_JOYSTICK_DPAD_LEFT:
			return KN_LEFT;
		case RAW_JOYSTICK_DPAD_RIGHT:
			return KN_RIGHT;
		default:
			return 0;
	}
}

void repeat_gamepad_button_action(SDL_GamepadButton button, bool current_down, uint64_t now_ns)
{
	if (!current_down) {
		g_gamepad_button_repeat_ns[button] = 0;
		return;
	}

	const bool was_down = g_gamepad_button_down[button];
	if (!was_down) {
		queue_gamepad_key_press(gamepad_dpad_action(button));
		g_gamepad_button_repeat_ns[button] = now_ns + GAMEPAD_REPEAT_INITIAL_NS;
		return;
	}

	if (g_gamepad_button_repeat_ns[button] == 0 || now_ns < g_gamepad_button_repeat_ns[button]) {
		return;
	}

	queue_gamepad_key_press(gamepad_dpad_action(button));
	g_gamepad_button_repeat_ns[button] += GAMEPAD_REPEAT_INTERVAL_NS;
	if (g_gamepad_button_repeat_ns[button] < now_ns) {
		g_gamepad_button_repeat_ns[button] = now_ns + GAMEPAD_REPEAT_INTERVAL_NS;
	}
}

void repeat_raw_joystick_dpad_action(RawJoystickDpadDirection direction, bool current_down, uint64_t now_ns)
{
	const size_t index = static_cast<size_t>(direction);
	if (!current_down) {
		g_raw_joystick_dpad_repeat_ns[index] = 0;
		return;
	}

	if (!g_raw_joystick_dpad_down[index]) {
		queue_gamepad_key_press(raw_joystick_dpad_action(direction));
		g_raw_joystick_dpad_repeat_ns[index] = now_ns + GAMEPAD_REPEAT_INITIAL_NS;
		return;
	}

	if (g_raw_joystick_dpad_repeat_ns[index] == 0 || now_ns < g_raw_joystick_dpad_repeat_ns[index]) {
		return;
	}

	queue_gamepad_key_press(raw_joystick_dpad_action(direction));
	g_raw_joystick_dpad_repeat_ns[index] += GAMEPAD_REPEAT_INTERVAL_NS;
	if (g_raw_joystick_dpad_repeat_ns[index] < now_ns) {
		g_raw_joystick_dpad_repeat_ns[index] = now_ns + GAMEPAD_REPEAT_INTERVAL_NS;
	}
}

void prime_gamepad_button_snapshot(SDL_Gamepad* gamepad)
{
	g_gamepad_button_down.fill(false);
	g_gamepad_button_repeat_ns.fill(0);
	g_gamepad_left_trigger_down = false;
	g_gamepad_right_trigger_down = false;
	g_last_gamepad_poll_ns = SDL_GetTicksNS();
	g_gamepad_cursor_subpixel_x = 0.0;
	g_gamepad_cursor_subpixel_y = 0.0;

	if (gamepad == NULL) {
		return;
	}

	for (int index = 0; index < SDL_GAMEPAD_BUTTON_COUNT; ++index) {
		g_gamepad_button_down[index] = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(index));
	}
	g_gamepad_left_trigger_down = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >= GAMEPAD_TRIGGER_THRESHOLD;
	g_gamepad_right_trigger_down = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >= GAMEPAD_TRIGGER_THRESHOLD;
}

void prime_raw_joystick_snapshot(SDL_Joystick* joystick)
{
	g_raw_joystick_button_down.fill(false);
	g_raw_joystick_dpad_down.fill(false);
	g_raw_joystick_dpad_repeat_ns.fill(0);
	g_gamepad_left_trigger_down = false;
	g_gamepad_right_trigger_down = false;
	g_last_gamepad_poll_ns = SDL_GetTicksNS();
	g_gamepad_cursor_subpixel_x = 0.0;
	g_gamepad_cursor_subpixel_y = 0.0;

	if (joystick == NULL) {
		return;
	}

	const int button_limit = std::min(g_active_joystick_profile.button_count,
		static_cast<int>(RAW_JOYSTICK_BUTTON_SNAPSHOT_COUNT));
	for (int index = 0; index < button_limit; ++index) {
		g_raw_joystick_button_down[index] = SDL_GetJoystickButton(joystick, index);
	}
	g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_UP] =
		raw_joystick_dpad_pressed(joystick, g_active_joystick_profile, RAW_JOYSTICK_DPAD_UP);
	g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_DOWN] =
		raw_joystick_dpad_pressed(joystick, g_active_joystick_profile, RAW_JOYSTICK_DPAD_DOWN);
	g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_LEFT] =
		raw_joystick_dpad_pressed(joystick, g_active_joystick_profile, RAW_JOYSTICK_DPAD_LEFT);
	g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_RIGHT] =
		raw_joystick_dpad_pressed(joystick, g_active_joystick_profile, RAW_JOYSTICK_DPAD_RIGHT);
	g_gamepad_left_trigger_down = raw_joystick_trigger_pressed(joystick, g_active_joystick_profile, true);
	g_gamepad_right_trigger_down = raw_joystick_trigger_pressed(joystick, g_active_joystick_profile, false);
}

void handle_gamepad_added(SDL_JoystickID instance_id)
{
	if (!SDL_IsGamepad(instance_id)) {
		return;
	}
	if (g_active_gamepad != NULL) {
		return;
	}
	if (g_active_joystick != NULL) {
		reset_controller_runtime_state(true);
		SDL_CloseJoystick(g_active_joystick);
		g_active_joystick = NULL;
		g_active_joystick_id = 0;
		g_active_joystick_profile = RawJoystickProfile{};
	}

	SDL_Gamepad* gamepad = SDL_OpenGamepad(instance_id);
	if (gamepad == NULL) {
		return;
	}

	g_active_gamepad = gamepad;
	g_active_gamepad_id = instance_id;
	prime_gamepad_button_snapshot(gamepad);
}

void handle_joystick_added(SDL_JoystickID instance_id)
{
	if (g_active_gamepad != NULL || g_active_joystick != NULL || SDL_IsGamepad(instance_id)) {
		return;
	}

	SDL_Joystick* joystick = SDL_OpenJoystick(instance_id);
	if (joystick == NULL) {
		return;
	}

	RawJoystickProfile profile = select_raw_joystick_profile(instance_id, joystick);
	if (!raw_joystick_profile_usable(profile)) {
		SDL_CloseJoystick(joystick);
		return;
	}

	g_active_joystick = joystick;
	g_active_joystick_id = instance_id;
	g_active_joystick_profile = profile;
	prime_raw_joystick_snapshot(joystick);
}

void close_active_gamepad(void)
{
	if (g_active_gamepad == NULL) {
		return;
	}

	SDL_CloseGamepad(g_active_gamepad);
	g_active_gamepad = NULL;
	g_active_gamepad_id = 0;
}

void close_active_joystick(void)
{
	if (g_active_joystick == NULL) {
		return;
	}

	SDL_CloseJoystick(g_active_joystick);
	g_active_joystick = NULL;
	g_active_joystick_id = 0;
	g_active_joystick_profile = RawJoystickProfile{};
}

void reset_controller_runtime_state(bool release_mouse_buttons)
{
	if (release_mouse_buttons) {
		if (g_gamepad_button_down[SDL_GAMEPAD_BUTTON_SOUTH]) {
			send_gamepad_mouse_button(SDL_BUTTON_LEFT, true);
		}
		if (g_gamepad_button_down[SDL_GAMEPAD_BUTTON_EAST]) {
			send_gamepad_mouse_button(SDL_BUTTON_RIGHT, true);
		}
		if (raw_joystick_button_was_pressed(g_active_joystick_profile.south_button)) {
			send_gamepad_mouse_button(SDL_BUTTON_LEFT, true);
		}
		if (raw_joystick_button_was_pressed(g_active_joystick_profile.east_button)) {
			send_gamepad_mouse_button(SDL_BUTTON_RIGHT, true);
		}
	}

	{
		std::scoped_lock lock(g_input_mutex);
		clear_virtual_legacy_keys_locked();
	}
	g_gamepad_button_down.fill(false);
	g_gamepad_button_repeat_ns.fill(0);
	g_raw_joystick_button_down.fill(false);
	g_raw_joystick_dpad_down.fill(false);
	g_raw_joystick_dpad_repeat_ns.fill(0);
	g_gamepad_left_trigger_down = false;
	g_gamepad_right_trigger_down = false;
	g_gamepad_activity.store(false);
	g_last_gamepad_poll_ns = SDL_GetTicksNS();
	g_gamepad_cursor_subpixel_x = 0.0;
	g_gamepad_cursor_subpixel_y = 0.0;
}

void handle_gamepad_removed(SDL_JoystickID instance_id)
{
	if (g_active_gamepad == NULL || g_active_gamepad_id != instance_id) {
		return;
	}

	reset_controller_runtime_state(true);
	close_active_gamepad();
	open_first_available_controller();
}

void handle_joystick_removed(SDL_JoystickID instance_id)
{
	if (g_active_joystick == NULL || g_active_joystick_id != instance_id) {
		return;
	}

	reset_controller_runtime_state(true);
	close_active_joystick();
	open_first_available_controller();
}

void open_first_available_controller(void)
{
	if (g_active_gamepad != NULL || g_active_joystick != NULL || !initialize_controller_subsystems()) {
		return;
	}

	int count = 0;
	SDL_JoystickID* ids = SDL_GetGamepads(&count);
	if (ids != NULL) {
		for (int index = 0; index < count; ++index) {
			if (SDL_IsGamepad(ids[index])) {
				handle_gamepad_added(ids[index]);
				if (g_active_gamepad != NULL) {
					break;
				}
			}
		}
		SDL_free(ids);
	}

	if (g_active_gamepad != NULL) {
		return;
	}

	ids = SDL_GetJoysticks(&count);
	if (ids == NULL) {
		return;
	}

	for (int index = 0; index < count; ++index) {
		if (!SDL_IsGamepad(ids[index])) {
			handle_joystick_added(ids[index]);
			if (g_active_joystick != NULL) {
				break;
			}
		}
	}
	SDL_free(ids);
}

void update_active_controller(void)
{
	if (g_active_gamepad == NULL && g_active_joystick == NULL) {
		open_first_available_controller();
		if (g_active_gamepad == NULL && g_active_joystick == NULL) {
			return;
		}
	}

	if (g_active_gamepad != NULL && !SDL_GamepadConnected(g_active_gamepad)) {
		reset_controller_runtime_state(true);
		close_active_gamepad();
		open_first_available_controller();
		if (g_active_gamepad == NULL && g_active_joystick == NULL) {
			return;
		}
	}

	if (g_active_joystick != NULL && !SDL_JoystickConnected(g_active_joystick)) {
		reset_controller_runtime_state(true);
		close_active_joystick();
		open_first_available_controller();
		if (g_active_gamepad == NULL && g_active_joystick == NULL) {
			return;
		}
	}

	const uint64_t now_ns = SDL_GetTicksNS();
	const uint64_t previous_ns = g_last_gamepad_poll_ns;
	g_last_gamepad_poll_ns = now_ns;
	const double delta_seconds = (previous_ns != 0 && now_ns >= previous_ns)
		? (static_cast<double>(now_ns - previous_ns) / 1000000000.0)
		: 0.0;

	if (g_active_gamepad == NULL && g_active_joystick != NULL) {
		const Sint16 left_x = raw_joystick_axis_value(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.left_x_axis);
		const Sint16 left_y = raw_joystick_axis_value(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.left_y_axis);
		const Sint16 right_x = raw_joystick_axis_value(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.right_x_axis);
		const Sint16 right_y = raw_joystick_axis_value(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.right_y_axis);

		move_gamepad_cursor(delta_seconds, left_x, left_y);
		update_gamepad_scroll_state(right_x, right_y);

		const bool south_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.south_button);
		const bool east_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.east_button);
		if (south_down != raw_joystick_button_was_pressed(g_active_joystick_profile.south_button)) {
			send_gamepad_mouse_button(SDL_BUTTON_LEFT, !south_down);
		}
		if (east_down != raw_joystick_button_was_pressed(g_active_joystick_profile.east_button)) {
			send_gamepad_mouse_button(SDL_BUTTON_RIGHT, !east_down);
		}

		const bool gameplay_actions = gameplay_controller_actions_enabled();
		const bool left_shoulder_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.left_shoulder_button);
		const bool right_shoulder_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.right_shoulder_button);
		{
			std::scoped_lock lock(g_input_mutex);
			set_virtual_legacy_key_locked(Options.KeyForceMove1, gameplay_actions && left_shoulder_down);
			set_virtual_legacy_key_locked(Options.KeyForceAttack1, gameplay_actions && right_shoulder_down);
		}

		const bool dpad_up_down = raw_joystick_dpad_pressed(g_active_joystick, g_active_joystick_profile,
			RAW_JOYSTICK_DPAD_UP);
		const bool dpad_down_down = raw_joystick_dpad_pressed(g_active_joystick, g_active_joystick_profile,
			RAW_JOYSTICK_DPAD_DOWN);
		const bool dpad_left_down = raw_joystick_dpad_pressed(g_active_joystick, g_active_joystick_profile,
			RAW_JOYSTICK_DPAD_LEFT);
		const bool dpad_right_down = raw_joystick_dpad_pressed(g_active_joystick, g_active_joystick_profile,
			RAW_JOYSTICK_DPAD_RIGHT);
		repeat_raw_joystick_dpad_action(RAW_JOYSTICK_DPAD_UP, dpad_up_down, now_ns);
		repeat_raw_joystick_dpad_action(RAW_JOYSTICK_DPAD_DOWN, dpad_down_down, now_ns);
		repeat_raw_joystick_dpad_action(RAW_JOYSTICK_DPAD_LEFT, dpad_left_down, now_ns);
		repeat_raw_joystick_dpad_action(RAW_JOYSTICK_DPAD_RIGHT, dpad_right_down, now_ns);

		const bool start_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.start_button);
		if (start_down && !raw_joystick_button_was_pressed(g_active_joystick_profile.start_button)) {
			queue_gamepad_key_press(Options.KeyOption1);
		}

		const bool back_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.back_button);
		if (back_down && !raw_joystick_button_was_pressed(g_active_joystick_profile.back_button)) {
			queue_gamepad_key_press(gameplay_actions ? Options.KeyScatter : KN_ESC);
		}

		const bool west_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.west_button);
		if (gameplay_actions && west_down && !raw_joystick_button_was_pressed(g_active_joystick_profile.west_button)) {
			queue_gamepad_key_press(Options.KeyStop);
		}

		const bool north_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.north_button);
		if (gameplay_actions && north_down && !raw_joystick_button_was_pressed(g_active_joystick_profile.north_button)) {
			queue_gamepad_key_press(Options.KeyGuard);
		}

		const bool left_stick_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.left_stick_button);
		if (gameplay_actions && left_stick_down
			&& !raw_joystick_button_was_pressed(g_active_joystick_profile.left_stick_button)) {
			queue_gamepad_key_press(Options.KeyBase);
		}

		const bool right_stick_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.right_stick_button);
		if (gameplay_actions && right_stick_down
			&& !raw_joystick_button_was_pressed(g_active_joystick_profile.right_stick_button)) {
			queue_gamepad_key_press(Options.KeyMap);
		}

		const bool misc1_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.misc1_button);
		if (gameplay_actions && misc1_down && !raw_joystick_button_was_pressed(g_active_joystick_profile.misc1_button)) {
			queue_gamepad_key_press(Options.KeyFormation);
		}

		const bool touchpad_down = raw_joystick_button_pressed(g_active_joystick, g_active_joystick_profile,
			g_active_joystick_profile.touchpad_button);
		if (gameplay_actions && touchpad_down
			&& !raw_joystick_button_was_pressed(g_active_joystick_profile.touchpad_button)) {
			queue_gamepad_key_press(Options.KeySelectView);
		}

		const bool left_trigger_down = raw_joystick_trigger_pressed(g_active_joystick, g_active_joystick_profile, true);
		if (gameplay_actions && left_trigger_down && !g_gamepad_left_trigger_down) {
			queue_gamepad_key_press(Options.KeyRepair);
		}

		const bool right_trigger_down = raw_joystick_trigger_pressed(g_active_joystick, g_active_joystick_profile, false);
		if (gameplay_actions && right_trigger_down && !g_gamepad_right_trigger_down) {
			queue_gamepad_key_press(Options.KeySell);
		}

		g_gamepad_left_trigger_down = left_trigger_down;
		g_gamepad_right_trigger_down = right_trigger_down;
		if (std::abs(static_cast<int>(left_x)) >= GAMEPAD_STICK_DEADZONE
			|| std::abs(static_cast<int>(left_y)) >= GAMEPAD_STICK_DEADZONE
			|| std::abs(static_cast<int>(right_x)) >= GAMEPAD_STICK_DEADZONE
			|| std::abs(static_cast<int>(right_y)) >= GAMEPAD_STICK_DEADZONE
			|| south_down
			|| east_down
			|| west_down
			|| north_down
			|| back_down
			|| start_down
			|| left_stick_down
			|| right_stick_down
			|| left_trigger_down
			|| right_trigger_down
			|| dpad_up_down
			|| dpad_down_down
			|| dpad_left_down
			|| dpad_right_down
			|| misc1_down
			|| touchpad_down
			|| left_shoulder_down
			|| right_shoulder_down) {
			g_gamepad_activity.store(true);
		}

		const int button_limit = std::min(g_active_joystick_profile.button_count,
			static_cast<int>(RAW_JOYSTICK_BUTTON_SNAPSHOT_COUNT));
		g_raw_joystick_button_down.fill(false);
		for (int index = 0; index < button_limit; ++index) {
			g_raw_joystick_button_down[index] = SDL_GetJoystickButton(g_active_joystick, index);
		}
		g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_UP] = dpad_up_down;
		g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_DOWN] = dpad_down_down;
		g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_LEFT] = dpad_left_down;
		g_raw_joystick_dpad_down[RAW_JOYSTICK_DPAD_RIGHT] = dpad_right_down;
		return;
	}

	const Sint16 left_x = SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
	const Sint16 left_y = SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
	const Sint16 right_x = SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
	const Sint16 right_y = SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_RIGHTY);

	move_gamepad_cursor(delta_seconds, left_x, left_y);
	update_gamepad_scroll_state(right_x, right_y);

	const bool south_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
	const bool east_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_EAST);
	if (south_down != g_gamepad_button_down[SDL_GAMEPAD_BUTTON_SOUTH]) {
		send_gamepad_mouse_button(SDL_BUTTON_LEFT, !south_down);
	}
	if (east_down != g_gamepad_button_down[SDL_GAMEPAD_BUTTON_EAST]) {
		send_gamepad_mouse_button(SDL_BUTTON_RIGHT, !east_down);
	}

	const bool gameplay_actions = gameplay_controller_actions_enabled();
	{
		std::scoped_lock lock(g_input_mutex);
		set_virtual_legacy_key_locked(Options.KeyForceMove1,
			gameplay_actions && SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
		set_virtual_legacy_key_locked(Options.KeyForceAttack1,
			gameplay_actions && SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
	}

	repeat_gamepad_button_action(SDL_GAMEPAD_BUTTON_DPAD_UP,
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP), now_ns);
	repeat_gamepad_button_action(SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN), now_ns);
	repeat_gamepad_button_action(SDL_GAMEPAD_BUTTON_DPAD_LEFT,
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT), now_ns);
	repeat_gamepad_button_action(SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), now_ns);

	const bool start_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_START);
	if (start_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_START]) {
		queue_gamepad_key_press(Options.KeyOption1);
	}

	const bool back_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_BACK);
	if (back_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_BACK]) {
		queue_gamepad_key_press(gameplay_actions ? Options.KeyScatter : KN_ESC);
	}

	const bool west_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_WEST);
	if (gameplay_actions && west_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_WEST]) {
		queue_gamepad_key_press(Options.KeyStop);
	}

	const bool north_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_NORTH);
	if (gameplay_actions && north_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_NORTH]) {
		queue_gamepad_key_press(Options.KeyGuard);
	}

	const bool left_stick_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
	if (gameplay_actions && left_stick_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_LEFT_STICK]) {
		queue_gamepad_key_press(Options.KeyBase);
	}

	const bool right_stick_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
	if (gameplay_actions && right_stick_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_RIGHT_STICK]) {
		queue_gamepad_key_press(Options.KeyMap);
	}

	const bool misc1_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_MISC1);
	if (gameplay_actions && misc1_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_MISC1]) {
		queue_gamepad_key_press(Options.KeyFormation);
	}

	const bool touchpad_down = SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);
	if (gameplay_actions && touchpad_down && !g_gamepad_button_down[SDL_GAMEPAD_BUTTON_TOUCHPAD]) {
		queue_gamepad_key_press(Options.KeySelectView);
	}

	const bool left_trigger_down =
		SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >= GAMEPAD_TRIGGER_THRESHOLD;
	if (gameplay_actions && left_trigger_down && !g_gamepad_left_trigger_down) {
		queue_gamepad_key_press(Options.KeyRepair);
	}

	const bool right_trigger_down =
		SDL_GetGamepadAxis(g_active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >= GAMEPAD_TRIGGER_THRESHOLD;
	if (gameplay_actions && right_trigger_down && !g_gamepad_right_trigger_down) {
		queue_gamepad_key_press(Options.KeySell);
	}

	g_gamepad_left_trigger_down = left_trigger_down;
	g_gamepad_right_trigger_down = right_trigger_down;
	if (std::abs(static_cast<int>(left_x)) >= GAMEPAD_STICK_DEADZONE
		|| std::abs(static_cast<int>(left_y)) >= GAMEPAD_STICK_DEADZONE
		|| std::abs(static_cast<int>(right_x)) >= GAMEPAD_STICK_DEADZONE
		|| std::abs(static_cast<int>(right_y)) >= GAMEPAD_STICK_DEADZONE
		|| south_down
		|| east_down
		|| west_down
		|| north_down
		|| back_down
		|| start_down
		|| left_stick_down
		|| right_stick_down
		|| left_trigger_down
		|| right_trigger_down
		|| g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_UP]
		|| g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_DOWN]
		|| g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_LEFT]
		|| g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_RIGHT]
		|| misc1_down
		|| touchpad_down
		|| SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
		|| SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
		g_gamepad_activity.store(true);
	}

	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_SOUTH] = south_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_EAST] = east_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_WEST] = west_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_NORTH] = north_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_BACK] = back_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_START] = start_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_LEFT_STICK] = left_stick_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_RIGHT_STICK] = right_stick_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_UP] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_DOWN] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_LEFT] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_DPAD_RIGHT] =
		SDL_GetGamepadButton(g_active_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_MISC1] = misc1_down;
	g_gamepad_button_down[SDL_GAMEPAD_BUTTON_TOUCHPAD] = touchpad_down;
}

bool is_direct_touch_device(
#if defined(__EMSCRIPTEN__)
	SDL_TouchID
#else
	SDL_TouchID touch_id
#endif
)
{
#if defined(__EMSCRIPTEN__)
	return true;
#else
	if (touch_id == 0 || touch_id == SDL_MOUSE_TOUCHID) {
		return false;
	}
	return SDL_GetTouchDeviceType(touch_id) == SDL_TOUCH_DEVICE_DIRECT;
#endif
}

void note_direct_touch_contact_begin(SDL_TouchID touch_id)
{
	if (!is_direct_touch_device(touch_id)) {
		return;
	}

	++g_active_direct_touch_contacts;
	g_last_pointer_source_was_direct_touch = true;
	apply_mission_mouse_capture();
}

void note_direct_touch_contact_end(SDL_TouchID touch_id)
{
	if (!is_direct_touch_device(touch_id)) {
		return;
	}

	if (g_active_direct_touch_contacts > 0) {
		--g_active_direct_touch_contacts;
	}
	apply_mission_mouse_capture();
}

void note_mouse_pointer_activity(SDL_MouseID which)
{
	if (which == SDL_TOUCH_MOUSEID) {
		return;
	}

	if (g_last_pointer_source_was_direct_touch) {
		g_last_pointer_source_was_direct_touch = false;
		apply_mission_mouse_capture();
	}
}

TouchFingerState* find_touch_finger(SDL_FingerID id)
{
	for (TouchFingerState& finger : g_touch_fingers) {
		if (finger.active && finger.id == id) {
			return &finger;
		}
	}
	return nullptr;
}

TouchFingerState* assign_touch_finger(SDL_FingerID id)
{
	TouchFingerState* finger = find_touch_finger(id);
	if (finger != nullptr) {
		return finger;
	}

	for (TouchFingerState& slot : g_touch_fingers) {
		if (!slot.active) {
			slot = TouchFingerState{};
			slot.active = true;
			slot.id = id;
			return &slot;
		}
	}
	return nullptr;
}

int active_touch_count(void)
{
	int count = 0;
	for (const TouchFingerState& finger : g_touch_fingers) {
		if (finger.active) {
			++count;
		}
	}
	return count;
}

TouchPoint active_touch_centroid(void)
{
	TouchPoint centroid{};
	int count = 0;

	for (const TouchFingerState& finger : g_touch_fingers) {
		if (!finger.active) {
			continue;
		}
		centroid.x += finger.point.x;
		centroid.y += finger.point.y;
		++count;
	}

	if (count > 0) {
		centroid.x /= count;
		centroid.y /= count;
	}
	return centroid;
}

bool touch_moved_past_threshold(const TouchPoint& from, const TouchPoint& to, int threshold)
{
	const int dx = to.x - from.x;
	const int dy = to.y - from.y;
	return dx * dx + dy * dy > threshold * threshold;
}

void reset_touch_state(void)
{
	g_touch_fingers = {};
	g_touch_gesture_mode = TouchGestureMode::None;
	g_touch_single_finger_id = 0;
	g_touch_wait_for_all_up = false;
	g_active_direct_touch_contacts = 0;
	g_touch_multitouch_anchor = TouchPoint{};
	g_touch_multitouch_point = TouchPoint{};
	g_touch_have_last_movie_tap = false;
	g_touch_last_movie_tap_tick = 0;
	g_touch_last_movie_tap_point = TouchPoint{};
	apply_mission_mouse_capture();
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

bool translate_touch_position(const SDL_TouchFingerEvent& touch_event, TouchPoint& point)
{
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		return false;
	}

	int window_width = 0;
	int window_height = 0;
	if (!SDL_GetWindowSize(MainWindow->sdl_window, &window_width, &window_height) || window_width <= 0 || window_height <= 0) {
		return false;
	}

	const float clamped_x = std::clamp(touch_event.x, 0.0f, 1.0f);
	const float clamped_y = std::clamp(touch_event.y, 0.0f, 1.0f);
	int x = static_cast<int>(clamped_x * static_cast<float>(window_width));
	int y = static_cast<int>(clamped_y * static_cast<float>(window_height));
	x = std::clamp(x, 0, std::max(0, window_width - 1));
	y = std::clamp(y, 0, std::max(0, window_height - 1));
	translate_window_mouse_position(x, y);
	point.x = x;
	point.y = y;
	return true;
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

void handle_translated_mouse_motion(int x, int y, uint32_t button_state_mask)
{
	int mouse_x = x;
	int mouse_y = y;
	{
		std::scoped_lock lock(g_input_mutex);
		update_mouse_position_locked(x, y);
		g_mouse_buttons = button_state_mask;
		mouse_x = g_mouse_x;
		mouse_y = g_mouse_y;
	}

	Keyboard_Handle_Mouse_Motion(mouse_x, mouse_y);
}

void reset_relative_mouse_motion_locked(void)
{
	g_relative_mouse_subpixel_x = 0.0;
	g_relative_mouse_subpixel_y = 0.0;
}

int scale_relative_mouse_delta_x2(int delta)
{
	if (delta < 0) {
		return -((Options.Get_Mouse_Acceleration() * 2) * -delta);
	}
	return((Options.Get_Mouse_Acceleration() * 2) * delta);
}

bool get_relative_mouse_game_scale(double &scale_x, double &scale_y)
{
	scale_x = 1.0;
	scale_y = 1.0;

	SDL_FRect presentation{};
	SDL_FRect source{};
	if (MainWindow == NULL
		|| !RA_GetPresentationRect(MainWindow, &presentation)
		|| !RA_GetRenderSourceRect(MainWindow, &source)
		|| presentation.w <= 0.0f
		|| presentation.h <= 0.0f
		|| source.w <= 0.0f
		|| source.h <= 0.0f) {
		return(false);
	}

	scale_x = static_cast<double>(source.w) / static_cast<double>(presentation.w);
	scale_y = static_cast<double>(source.h) / static_cast<double>(presentation.h);
	return(true);
}

void handle_relative_mouse_motion(int x, int y, int xrel, int yrel, uint32_t button_state_mask)
{
	(void)x;
	(void)y;

	double scale_x = 1.0;
	double scale_y = 1.0;
	get_relative_mouse_game_scale(scale_x, scale_y);

	int translated_x = 0;
	int translated_y = 0;
	{
		std::scoped_lock lock(g_input_mutex);
		double const delta_x = static_cast<double>(scale_relative_mouse_delta_x2(xrel)) * 0.5 * scale_x;
		double const delta_y = static_cast<double>(scale_relative_mouse_delta_x2(yrel)) * 0.5 * scale_y;
		double const total_x = g_relative_mouse_subpixel_x + delta_x;
		double const total_y = g_relative_mouse_subpixel_y + delta_y;
		int const whole_dx = static_cast<int>(total_x);
		int const whole_dy = static_cast<int>(total_y);
		g_relative_mouse_subpixel_x = total_x - static_cast<double>(whole_dx);
		g_relative_mouse_subpixel_y = total_y - static_cast<double>(whole_dy);
		translated_x = g_mouse_x + whole_dx;
		translated_y = g_mouse_y + whole_dy;
	}

	handle_translated_mouse_motion(translated_x, translated_y, button_state_mask);
}

void handle_translated_mouse_button(int button, int x, int y, bool release)
{
	const uint16_t legacy_key = mouse_button_to_legacy_key(button);
	if (legacy_key == 0) {
		return;
	}

	int mouse_x = x;
	int mouse_y = y;
	{
		std::scoped_lock lock(g_input_mutex);
		update_mouse_position_locked(x, y);
		const uint32_t mask = mouse_button_mask(button);
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

TouchPoint touch_scroll_cursor_point(const TouchPoint& actual_point)
{
	TouchPoint scroll_point = actual_point;
	const int width = std::max(0, SeenBuff.Get_Width() - 1);
	const int height = std::max(0, SeenBuff.Get_Height() - 1);
	const int dx = g_touch_multitouch_point.x - g_touch_multitouch_anchor.x;
	const int dy = g_touch_multitouch_point.y - g_touch_multitouch_anchor.y;

	if (dx > TOUCH_DRAG_SLOP_PIXELS) {
		scroll_point.x = width;
	} else if (dx < -TOUCH_DRAG_SLOP_PIXELS) {
		scroll_point.x = 0;
	}

	if (dy > TOUCH_DRAG_SLOP_PIXELS) {
		scroll_point.y = height;
	} else if (dy < -TOUCH_DRAG_SLOP_PIXELS) {
		scroll_point.y = 0;
	}

	return scroll_point;
}

void begin_multitouch_gesture(void)
{
	if (!touch_gameplay_gestures_enabled()) {
		return;
	}

	if (g_touch_gesture_mode == TouchGestureMode::SingleDrag) {
		TouchFingerState* single_finger = find_touch_finger(g_touch_single_finger_id);
		if (single_finger != nullptr) {
			handle_translated_mouse_button(SDL_BUTTON_LEFT, single_finger->point.x, single_finger->point.y, true);
		}
	}

	g_touch_single_finger_id = 0;
	g_touch_wait_for_all_up = true;
	g_touch_gesture_mode = TouchGestureMode::TwoFingerTap;
	g_touch_multitouch_anchor = active_touch_centroid();
	g_touch_multitouch_point = g_touch_multitouch_anchor;
	handle_translated_mouse_motion(g_touch_multitouch_point.x, g_touch_multitouch_point.y, 0);
}

void maybe_begin_touch_drag(const TouchFingerState& finger)
{
	if (g_touch_gesture_mode != TouchGestureMode::SinglePending || finger.id != g_touch_single_finger_id) {
		return;
	}

	if (!touch_moved_past_threshold(finger.start_point, finger.point, TOUCH_DRAG_SLOP_PIXELS)) {
		handle_translated_mouse_motion(finger.point.x, finger.point.y, 0);
		return;
	}

	g_touch_gesture_mode = TouchGestureMode::SingleDrag;
	handle_translated_mouse_button(SDL_BUTTON_LEFT, finger.start_point.x, finger.start_point.y, false);
	handle_translated_mouse_motion(finger.point.x, finger.point.y, MOUSE_LEFT_MASK);
}

void maybe_commit_movie_double_tap(const TouchFingerState& finger, int active_count_before_release, bool canceled)
{
	if (canceled || active_count_before_release != 1 || touch_moved_past_threshold(finger.start_point, finger.point, TOUCH_TAP_SLOP_PIXELS)) {
		return;
	}

	const uint64_t now = SDL_GetTicks();
	if (g_touch_have_last_movie_tap
		&& now - g_touch_last_movie_tap_tick <= TOUCH_DOUBLE_TAP_INTERVAL_MS
		&& !touch_moved_past_threshold(g_touch_last_movie_tap_point, finger.point, TOUCH_TAP_SLOP_PIXELS)) {
		Keyboard_Handle_Key_Event(KN_ESC, false);
		g_touch_have_last_movie_tap = false;
		g_touch_last_movie_tap_tick = 0;
		g_touch_last_movie_tap_point = TouchPoint{};
		return;
	}

	g_touch_have_last_movie_tap = true;
	g_touch_last_movie_tap_tick = now;
	g_touch_last_movie_tap_point = finger.point;
}

void handle_touch_down(const SDL_TouchFingerEvent& touch_event)
{
	note_direct_touch_contact_begin(touch_event.touchID);

	TouchPoint point{};
	if (!translate_touch_position(touch_event, point)) {
		return;
	}

	TouchFingerState* finger = assign_touch_finger(touch_event.fingerID);
	if (finger == nullptr) {
		return;
	}
	finger->point = point;
	finger->start_point = point;

	if (InMovie) {
		return;
	}

	if (touch_gameplay_gestures_enabled() && active_touch_count() == 2) {
		begin_multitouch_gesture();
		return;
	}

	if (g_touch_wait_for_all_up || active_touch_count() != 1) {
		return;
	}

	g_touch_single_finger_id = touch_event.fingerID;
	g_touch_gesture_mode = TouchGestureMode::SinglePending;
	handle_translated_mouse_motion(point.x, point.y, 0);
}

void handle_touch_motion(const SDL_TouchFingerEvent& touch_event)
{
	if (is_direct_touch_device(touch_event.touchID)) {
		g_last_pointer_source_was_direct_touch = true;
	}

	TouchFingerState* finger = find_touch_finger(touch_event.fingerID);
	if (finger == nullptr) {
		return;
	}

	TouchPoint point{};
	if (!translate_touch_position(touch_event, point)) {
		return;
	}
	finger->point = point;

	if (InMovie) {
		return;
	}

	if (g_touch_wait_for_all_up) {
		const int active_count_now = active_touch_count();
		if (active_count_now >= 2) {
			g_touch_multitouch_point = active_touch_centroid();
			if (g_touch_gesture_mode == TouchGestureMode::TwoFingerTap
				&& touch_moved_past_threshold(g_touch_multitouch_anchor, g_touch_multitouch_point, TOUCH_DRAG_SLOP_PIXELS)) {
				g_touch_gesture_mode = TouchGestureMode::TwoFingerScroll;
			}

			if (g_touch_gesture_mode == TouchGestureMode::TwoFingerScroll) {
				const TouchPoint scroll_point = touch_scroll_cursor_point(g_touch_multitouch_point);
				handle_translated_mouse_motion(scroll_point.x, scroll_point.y, 0);
			} else {
				handle_translated_mouse_motion(g_touch_multitouch_point.x, g_touch_multitouch_point.y, 0);
			}
		} else {
			if (g_touch_gesture_mode == TouchGestureMode::TwoFingerTap) {
				g_touch_gesture_mode = TouchGestureMode::None;
			}
			handle_translated_mouse_motion(point.x, point.y, 0);
		}
		return;
	}

	if (touch_event.fingerID != g_touch_single_finger_id) {
		return;
	}

	if (g_touch_gesture_mode == TouchGestureMode::SinglePending) {
		maybe_begin_touch_drag(*finger);
		return;
	}

	if (g_touch_gesture_mode == TouchGestureMode::SingleDrag) {
		handle_translated_mouse_motion(point.x, point.y, MOUSE_LEFT_MASK);
	}
}

void handle_touch_up(const SDL_TouchFingerEvent& touch_event, bool canceled)
{
	TouchFingerState* finger = find_touch_finger(touch_event.fingerID);
	if (finger == nullptr) {
		note_direct_touch_contact_end(touch_event.touchID);
		return;
	}

	TouchPoint point = finger->point;
	translate_touch_position(touch_event, point);
	finger->point = point;

	const TouchFingerState released_finger = *finger;
	const int active_count_before_release = active_touch_count();
	*finger = TouchFingerState{};
	const int active_count_after_release = active_touch_count();

	if (InMovie) {
		maybe_commit_movie_double_tap(released_finger, active_count_before_release, canceled);
		if (active_count_after_release == 0) {
			g_touch_single_finger_id = 0;
			g_touch_gesture_mode = TouchGestureMode::None;
			g_touch_wait_for_all_up = false;
		}
		return;
	}

	if (released_finger.id == g_touch_single_finger_id) {
		if (g_touch_gesture_mode == TouchGestureMode::SinglePending) {
			if (!canceled) {
				handle_translated_mouse_button(SDL_BUTTON_LEFT, released_finger.point.x, released_finger.point.y, false);
				handle_translated_mouse_button(SDL_BUTTON_LEFT, released_finger.point.x, released_finger.point.y, true);
			}
			g_touch_single_finger_id = 0;
			g_touch_gesture_mode = TouchGestureMode::None;
			return;
		}

		if (g_touch_gesture_mode == TouchGestureMode::SingleDrag) {
			handle_translated_mouse_button(SDL_BUTTON_LEFT, released_finger.point.x, released_finger.point.y, true);
			g_touch_single_finger_id = 0;
			g_touch_gesture_mode = TouchGestureMode::None;
			return;
		}
	}

	if (!g_touch_wait_for_all_up) {
		return;
	}

	const TouchPoint restore_point = (active_count_after_release > 0) ? active_touch_centroid() : g_touch_multitouch_point;
	if (!canceled && g_touch_gesture_mode == TouchGestureMode::TwoFingerTap && active_count_after_release == 0) {
		handle_translated_mouse_button(SDL_BUTTON_RIGHT, g_touch_multitouch_point.x, g_touch_multitouch_point.y, false);
		handle_translated_mouse_button(SDL_BUTTON_RIGHT, g_touch_multitouch_point.x, g_touch_multitouch_point.y, true);
	}

	handle_translated_mouse_motion(restore_point.x, restore_point.y, 0);
	if (g_touch_gesture_mode == TouchGestureMode::TwoFingerScroll && active_count_after_release < 2) {
		g_touch_gesture_mode = TouchGestureMode::None;
	}

	if (active_count_after_release == 0) {
		g_touch_wait_for_all_up = false;
		g_touch_gesture_mode = TouchGestureMode::None;
		g_touch_single_finger_id = 0;
	}
	note_direct_touch_contact_end(touch_event.touchID);
}

void initialize_mouse_snapshot_locked(void);

bool should_apply_mission_mouse_capture(void)
{
	return g_mission_mouse_capture_requested
		&& g_mouse_capture_suppression_count == 0
		&& !InMovie
		&& MainWindow != NULL
		&& MainWindow->sdl_window != NULL;
}

bool should_apply_relative_mouse_mode(void)
{
	return !g_last_pointer_source_was_direct_touch && g_active_direct_touch_contacts == 0;
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

#if !defined(__EMSCRIPTEN__)
bool get_window_rect_for_text_input(SDL_Rect& window_rect)
{
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		return false;
	}

	RECT viewport_rect{};
	if (!SDL_GameInput_GetViewportRect(&viewport_rect)) {
		int window_width = 0;
		int window_height = 0;
		if (!SDL_GetWindowSize(MainWindow->sdl_window, &window_width, &window_height)
			|| window_width <= 0
			|| window_height <= 0) {
			return false;
		}

		window_rect.x = 0;
		window_rect.y = 0;
		window_rect.w = window_width;
		window_rect.h = window_height;
		return true;
	}

	if (!RA_GameRectToWindowRect(MainWindow, &viewport_rect, &window_rect)
		|| window_rect.w <= 0
		|| window_rect.h <= 0) {
		window_rect.x = viewport_rect.left;
		window_rect.y = viewport_rect.top;
		window_rect.w = std::max<int>(0, viewport_rect.right - viewport_rect.left);
		window_rect.h = std::max<int>(0, viewport_rect.bottom - viewport_rect.top);
	}

	return window_rect.w > 0 && window_rect.h > 0;
}
#endif

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

EM_JS(int32_t, ra_pop_web_soft_keyboard_event, (), {
    if (typeof Module === 'undefined' || !Module.raSoftKeyboard || !Module.raSoftKeyboard.pendingEvents) {
        return 0;
    }
    if (!Module.raSoftKeyboard.pendingEvents.length) {
        return 0;
    }
    return Module.raSoftKeyboard.pendingEvents.shift() | 0;
});

EM_JS(int32_t, ra_take_web_soft_keyboard_redraw_request, (), {
    if (typeof Module === 'undefined' || !Module.raSoftKeyboard) {
        return 0;
    }
    if (!Module.raSoftKeyboard.pendingRedraw) {
        return 0;
    }
    Module.raSoftKeyboard.pendingRedraw = false;
    return 1;
});

static void push_web_soft_key(uint16_t legacy_key)
{
	Keyboard_Handle_Key_Event(legacy_key, false);
}

enum : uint32_t {
	WEB_SOFT_KEY_EVENT_NONE = 0u,
	WEB_SOFT_KEY_EVENT_TEXT = 1u,
	WEB_SOFT_KEY_EVENT_BACKSPACE = 2u,
	WEB_SOFT_KEY_EVENT_RETURN = 3u
};

void drain_web_soft_keyboard_bridge(void)
{
	while (true) {
		const uint32_t packed = static_cast<uint32_t>(ra_pop_web_soft_keyboard_event());
		if (packed == WEB_SOFT_KEY_EVENT_NONE) {
			break;
		}

		const uint32_t event_type = (packed >> 24) & 0xFFu;
		const uint32_t event_value = packed & 0x00FFFFFFu;

		switch (event_type) {
			case WEB_SOFT_KEY_EVENT_TEXT:
				if (event_value <= 255u) {
					const int16_t legacy_key = SDL_GameInput_AsciiToLegacyKey(static_cast<uint8_t>(event_value));
					if (legacy_key != 0) {
						push_web_soft_key(static_cast<uint16_t>(legacy_key));
					}
				}
				break;

			case WEB_SOFT_KEY_EVENT_BACKSPACE:
				push_web_soft_key(KN_BACKSPACE);
				break;

			case WEB_SOFT_KEY_EVENT_RETURN:
				push_web_soft_key(KN_RETURN);
				break;

			default:
				break;
		}
	}

	if (ra_take_web_soft_keyboard_redraw_request() != 0) {
		SDL_GameInput_RequestRedraw();
	}
}
#endif

void apply_mission_mouse_capture(void)
{
	const bool should_capture = should_apply_mission_mouse_capture();
	const bool should_relative_mode = should_capture && should_apply_relative_mouse_mode();

	if (g_mission_relative_mouse_mode_applied != should_relative_mode) {
		{
			std::scoped_lock lock(g_input_mutex);
			reset_relative_mouse_motion_locked();
		}
		if (MainWindow != NULL && MainWindow->sdl_window != NULL && SDL_GetWindowRelativeMouseMode(MainWindow->sdl_window)) {
			if (!g_last_pointer_source_was_direct_touch) {
				float window_x = 0.0f;
				float window_y = 0.0f;
				if (get_window_point_for_current_game_cursor(window_x, window_y)) {
					SDL_WarpMouseInWindow(MainWindow->sdl_window, window_x, window_y);
				}
			}
			SDL_SetWindowRelativeMouseMode(MainWindow->sdl_window, false);
		}
		if (should_relative_mode && MainWindow != NULL && MainWindow->sdl_window != NULL) {
			SDL_SetWindowRelativeMouseMode(MainWindow->sdl_window, true);
		}
		g_mission_relative_mouse_mode_applied = should_relative_mode;
	}

	if (g_mission_mouse_capture_applied == should_capture) {
		if (should_capture && WWMouse != NULL) {
			WWMouse->Set_Cursor_Clip();
		}
		return;
	}

	if (!should_capture) {
		if (WWMouse != NULL) {
			WWMouse->Clear_Cursor_Clip();
		}
		{
			std::scoped_lock lock(g_input_mutex);
			if (!g_last_pointer_source_was_direct_touch) {
				initialize_mouse_snapshot_locked();
			} else {
				g_mouse_buttons = 0;
			}
		}
		g_mission_mouse_capture_applied = false;
		return;
	}

	if (WWMouse != NULL) {
		WWMouse->Set_Cursor_Clip();
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
	clear_virtual_legacy_keys_locked();
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
	if (virtual_legacy_key_down_locked(key)) {
		return true;
	}

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
	if (!focused) {
		reset_touch_state();
		reset_controller_runtime_state(false);
	} else if (g_active_gamepad != NULL) {
		prime_gamepad_button_snapshot(g_active_gamepad);
	} else if (g_active_joystick != NULL) {
		prime_raw_joystick_snapshot(g_active_joystick);
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
	reset_relative_mouse_motion_locked();
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
		case SDL_EVENT_GAMEPAD_ADDED:
		case SDL_EVENT_GAMEPAD_REMOVED:
		case SDL_EVENT_JOYSTICK_ADDED:
		case SDL_EVENT_JOYSTICK_REMOVED:
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_MOTION:
		case SDL_EVENT_FINGER_CANCELED:
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

		case SDL_EVENT_GAMEPAD_ADDED:
			handle_gamepad_added(event.gdevice.which);
			break;

		case SDL_EVENT_GAMEPAD_REMOVED:
			handle_gamepad_removed(event.gdevice.which);
			break;

		case SDL_EVENT_JOYSTICK_ADDED:
			handle_joystick_added(event.jdevice.which);
			break;

		case SDL_EVENT_JOYSTICK_REMOVED:
			handle_joystick_removed(event.jdevice.which);
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
			note_mouse_pointer_activity(event.motion.which);
			SDL_GameInput_Handle_Mouse_Motion(static_cast<int>(event.motion.x), static_cast<int>(event.motion.y),
				static_cast<int>(event.motion.xrel), static_cast<int>(event.motion.yrel),
				static_cast<uint32_t>(event.motion.state));
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			note_mouse_pointer_activity(event.button.which);
			SDL_GameInput_Handle_Mouse_Button(static_cast<int>(event.button.button), static_cast<int>(event.button.x),
				static_cast<int>(event.button.y), false);
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			note_mouse_pointer_activity(event.button.which);
			SDL_GameInput_Handle_Mouse_Button(static_cast<int>(event.button.button), static_cast<int>(event.button.x),
				static_cast<int>(event.button.y), true);
			break;

		case SDL_EVENT_FINGER_DOWN:
			handle_touch_down(event.tfinger);
			break;

		case SDL_EVENT_FINGER_UP:
			handle_touch_up(event.tfinger, false);
			break;

		case SDL_EVENT_FINGER_MOTION:
			handle_touch_motion(event.tfinger);
			break;

		case SDL_EVENT_FINGER_CANCELED:
			handle_touch_up(event.tfinger, true);
			break;

		default:
			break;
	}
}

}	// namespace

void SDL_GameInput_Set_Main_Thread(void)
{
	g_main_thread_id = std::this_thread::get_id();
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	reset_touch_state();
	initialize_controller_subsystems();
	open_first_available_controller();

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
	if (MainWindow == NULL || MainWindow->sdl_window == NULL) {
		return;
	}

	SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "auto");
	if (enabled) {
		SDL_Rect text_input_rect{};
		if (get_window_rect_for_text_input(text_input_rect)) {
			SDL_SetTextInputArea(MainWindow->sdl_window, &text_input_rect, 0);
		}
		SDL_StartTextInput(MainWindow->sdl_window);
	} else if (SDL_TextInputActive(MainWindow->sdl_window)) {
		SDL_StopTextInput(MainWindow->sdl_window);
	}
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

void SDL_GameInput_Handle_Mouse_Motion(int x, int y, int xrel, int yrel, uint32_t button_state)
{
	if (MainWindow != NULL
		&& MainWindow->sdl_window != NULL
		&& SDL_GetWindowRelativeMouseMode(MainWindow->sdl_window)) {
		handle_relative_mouse_motion(x, y, xrel, yrel, translate_mouse_button_state(button_state));
		return;
	}
	{
		std::scoped_lock lock(g_input_mutex);
		reset_relative_mouse_motion_locked();
	}
	translate_window_mouse_position(x, y);
	handle_translated_mouse_motion(x, y, translate_mouse_button_state(button_state));
}

void SDL_GameInput_Handle_Mouse_Button(int button, int x, int y, bool release)
{
	if (MainWindow != NULL
		&& MainWindow->sdl_window != NULL
		&& SDL_GetWindowRelativeMouseMode(MainWindow->sdl_window)) {
		int translated_x = 0;
		int translated_y = 0;
		{
			std::scoped_lock lock(g_input_mutex);
			translated_x = g_mouse_x;
			translated_y = g_mouse_y;
		}
		handle_translated_mouse_button(button, translated_x, translated_y, release);
		return;
	}

	translate_window_mouse_position(x, y);
	handle_translated_mouse_button(button, x, y, release);
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

#if defined(__EMSCRIPTEN__)
	drain_web_soft_keyboard_bridge();
#endif

	if (g_app_callbacks_mode.load()) {
		/*
		** Legacy menus, movies, and dialogs still call Call_Back()/Message_Loop()
		** from inside long-lived loops. In callback mode, gather OS events here,
		** translate the copied watch events into the legacy input path, and drain
		** the SDL queue entries so SDL_AppEvent doesn't double-handle them later.
		*/
		SDL_PumpEvents();
		drain_callback_events();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
		}
		update_active_controller();
		g_is_pumping.store(false);
		return;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		handle_sdl_event(event);
	}
	update_active_controller();

#if defined(__EMSCRIPTEN__)
	drain_web_soft_keyboard_bridge();
#endif

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
	update_active_controller();

#if defined(__EMSCRIPTEN__)
	drain_web_soft_keyboard_bridge();
#endif

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

	const bool relative_mode = SDL_GetWindowRelativeMouseMode(MainWindow->sdl_window);
	const bool wayland_windowed = sdl_uses_wayland_video_driver()
		&& (SDL_GetWindowFlags(MainWindow->sdl_window) & SDL_WINDOW_FULLSCREEN) == 0;

	if (rect == NULL) {
		if (relative_mode || wayland_windowed) {
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

	if (relative_mode) {
		/*
		** Relative mode already owns pointer capture. Applying an SDL window
		** mouse rect here can stop further relative deltas at the clipped edge
		** (notably on Emscripten), which breaks border scrolling and creates a
		** matching dead zone when the player moves back inward.
		*/
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

bool SDL_GameInput_ConsumeGamepadActivity(void)
{
	SDL_GameInput_Maybe_Pump();
	return g_gamepad_activity.exchange(false);
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
