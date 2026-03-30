# Porting Progress

_Last updated: 2026-03-29_

## Goal

Port the Red Alert codebase to a reproducible cross-platform build using SDL3 for platform-specific functionality, with working builds on Linux, Windows, and other supported SDL3 platforms.

## Current status

- The reconstructed CMake/SDL3 build is now able to compile and link the full `redalert` executable on Linux.
- `cmake --build build --target redalert -j4` completes successfully.
- `cmake --build build-asan --target redalert -j4` completes successfully with ASan/UBSan enabled.
- The Linux build now enters the real game startup path instead of the old Unix stub path:
  - `CODE/STUB.CPP` bridges Linux `main()` into `WinMain(...)`.
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` returns a real executable path from `GetModuleFileName()`.
- The original startup crash in `DdeInitialize()` is fixed by making the DDE compatibility state lazy/process-lifetime instead of relying on file-scope mutable globals during static initialization.
- The previously blocked movie layer is still wired into the active build through the portable `VQ/VQA32` path instead of the old unresolved Win32-only movie stack.
- The active VQA path now includes:
  - the buffered `UnVQ_4x2` decoder replacement in `VQ/VQA32/UNVQCOMPAT.CPP`;
  - the audio decompressor replacement in `VQ/VQA32/AUDUNZAPCOMPAT.CPP`;
  - palette/text/INI/VBlank compatibility glue in `VQ/VQA32/VQCOMPAT.CPP`;
  - an SDL3-backed movie audio/timer implementation in `VQ/VQA32/AUDIOCOMPAT.CPP`.
- The non-portable DirectShow/DirectDraw MPEG path is now disabled by leaving `MPEGMOVIE` off in `CODE/DEFINES.H`. The buffered VQA path remains the active movie implementation.
- Case-sensitive include fixes and DOS-header cleanup have been applied across the active `VQ/VQA32` and `VQ/INCLUDE/VQM32` surface so the portable movie sources build cleanly on Linux.
- The runtime now reaches the real front-end path from `GameData` in both normal and ASan builds instead of dying in early startup.
- The SDL3 window is no longer stuck black after startup:
  - the DirectDraw compatibility layer now presents primary-surface updates on `Unlock()`, `Blt()`, and palette attachment;
  - a live Hyprland capture of the rebuilt game window shows substantial non-black content instead of an all-black frame.
- Audio is active in the live SDL3 build:
  - the game opens a real PipeWire sink input while the menu/front-end is running;
  - a short capture from the default sink monitor shows non-silent PCM output.
- A compositor close request now shuts the game down cleanly through the SDL/Win32 message bridge instead of leaving the process hung until `SIGKILL`.
- Validation status:
  - `cmake --build build --target redalert -j4` passes.
  - `cmake --build build -j4` passes.
  - `cmake --build build-asan --target redalert -j4` passes.
  - Running `GameData/redalert` reaches a live `640x480` `Red Alert` window with visible framebuffer updates.
  - A captured window image from the rebuilt normal run reports `mean_rgb=22,6,6`, `nonblack_ratio=0.444277`, `bright_ratio=0.365957`.
  - A short monitor capture from the rebuilt normal run reports `abs_mean=1224.67`, `rms=2406.59`, `peak=21253`, `nonzero_ratio=0.808433`.
  - Running `GameData/redalert-asan` with `ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:new_delete_type_mismatch=0` reaches the same front-end baseline, handles window close, and emits no sanitizer diagnostics on the latest quiet run.
  - `ctest --test-dir build --output-on-failure` reports that the repository currently has no registered tests.

## Runtime fixes since first successful link

- Fixed the DDE startup crash caused by static initialization order in `SDL3_COMPAT/wrappers/ddeml_compat.cpp`.
- Fixed Linux startup handoff so the real `WinMain(...)` path runs instead of printing `Run C&C.COM.` from the old stub.
- Fixed `GetModuleFileName()` so the legacy startup code can locate the executable correctly on Linux.
- Fixed leaked struct packing from the sound headers so unrelated runtime/data structures no longer inherit 1-byte packing.
- Fixed `DSBUFFERDESC` layout and primary-buffer handling in the DirectSound compatibility layer.
- Replaced `_dos_getdiskfree()` with a `std::filesystem::space(".")` implementation suitable for Linux.
- Fixed `strtrim()` overlap handling in `CODE/READLINE.CPP` by switching the leading-trim copy to `memmove()` and tightening whitespace handling.
- Fixed CRC accumulation to use explicit `uint32_t` wraparound semantics instead of relying on signed overflow.
- Fixed typed list-node deletion so derived nodes are deleted through the correct static type.
- Restored `GameInFocus` to `BOOL` so Win32-era code stops reading past a 1-byte `bool`.
- Fixed remaining sound-runtime alignment issues by restoring native alignment around `SampleTrackerType` / `LockedDataType` and explicitly aligning `CRITICAL_SECTION` to pointer alignment in the Win32 wrapper.
- Fixed multiple startup and loader loops that depended on enum post-increment reaching the `COUNT` sentinel instead of wrapping back to `FIRST`.
- Replaced unsafe `std::filesystem`-based DOS and Win32 directory enumeration wrappers with POSIX-backed implementations suitable for the Linux `_WIN32` hybrid build.
- Fixed several LP64/alignment/runtime faults uncovered by ASan/UBSan in active startup paths, including:
  - unaligned font and shape header reads;
  - overlapping INI string copies;
  - LP64 IFF/CPS header layout and parsing;
  - unaligned keyframe header access;
  - signed left-shift UB in palette and software draw paths;
  - delete mismatches in the active buffer and heap implementations.
- Fixed the SDL/Wayland bootstrap focus race by latching the first valid activation edge during startup.
- Fixed the DirectDraw compatibility presentation path so updates to the primary surface actually reach the SDL window after drawing and palette changes.
- Fixed the shutdown-time `FixedHeapClass::Clear()` deallocation mismatch exposed by ASan after clean SDL window close.

## Build layout in use

### Main executable

- `redalert`
- Sources:
  - `CODE/*.CPP`
  - `CODE/DIBCOMPAT.CPP`
  - `VQ/VQA32/AUDIOCOMPAT.CPP`
  - `VQ/VQA32/AUDUNZAPCOMPAT.CPP`
  - `VQ/VQA32/CAPTION.CPP`
  - `VQ/VQA32/CONFIG.CPP`
  - `VQ/VQA32/DRAWER.CPP`
  - `VQ/VQA32/LOADER.CPP`
  - `VQ/VQA32/TASK.CPP`
  - `VQ/VQA32/UNVQCOMPAT.CPP`
  - `VQ/VQA32/VQCOMPAT.CPP`
- Includes:
  - `SDL3_COMPAT/wrappers`
  - `CODE`
  - `WIN32LIB/INCLUDE`
  - `VQ/INCLUDE`
  - `WINVQ/INCLUDE`
- Compile defines:
  - `ENGLISH=1`
  - `FIXIT_MULTI_SAVE`
  - `FIXIT_NAME_OVERRIDE`
  - `FIXIT_SCORE_CRASH`
  - `RELEASE_VERSION=0`
  - `WIN32=1`

### Support libraries

- `win32lib` from:
  - `WIN32LIB/AUDIO`
  - `WIN32LIB/DIPTHONG`
  - `WIN32LIB/DRAWBUFF`
  - `WIN32LIB/FONT`
  - `WIN32LIB/IFF`
  - `WIN32LIB/KEYBOARD`
  - `WIN32LIB/MEM`
  - `WIN32LIB/MISC`
  - `WIN32LIB/MONO`
  - `WIN32LIB/PALETTE`
  - `WIN32LIB/PLAYCD`
  - `WIN32LIB/PROFILE`
  - `WIN32LIB/SHAPE`
  - `WIN32LIB/TILE`
  - `WIN32LIB/TIMER`
  - `WIN32LIB/WSA`
  - `WIN32LIB/WW_WIN`
  - `WIN32LIB/WINCOMM`
- `sdl3_compat` from:
  - `SDL3_COMPAT/wrappers/win32_compat.cpp`
  - `SDL3_COMPAT/wrappers/ddeml_compat.cpp`
  - `SDL3_COMPAT/wrappers/dos_compat.cpp`
  - `SDL3_COMPAT/wrappers/ddraw_compat.cpp`
  - `SDL3_COMPAT/wrappers/dsound_compat.cpp`
  - `SDL3_COMPAT/wrappers/profile_compat.cpp`

## Resolved blocker

The final link failure was narrowed to the movie layer and resolved by replacing the remaining missing pieces of the buffered VQA path:

1. Integrated the portable `VQ/VQA32` sources into the real game target.
2. Fixed header casing and stale DOS-era includes in the active movie sources.
3. Added direct C++ ports for the old assembly-only `UnVQ_4x2` and `AudioUnzap` helpers.
4. Replaced the missing HMI/SOS movie-audio dependency chain with an SDL3-backed audio stream implementation.
5. Disabled the obsolete DirectShow-based MPEG path that had no in-tree implementation and was not appropriate for the SDL3 port.

## Remaining validation work

1. Exercise front-end interaction and actual menu/input flow now that visible rendering, audio output, and clean quit are all working in the Linux SDL3 build.
2. Verify real VQA playback and in-game transitions, not just front-end/menu startup.
3. Validate the same CMake target on Windows and fix any remaining case, type-width, or wrapper issues that only surface there.
4. Continue tightening legacy Win32/DirectDraw/DirectSound assumptions only where runtime behavior or sanitizers still prove they are wrong on 64-bit/SDL3 builds.
5. Keep the porting docs aligned with each verified runtime or cross-platform milestone.
