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

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include <string>
#include <vector>

#include "win32_compat.h"

extern int  WinMain(void* instance, void* previous_instance, char* command_line, int command_show);

namespace {

std::string Build_Command_Line(int argc, char* argv[])
{
	std::string command_line;

	for (int index = 1; index < argc; ++index) {
		if (!command_line.empty()) {
			command_line.push_back(' ');
		}
		command_line += argv[index];
	}

	return command_line;
}

} // namespace

int main(int argc, char* argv[])
{
	std::string command_line = Build_Command_Line(argc, argv);
	std::vector<char> command_buffer(command_line.begin(), command_line.end());
	command_buffer.push_back('\0');

	return WinMain(nullptr, nullptr, command_buffer.data(), SW_SHOWNORMAL);
}

#endif
