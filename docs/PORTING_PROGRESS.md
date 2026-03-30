# Porting Progress

_Last updated: 2026-03-30_

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
  - the buffered `UnVQ_4x4` decoder replacement in `VQ/VQA32/UNVQCOMPAT.CPP`;
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
- The active gameplay sound path is now SDL-backed instead of DirectSound-backed:
  - `CODE/AUDIO.CPP` still drives the legacy `Audio_*` / `Play_Sample` API surface used by the game;
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` / `WIN32LIB/AUDIO/SOUNDINT.CPP` now sit on top of `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP`;
  - the active build no longer compiles `SDL3_COMPAT/wrappers/dsound_compat.cpp`.
- Active sound servicing no longer depends on WinMM multimedia timers:
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` now uses `Pump_Sound_Service()` and a dedicated sound thread around the existing maintenance callback logic.
- The runnable `GameData` executables are now refreshed automatically from the active build trees:
  - normal builds copy `build/redalert` to `GameData/redalert`;
  - ASan builds copy `build-asan/redalert` to `GameData/redalert-asan`.
- Startup movie playback is working again in the Linux SDL3 build:
  - `ENGLISH.VQA` now opens through the buffered VQA path instead of failing with `VQAERR_NOTVQA`;
  - the intro no longer jumps straight to the main menu;
  - focused captures of the intro window now show changing non-black frames instead of a static black surface.
- The startup movie audio path is working again:
  - the intro plays through the same restored VQA audio decode/output path validated under ASan;
  - normal focused runs continue into the front end with active audio output.
- Menu click hit-testing is no longer using stale mouse coordinates:
  - `CODE/KEY.CPP` now updates `MouseQX` / `MouseQY` on `WM_MOUSEMOVE`;
  - queued mouse-button messages now capture the current coordinates immediately when the event is enqueued.
- A compositor close request now shuts the game down cleanly through the SDL/Win32 message bridge instead of leaving the process hung until `SIGKILL`.
- Validation status:
  - `cmake --build build --target redalert -j4` passes.
  - `cmake --build build -j4` passes.
  - `cmake --build build-asan --target redalert -j4` passes.
  - Running `GameData/redalert` reaches a live `640x480` `Red Alert` window with visible framebuffer updates.
  - `RA_TRACE_STARTUP=1 gdb ./redalert` from `GameData/` now gets through window/audio/media startup instead of aborting immediately in `GetDriveType()`.
  - A captured window image from the rebuilt normal run reports `mean_rgb=22,6,6`, `nonblack_ratio=0.444277`, `bright_ratio=0.365957`.
  - A short monitor capture from the rebuilt normal run reports `abs_mean=1224.67`, `rms=2406.59`, `peak=21253`, `nonzero_ratio=0.808433`.
  - A traced startup run now logs `ENGLISH.VQA` with `block=4x4`, and the first callback samples are non-zero from frame `0` onward instead of staying all black.
  - Two focused intro captures taken a few seconds apart now differ across tens of thousands of pixels, confirming that the intro is animating on-screen instead of presenting a static black frame.
  - Running `GameData/redalert-asan` with `ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:new_delete_type_mismatch=0` reaches the same front-end baseline, handles window close, and emits no sanitizer diagnostics on the latest quiet run.
  - `RA_TRACE_STARTUP=1 gdb ./redalert-asan` from `GameData/` now gets through the same startup path without the earlier immediate ASan abort.
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
- Fixed Linux compat virtual-CD probing in `SDL3_COMPAT/wrappers/win32_compat.cpp` by replacing `std::filesystem` path composition in `virtual_cd_index_for_drive_letter()` / `GetDriveType()` with string-plus-`stat()` checks on non-Windows hosts; this removes the immediate ASan `new-delete-type-mismatch` abort seen under `gdb` during `GetCDClass` startup.
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
- Fixed LP64-breaking movie file parsing by replacing active on-disk IFF/VQA header fields that still used `unsigned long` with fixed-width integer types.
- Replaced the active gameplay DirectSound backend with `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP`, keeping the legacy `Audio_*` / `Play_Sample` behavior and refill semantics intact while moving transport/mixing to SDL3.
- Replaced active WinMM sound-maintenance timer usage in `WIN32LIB/AUDIO/SOUNDIO.CPP` with `Pump_Sound_Service()` plus a sound worker thread.
- Fixed the active public/private `VQAConfig` layout mismatch by keeping `SoundObject` / `PrimaryBufferPtr` as backend-agnostic placeholder pointers on the game include path and in `CONFIG.CPP`.
- Fixed the active movie ADPCM ABI mismatch by restoring the `_SOS_COMPRESS_INFO` field order expected by the game-side decoder and routing movie decode through `General_sosCODECDecompressData()`.
- Restored buffered `4x4` VQA decode support by porting the old `UnVQ_4x4` helper into `VQ/VQA32/UNVQCOMPAT.CPP` and enabling the `VQABLOCK_4X4` path used by the startup intro movies.
- Fixed SDL/Win32 menu click handling by updating `CODE/KEY.CPP` so `WM_MOUSEMOVE` refreshes `MouseQX` / `MouseQY` and queued mouse-button events latch the current coordinates immediately.
- Added a `redalert` post-build deployment step in `CMakeLists.txt` so `GameData/redalert` and `GameData/redalert-asan` stay synchronized with the latest normal and ASan builds.

## Build layout in use

### Main executable

- `redalert`
- Post-build deployment:
  - normal builds refresh `GameData/redalert`;
  - ASan builds refresh `GameData/redalert-asan`.
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
  - `SDL3_COMPAT/wrappers/profile_compat.cpp`

## Resolved blocker

The final link failure was narrowed to the movie layer and resolved by replacing the remaining missing pieces of the buffered VQA path:

1. Integrated the portable `VQ/VQA32` sources into the real game target.
2. Fixed header casing and stale DOS-era includes in the active movie sources.
3. Added direct C++ ports for the old assembly-only `UnVQ_4x2` and `AudioUnzap` helpers.
4. Replaced the missing HMI/SOS movie-audio dependency chain with an SDL3-backed audio stream implementation.
5. Disabled the obsolete DirectShow-based MPEG path that had no in-tree implementation and was not appropriate for the SDL3 port.

## Remaining validation work

1. Re-test front-end interaction with real user-driven pointer input on SDL/Wayland to confirm that visible cursor motion and click behavior now match the restored queue-side mouse coordinates.
2. Exercise additional VQA playback and in-game transitions beyond the startup intro and front-end.
3. Validate the same CMake target on Windows and fix any remaining case, type-width, or wrapper issues that only surface there.
4. Continue tightening remaining legacy Win32/DirectDraw/audio assumptions only where runtime behavior or sanitizers still prove they are wrong on 64-bit/SDL3 builds.
5. Keep the porting docs aligned with each verified runtime or cross-platform milestone.
