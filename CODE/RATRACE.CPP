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

#include "RATRACE.H"

#include <SDL3/SDL_log.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
struct TraceToggleCacheEntry
{
	char const* EnvName;
	int Enabled;
};

TraceToggleCacheEntry TraceToggleCache[] = {
	{NULL, 0},
	{NULL, 0},
	{NULL, 0},
	{NULL, 0},
	{NULL, 0},
	{NULL, 0},
	{NULL, 0},
	{NULL, 0}
};
}

bool RA_Trace_Enabled(char const* env_name)
{
	if (env_name == NULL || env_name[0] == '\0') {
		return(true);
	}

	for (int index = 0; index < static_cast<int>(sizeof(TraceToggleCache) / sizeof(TraceToggleCache[0])); ++index) {
		if (TraceToggleCache[index].EnvName == NULL) {
			char const* env = std::getenv(env_name);
			TraceToggleCache[index].EnvName = env_name;
			TraceToggleCache[index].Enabled = (env != NULL && env[0] != '\0') ? 1 : 0;
			return(TraceToggleCache[index].Enabled != 0);
		}

		if (std::strcmp(TraceToggleCache[index].EnvName, env_name) == 0) {
			return(TraceToggleCache[index].Enabled != 0);
		}
	}

	char const* env = std::getenv(env_name);
	return(env != NULL && env[0] != '\0');
}

void RA_Trace(char const* env_name, char const* tag, char const* format, ...)
{
	if (!RA_Trace_Enabled(env_name) || format == NULL) {
		return;
	}

	char message[1024];
	va_list arguments;
	va_start(arguments, format);
	std::vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);

	if (tag != NULL && tag[0] != '\0') {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[%s] %s", tag, message);
	} else {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
	}
}
