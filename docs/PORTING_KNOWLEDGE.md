# Porting Knowledge

_Last updated: 2026-03-29_

## Repo facts

- The repository ships a bundled SDL3 source tree in `extern/SDL3`.
- The main game logic lives in `CODE/`.
- Shared platform/rendering/input/audio support code lives in `WIN32LIB/`.
- The active compatibility include order puts `SDL3_COMPAT/wrappers/` ahead of `CODE/` and `WIN32LIB/INCLUDE`.

## Movie system observations

- The active Red Alert in-game movie path is buffered VQA playback (`VQACFGF_BUFFER`) into system-memory pages. The current SDL3 port does not need the old direct MCGA/XMode/VESA output paths for normal gameplay movie playback.
- The correct movie codebase for the active build is `VQ/VQA32`, not `WINVQ/VQA32`. `WINVQ` is tied to older Win32/DirectSound assumptions that are not needed for the current buffered path.
- The old DirectShow/DirectDraw MPEG path behind `MPEGMOVIE` is obsolete for the SDL3 port and now stays disabled in `CODE/DEFINES.H`.

## Ported movie helpers

- `VQ/VQA32/UNVQCOMPAT.CPP` is the C++ replacement for the old `UnVQ_4x2` decoder.
  - Pointer bytes are split into low-byte and high-byte streams.
  - A high byte of `0x0F` means a solid-color 4x2 block filled with the low byte.
  - Otherwise `((high << 8) | low)` selects an 8-byte codebook entry.
- `VQ/VQA32/AUDUNZAPCOMPAT.CPP` is the C++ replacement for the old `AUDUNZAP.ASM` helper used by compressed movie audio.
  - 2-bit and 4-bit delta modes use saturated sample deltas.
  - Raw 5-bit delta mode preserves the original wrapping byte arithmetic.
  - The function returns the number of compressed bytes consumed, matching the old assembly contract.

## SDL3 movie-audio integration

- `VQ/VQA32/AUDIOCOMPAT.CPP` replaces the missing SOS/HMI movie-audio linkage with SDL3 audio streams.
- The SDL3-backed implementation keeps the old VQA loader/buffer structure intact:
  - audio blocks are still copied into the existing ring buffer;
  - queued-byte accounting is used for `VQA_GetTime()` when audio timing is active;
  - `VQA_PauseAudio()` / `VQA_ResumeAudio()` pause and resume the SDL playback device instead of touching HMI sample handles.
- Because the build globally defines `WIN32=1` for legacy code, SDL headers must be included with `WIN32` / `_WIN32` temporarily undefined in files that include SDL directly. Otherwise SDL takes Windows-only include paths and collides with the old SOS typedef layer.

## Linkage and header pitfalls

- The VQM32 headers rely on legacy memory-model and calling-convention keywords such as `far`, `near`, `huge`, `interrupt`, and `cdecl`. The safest fix is to define those macros locally in the VQM32 headers that need them.
- Do **not** pull the full `win32_compat.h` typedef layer into the VQM32/HMI headers just to get `cdecl` or `far`; that causes type collisions with old SOS definitions like `WORD`, `DWORD`, `HANDLE`, `LPSTR`, and `VOID`.
- `VQ/INCLUDE/VQM32/PROFILE.H` declares `GetINIInt()` and `GetINIString()` with normal C++ linkage, so the compatibility implementations in `VQ/VQA32/VQCOMPAT.CPP` must also use normal C++ linkage.
- `VQ/INCLUDE/VQM32/VIDEO.H` declares `TestVBIBit()` and `GetVBIBit()` with normal C++ linkage; only `WaitVB()` / `WaitNoVB()` use C linkage there.
- `VQ/INCLUDE/VQM32/FONT.H` needs `Char_Pixel_Width()` declared with normal C++ linkage so it matches the existing `win32lib` font implementation.
- `VQ/INCLUDE/VQM32/COMPRESS.H` also needs the legacy `cdecl` macro definitions if it is included outside the usual VQA private headers.

## Broader wrapper surface still in play

- The codebase still leans on compatibility wrappers for:
  - file I/O (`CreateFile`, `ReadFile`, `WriteFile`, `CloseHandle`);
  - threading and timing (`Sleep`, multimedia timers, thread priority helpers);
  - registry access (`RegOpenKeyEx`, `RegQueryValueEx`, `RegCloseKey`);
  - directory enumeration (`FindFirstFile`, `FindNextFile`, `FindClose`);
  - DirectDraw and DirectSound compatibility types used by legacy rendering/audio code.

## Startup/runtime porting notes

- The Linux build still compiles large parts of the game with `WIN32=1` / `_WIN32=1`, so runtime compatibility has to preserve Win32-era expectations even on SDL/Linux.
- Because `_WIN32` remains visible in this Linux build, `std::filesystem` can take Windows-oriented code paths that are unsafe in compat wrappers. For file enumeration and similar wrappers, prefer direct POSIX `opendir` / `readdir` / `stat` / `statvfs` logic on non-Windows hosts.
- Linux must enter the real `WinMain(...)` startup path. If the Unix stub `main()` is left in place, the program only prints the old placeholder message (`Run C&C.COM.`) instead of running the game.
- `GetModuleFileName()` must return a real executable path on Linux because `CODE/STARTUP.CPP` uses it during startup.
- `CODE/STARTUP.CPP` changes the current working directory to the executable directory. For smoke tests that need the real assets, copy the built executable into `GameData/` and launch it from there.
- The DDE compatibility layer cannot keep startup-critical mutable state in ordinary file-scope globals. Game-side global constructors may call into DDE before those globals are safely initialized.
- SDL/Wayland startup can deliver focus gained and focus lost in the same early pump cycle. The bootstrap path needs a sticky "focus seen once" latch instead of waiting only on the live `GameInFocus` bit.

## 64-bit ABI pitfalls found during runtime debugging

- `GameInFocus` must stay a Win32-style `BOOL`, not `bool`. Several legacy translation units still declare it as `extern BOOL GameInFocus`, and shrinking it to 1 byte causes out-of-bounds reads on 64-bit builds.
- `strtrim()` in `CODE/READLINE.CPP` must use `memmove()` for the left-trim step because the source and destination ranges overlap.
- CRC accumulation code that historically relied on 32-bit signed wraparound should use explicit `uint32_t` intermediates instead. This preserves the original modulo-2^32 behavior without triggering UB on modern compilers/sanitizers.
- Typed list nodes must be deleted through their concrete type. Deleting derived list nodes through `GenericNode*` triggers `new-delete-type-mismatch` under ASan.
- Enum post-increment helpers used by ordinary `for (x = FIRST; x < COUNT; x++)` loops must advance to the `COUNT` sentinel, not wrap back to `FIRST`, or startup/UI one-time initialization can spin forever.

## Packing/alignment notes

- Old `#pragma pack` state leaks easily through the audio headers; keep those headers balanced with `push` / `pop`.
- The DirectSound compatibility wrapper must match the original `DSBUFFERDESC` layout closely enough for the legacy code to populate it safely, including `dwReserved`.
- Some translation units include `windows.h` under `#pragma pack(4)`. The compatibility `CRITICAL_SECTION` therefore needs explicit pointer alignment (`alignas(void*)`) so its internal mutex pointer is not placed at 4-byte-aligned addresses on 64-bit Linux.
- The runtime sound bookkeeping structs (`SampleTrackerType`, `LockedDataType`) need native alignment for pointer-bearing members and `CRITICAL_SECTION`. Keeping them under forced 4-byte packing produces sanitizer-reported misaligned accesses during `Audio_Init()`.
- Asset/file formats that were historically read through raw pointer casts now need fixed-width structs or `memcpy`-based header loads in active Linux/ASan paths. Unaligned font, shape, IFF/CPS, and keyframe reads were all real runtime issues in this port.

## DirectDraw/SDL presentation notes

- The software renderer can draw correctly and still show an all-black SDL window if the DirectDraw compatibility layer never presents the updated primary surface.
- In the current compat layer, primary-surface presentation must happen after:
  - `IDirectDrawSurface::Unlock()` on the primary surface;
  - `IDirectDrawSurface::Blt()` when the destination is the primary surface;
  - `IDirectDrawSurface::SetPalette()` when a primary surface palette is attached or changed.
- The DirectDraw compat surface needs the owning SDL window/`HWND` stored with it so presentation can target the correct window during those updates.

## Runtime tracing notes

- Startup tracing is useful, but per-frame callback tracing is too noisy in the menu loop and can perturb runtime by blocking on `stderr` writes.
- Keep startup tracing under `RA_TRACE_STARTUP`, and only enable callback spam behind a separate `RA_TRACE_CALLBACK` gate when specifically needed.

## Current working baseline

- A full Linux build now links successfully with the SDL3/CMake path.
- Running the copied executable from `GameData/` now reaches the real front-end path with a live `640x480` SDL/Hyprland window titled `Red Alert`.
- The latest validated normal run is no longer visually black; a captured window image has substantial non-black and bright-pixel coverage after the DirectDraw present-path fix.
- The latest validated normal run opens a PipeWire sink input and emits non-silent monitor audio during the front-end/menu baseline.
- The latest validated normal and ASan runs both handle compositor-driven window close cleanly through `SDL_EVENT_WINDOW_CLOSE_REQUESTED -> WM_CLOSE -> WM_DESTROY`.
- The latest quiet ASan/UBSan validation reaches that same front-end baseline and shuts down without sanitizer diagnostics.
- The next likely issues are now deeper validation items such as front-end interaction, VQA playback, gameplay transitions, and Windows-specific runtime behavior.
