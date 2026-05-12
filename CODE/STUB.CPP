/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define SDL_MAIN_USE_CALLBACKS 1

#include <cstdarg>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>

#include "SDLINPUT.H"
#include "sdl_fs.h"

extern int RedAlert_Main(int argc, char* argv[]);

namespace {

#if defined(SDL_PLATFORM_WIIU)
constexpr char kWiiUDefaultGameDataPath[] = "/vol/external01/redalert";
#endif

struct CallbackAppState
{
	std::vector<std::string> arguments;
	std::vector<char*> argv;
	bool main_started = false;
	bool main_finished = false;
	int exit_code = EXIT_FAILURE;
};

void Startup_Log(SDL_LogPriority priority, const char* format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, priority, format, arguments);
	va_end(arguments);
}

void Build_Startup_Arguments(CallbackAppState& state, int argc, char* argv[])
{
	state.arguments.clear();
	state.argv.clear();

#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
	std::string executable_path = WWFS_GetBaseDirectoryPath();
	if (executable_path.empty() || executable_path == ".") {
		executable_path = "/redalert";
	} else if (executable_path == "/") {
		executable_path = "/redalert";
	} else {
		executable_path += "/redalert";
	}
	state.arguments.emplace_back(executable_path);
#else
	const char* base_path = SDL_GetBasePath();
	state.arguments.emplace_back((base_path != NULL && base_path[0] != '\0') ? base_path : "./");
#endif

	for (int index = 1; index < argc; ++index) {
		state.arguments.emplace_back(argv[index] != NULL ? argv[index] : "");
	}

#if defined(SDL_PLATFORM_WIIU)
	bool has_gamedata_override = false;
	for (int index = 1; index < argc; ++index) {
		const char* argument = argv[index];
		if (argument != NULL && SDL_strcasecmp(argument, "-gamedata") == 0) {
			has_gamedata_override = true;
			break;
		}
	}

	if (!has_gamedata_override) {
		state.arguments.emplace_back("-gamedata");
		state.arguments.emplace_back(kWiiUDefaultGameDataPath);
	}
#endif

	state.argv.reserve(state.arguments.size());
	for (std::string& argument : state.arguments) {
		state.argv.push_back(argument.data());
	}
}

void Run_RedAlert_Main(CallbackAppState* state)
{
	if (state == NULL) {
		return;
	}

	Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: Run_RedAlert_Main begin argc=%d", static_cast<int>(state->argv.size()));
	state->main_started = true;
	state->exit_code = RedAlert_Main(static_cast<int>(state->argv.size()), state->argv.data());
	state->main_finished = true;
	Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: Run_RedAlert_Main end exit_code=%d", state->exit_code);
}

SDL_AppResult App_Result_From_Exit_Code(int exit_code)
{
	return (exit_code == EXIT_SUCCESS) ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
}

} // namespace

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char* argv[])
{
	CallbackAppState* state = new CallbackAppState();
#if defined(__EMSCRIPTEN__) && RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA
	WWFS_SetBaseDirectory("/persistent");
	if (!WWFS_InitializeEmscriptenAssetCache()) {
		delete state;
		return SDL_APP_FAILURE;
	}
#endif
	Build_Startup_Arguments(*state, argc, argv);
	Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: SDL_AppInit argc=%d synthesized_argc=%d", argc, static_cast<int>(state->argv.size()));
	for (size_t index = 0; index < state->arguments.size(); ++index) {
		Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: argv[%d]=\"%s\"", static_cast<int>(index), state->arguments[index].c_str());
	}

	SDL_GameInput_Set_Main_Thread();
	SDL_GameInput_Set_AppCallbacks_Mode(true);

	*appstate = state;

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
	CallbackAppState* state = static_cast<CallbackAppState*>(appstate);
	if (state == NULL) {
		return SDL_APP_FAILURE;
	}

	if (!state->main_started) {
		Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: SDL_AppIterate starting main");
		Run_RedAlert_Main(state);
	}

	if (state->main_finished) {
		return App_Result_From_Exit_Code(state->exit_code);
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event*)
{
	CallbackAppState* state = static_cast<CallbackAppState*>(appstate);
	/*
	** The legacy runtime still consumes input from Call_Back()/Message_Loop().
	** SDLINPUT's callback-mode bridge translates pumped SDL events into that
	** path, so the SDL callback entrypoint only needs to report shutdown here.
	*/
	if (state != NULL && state->main_finished) {
		return App_Result_From_Exit_Code(state->exit_code);
	}

	return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult)
{
	CallbackAppState* state = static_cast<CallbackAppState*>(appstate);
	if (state != NULL) {
		Startup_Log(SDL_LOG_PRIORITY_INFO, "startup: SDL_AppQuit exit_code=%d main_started=%d main_finished=%d",
			state->exit_code, state->main_started ? 1 : 0, state->main_finished ? 1 : 0);
	}
	SDL_GameInput_Set_AppCallbacks_Mode(false);
	delete state;
}
