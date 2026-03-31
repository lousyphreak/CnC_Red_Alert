# Command and Conquer: Red Alert SDL3 Port

current progress is in `docs/PORTING_PROGRESS.md`, use it as reference, and keep it updated as you go. Make sure to reference it regularly to know what to do next. Also keep it up to date with new findings and detailed progress notes.

## Porting rules

- fix include directives in all source files to match correct casing of file names, no symlinks, no "forwarding" headers, just direct includes with correct casing. this is important for case-sensitive filesystems and to avoid confusion later on.
- no functionality change, we want to preserve the original behavior as much as possible, so no refactoring or code cleanup, just a straight port
- use correct size types (e.g. `uint32_t` instead of `unsigned int`) and correct format specifiers (e.g. `%u` for `uint32_t`) in all source files. be aware that the original code is for 32
- use SDL3 for all platform-specific functionality, including file reading, window management, input handling, audio, and rendering. do not use any platform-specific APIs directly.
- do not use any third-party libraries other than SDL3, unless absolutely necessary and approved by the project maintainers.
- the original code is VERY sensitive to the size of types, especially in loading and parsing of the original data. one of the main problems is the `long` type, which is 32-bit on Windows and 64-bit on Linux. make sure to use int32_t/uint32_t where needed.
- do NOT create type aliases to work around old code using `long` or other types, just change the code to use the correct types directly. this will make it easier to keep track of where the issues are and avoid confusion later on.
- remove code that is no longer required on modern systems, like CPU detection, old compiler workarounds, and legacy platform support. but do not remove any code that is still needed for the game to function correctly, even if it's not strictly necessary on modern systems.

## General guidelines

- use asan and ubsan to catch memory errors and undefined behavior during development
- do not stub any functionality, if you port a part of the codebase, make sure the port is **COMPLETE** and fully functional, even if it's not perfect or optimized. we want to have a working port as soon as possible, and then we can improve it later.
- keep the original code structure and organization as much as possible, do not move files around or change the directory structure, just port the code in place.
- keep a state document (e.g. `PORTING_PROGRESS.md`) to track the progress of the port, and update it regularly with detailed notes on what has been done, what is left to do, and any issues or challenges encountered along the way.
- keep a knowledge document (e.g. `PORTING_KNOWLEDGE.md`) to document any important information, insights, or discoveries made during the porting process, such as how certain systems work, any quirks or edge cases discovered, and any useful resources or references found.
- if there is any decision to be made about how to implement something, or if there are any questions or uncertainties about how to proceed, use the `askQuestions` tool.
- the file system code needs to deal with a case sensitive filesystem, so make sure that the lowest level does correct abstraction for both Windows and Linux, and that the upper levels of the code does not need to care.

## Technology used

- SDL3
- CMake

## Building and testing

- use CMake to build the project, and make sure to set up the build system correctly for both Windows and Linux.
- use the original game data files for testing, and make sure to run the game to find any early problems with the port, such as crashes
- use asan and ubsan to catch memory errors and undefined behavior during testing, and fix any issues found as soon as possible.



- **DO NOT COMMIT** - the user will do that
- **DO NOT CHANGE THE ORIGINAL GAME CODE** - unless needed to port to new functionality, like changing the file system to use SDL3, or changing the input handling to use SDL3, but do not change the original game logic or behavior unless absolutely necessary. if you need to change something in the original code, make sure to document it in the `PORTING_PROGRESS.md` and `PORTING_KNOWLEDGE.md` files, and explain why the change was needed and how it was implemented.
