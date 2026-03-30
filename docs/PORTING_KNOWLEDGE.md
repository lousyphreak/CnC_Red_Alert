# Porting Knowledge

_Last updated: 2026-03-30_

## Repo facts

- The repository ships a bundled SDL3 source tree in `extern/SDL3`.
- The main game logic lives in `CODE/`.
- Shared platform/rendering/input/audio support code lives in `WIN32LIB/`.
- The active compatibility include order puts `SDL3_COMPAT/wrappers/` ahead of `CODE/` and `WIN32LIB/INCLUDE`.

## Gameplay audio system observations

- The active gameplay audio path is `CODE/AUDIO.CPP` -> `WIN32LIB/AUDIO/SOUNDIO.CPP` -> `WIN32LIB/AUDIO/SOUNDINT.CPP`.
- The safest porting seam is below the legacy `Audio_*`, `Play_Sample`, and sample-tracker logic. Keep those APIs and refill rules stable; replace only the device/buffer backend under them.
- The active gameplay backend is now `WIN32LIB/INCLUDE/SDLAUDIOBACKEND.H` plus `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP`.
  - `AudioBackendDevice`, `AudioBackendBuffer`, and `AudioBufferFormat` replace the active DirectSound types.
  - The backend uses SDL3 audio streams/device output internally while preserving the old primary/secondary buffer contract expected by `SOUNDIO.CPP` and `SOUNDINT.CPP`.
- `WIN32LIB/AUDIO/SOUNDIO.CPP` no longer uses `DirectSoundCreate()` or WinMM timer callbacks on the active path.
  - maintenance now runs through `Pump_Sound_Service()` plus `Start_Sound_Thread()` / `Stop_Sound_Thread()`;
  - the service thread still uses the generic Win32-compat thread helpers, but audio transport/output is SDL-backed.

## Movie system observations

- The active Red Alert in-game movie path is buffered VQA playback (`VQACFGF_BUFFER`) into system-memory pages. The current SDL3 port does not need the old direct MCGA/XMode/VESA output paths for normal gameplay movie playback.
- The correct movie codebase for the active build is `VQ/VQA32`, not `WINVQ/VQA32`. `WINVQ` is tied to older Win32/DirectSound assumptions that are not needed for the current buffered path.
- The old DirectShow/DirectDraw MPEG path behind `MPEGMOVIE` is obsolete for the SDL3 port and now stays disabled in `CODE/DEFINES.H`.

## Ported movie helpers

- `VQ/VQA32/UNVQCOMPAT.CPP` is the C++ replacement for the old `UnVQ_4x2` decoder.
  - Pointer bytes are split into low-byte and high-byte streams.
  - A high byte of `0x0F` means a solid-color 4x2 block filled with the low byte.
  - Otherwise `((high << 8) | low)` selects an 8-byte codebook entry.
- `VQ/VQA32/UNVQCOMPAT.CPP` also now carries the C++ replacement for the old `UnVQ_4x4` decoder used by startup movies.
  - **Important:** `4x4` decoding is version-dependent.
  - VQA header versions `1`/`2` use the older split low-byte/high-byte pointer layout, with the low-byte plane first and the high-byte plane second.
  - For the active startup intro asset `ENGLISH.VQA` (version `2`, palettized), the correct buffered decode is `vector_index = (high << 8) | low`, and `high == 0xFF` is a solid-color `4x4` fill using `low` as the palette index.
  - The direct 16-bit offset / high-bit special-case path belongs to newer `4x4` streams and must not be used for version-2 palettized intro movies.
- `VQ/VQA32/AUDUNZAPCOMPAT.CPP` is the C++ replacement for the old `AUDUNZAP.ASM` helper used by compressed movie audio.
  - 2-bit and 4-bit delta modes use saturated sample deltas.
  - Raw 5-bit delta mode preserves the original wrapping byte arithmetic.
  - The function returns the number of compressed bytes consumed, matching the old assembly contract.

## Movie runtime pitfalls

- Active VQA/IFF on-disk header structs must use fixed-width integer types. Leaving legacy `unsigned long` fields in the active movie headers breaks parsing on LP64 hosts and causes `VQAERR_NOTVQA`/bad chunk handling on Linux.
- The active game-facing and VQA-private `VQAConfig` definitions must stay layout-compatible even though the SDL3 build does not use DirectSound objects directly.
  - On the active game include path (`VQ/INCLUDE/VQA32/VQAPLAY.H`), keep `VQADIRECT_SOUND` disabled and expose `SoundObject` / `PrimaryBufferPtr` as generic pointer placeholders.
  - Keep the matching initializer slots in `VQ/VQA32/CONFIG.CPP`.
- The VQ-side `_SOS_COMPRESS_INFO` layout must match the active game-side ADPCM decoder, not just the original VQ headers.
- The startup intro movies loaded from the MIX archives use `4x4` VQ blocks. If only `4x2` decode is enabled, the intro can open and play audio while the video surface stays black.
- Buffered `4x4` VQ playback does not size codebooks the same way as buffered `4x2`.
  - `UnVQ_4x4` consumes direct positive 15-bit offsets from the pointer stream and then reads a full 16-byte codeword at `codebook + pointer - 4`.
  - For `ENGLISH.VQA` (`block=4x4`, `group=8`, `cb=2000`, `one=256`), the legacy `CBentries * 16 + 250` estimate in `LOADER.CPP` was too small for that address space.
  - `LOADER.CPP` therefore has to clamp `Max_CB_Size` to the full buffered `4x4` pointer range (~32 KiB) before LCW decompression. Otherwise the codebook is silently truncated, the intro can go audio-only/black, and ASan trips in `UnVQ_4x4`.
- The buffered VQA callback path does not get palette handling "for free" from the page-flip routines.
  - `PageFlip_MCGABuf()` and related direct-display paths already call `SetPalette(...)` / handle `VQADRWF_SETPAL`.
  - `DrawFrame_Buffer()` originally only decoded into `ImageBuf` and invoked the callback, so SDL/Linux startup movies could play audio and decode non-zero frames while the visible page stayed black.
  - The active buffered path therefore has to apply `VQAFRMF_PALETTE` and deferred `VQADRWF_SETPAL` updates itself before invoking the callback.

## SDL3 movie-audio integration

- `VQ/VQA32/AUDIOCOMPAT.CPP` replaces the missing SOS/HMI movie-audio linkage with SDL3 audio streams.
- The SDL3-backed implementation keeps the old VQA loader/buffer structure intact:
  - audio blocks are still copied into the existing ring buffer;
  - queued-byte accounting is used for `VQA_GetTime()` when audio timing is active;
  - `VQA_PauseAudio()` / `VQA_ResumeAudio()` pause and resume the movie SDL stream directly instead of touching HMI sample handles.
- Traced intro→menu runs now show the front-end theme successfully reaching `File_Stream_Sample_Vol("INTRO.AUD", ...)` and `Stream_Sample_Vol(...)` after startup movie playback completes.
- `WINSTUB.CPP::Focus_Loss()` calls `Theme.Suspend()`, so a brief `WM_ACTIVATEAPP` focus flap around the intro→menu handoff can legitimately reset `Current=-1`, move `Pending=Score`, and cause one immediate theme restart on the next `Theme.AI()` pass.
- In the active buffered movie path, `config->DrawerCallback(...)` returning non-zero makes `DrawFrame_Buffer()` return `VQAERR_EOF`.
  - `VQ/VQA32/TASK.CPP` must treat that as a real stop condition.
  - If the draw loop ignores it, the player keeps spinning with `VQADATF_UPDATE` / `VQADATF_DSLEEP` state still set and the process appears hard-locked when the intro is aborted with `ESC`.
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
  - threading and timing (`Sleep`, generic thread helpers, thread priority helpers);
  - registry access (`RegOpenKeyEx`, `RegQueryValueEx`, `RegCloseKey`);
  - directory enumeration (`FindFirstFile`, `FindNextFile`, `FindClose`);
  - DirectDraw compatibility types used by legacy rendering code.
- The active gameplay audio path no longer depends on the DirectSound compatibility wrapper; any remaining DirectSound-shaped code is archival or inactive.

## Startup/runtime porting notes

- The Linux build still compiles large parts of the game with `WIN32=1` / `_WIN32=1`, so runtime compatibility has to preserve Win32-era expectations even on SDL/Linux.
- Because `_WIN32` remains visible in this Linux build, `std::filesystem` can take Windows-oriented code paths that are unsafe in compat wrappers. For file enumeration and similar wrappers, prefer direct POSIX `opendir` / `readdir` / `stat` / `statvfs` logic on non-Windows hosts.
- `SDL3_COMPAT/wrappers/win32_compat.cpp` virtual-CD probing (`virtual_cd_index_for_drive_letter()` / `GetDriveType()`) is part of static startup through `GetCDClass`. On the Linux `_WIN32` hybrid build, avoid `std::filesystem::path` composition there; it can trip ASan/libstdc++ `new-delete-type-mismatch` failures before the game reaches normal startup.
- Linux must enter the real `WinMain(...)` startup path. If the Unix stub `main()` is left in place, the program only prints the old placeholder message (`Run C&C.COM.`) instead of running the game.
- `GetModuleFileName()` must return a real executable path on Linux because `CODE/STARTUP.CPP` uses it during startup.
- `CODE/STARTUP.CPP` changes the current working directory to the executable directory. The build now copies the runnable binaries into `GameData/` automatically:
  - normal builds refresh `GameData/redalert`;
  - ASan builds refresh `GameData/redalert-asan`.
  Smoke tests should launch those `GameData/` copies so the executable directory still contains the real assets.
- The DDE compatibility layer cannot keep startup-critical mutable state in ordinary file-scope globals. Game-side global constructors may call into DDE before those globals are safely initialized.
- SDL/Wayland startup can deliver focus gained and focus lost in the same early pump cycle. The bootstrap path needs a sticky "focus seen once" latch instead of waiting only on the live `GameInFocus` bit.

## Mouse/input notes

- The active keyboard queue path in this build is `CODE/KEY.CPP`, not `CODE/KEYBOARD.CPP`.
- Main-menu hover logic is split from click logic:
  - hover/selection tracking uses `Get_Mouse_X()` / `Get_Mouse_Y()` from `WIN32LIB/KEYBOARD/MOUSE.CPP`;
  - click selection uses `Keyboard->MouseQX` / `Keyboard->MouseQY` from `CODE/KEY.CPP`.
- `CODE/KEY.CPP` therefore has to update `MouseQX` / `MouseQY` both on `WM_MOUSEMOVE` and when queueing mouse-button events. Otherwise button clicks can be delivered with stale coordinates even if the low-level SDL/Win32 button message itself arrived.
- The software cursor in `WIN32LIB/KEYBOARD/MOUSE.CPP` is timer-driven and redraws from `GetCursorPos()`, so queue-side mouse-coordinate fixes and visible cursor fixes are related but not the same subsystem.
- `SDL3_COMPAT/wrappers/win32_compat.cpp` must translate SDL keyboard events to Win32 virtual-key values before they enter the legacy queue.
  - Passing raw SDL keycodes through as `wParam` only works for a few accidental overlaps such as `ESC`.
  - Letters need uppercase `VK_*` values, arrows/function keys need their Win32 virtual keys, and keypad/punctuation keys need explicit mapping.
- `GetKeyState()` / `GetAsyncKeyState()` in the SDL compat layer must report ordinary keys and mouse buttons, not just modifiers.
  - `CODE/KEY.CPP::Down()` is just `GetAsyncKeyState(key & 0xFF) != 0`, so returning `0` for non-modifier keys makes the UI look frozen even when the menu loop is still running.
  - Some legacy loops also use `GetAsyncKeyState(vk) & 1`, so the compat layer needs a one-shot low-bit latch for recent press transitions as well as current high-bit state.
- Toggle keys must also keep real Win32 low-bit semantics:
  - `CODE/KEY.CPP` still probes `GetKeyState(VK_CAPITAL/VK_NUMLOCK) & 0x0008` when building queued key codes;
  - if the compat layer incorrectly sets that bit, ordinary keys like `ESC` pick up `WWKEY_SHIFT_BIT` and exact comparisons against `KN_ESC` fail in the intro callback.
- The intro breakout path on this build is now two-stage:
  - `CODE/CONQUER.CPP::VQ_Call_Back()` sets `Brokeout = true` and returns non-zero when it sees exact `KN_ESC`;
  - `VQ/VQA32/TASK.CPP` then has to convert the resulting `VQAERR_EOF` into movie shutdown so `VQA_StopAudio()`, `VQA_Play()` return, and startup continues to the title/menu path.
- Mouse messages from the SDL compat layer should carry current `MK_*` flags and correct button identities.
  - `SDL_EVENT_MOUSE_MOTION` should preserve held-button state in `wParam`.
  - `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` must distinguish left, middle, and right buttons; mapping middle mouse to right mouse breaks the old Win32 message contract.
- `Init_Mouse()` leaves the software cursor hidden again before front-end flow continues, so menu entry points that need pointer interaction must drive the Westwood cursor hide count back to visible state explicitly and restore the prior hide state when they exit.
- The menu cursor regression on LP64 was not just a hide-count problem:
  - the active `Shape_Type` file header must stay packed to the original 26-byte on-disk layout in `WIN32LIB/SHAPE/SHAPE.H` / `WIN32LIB/INCLUDE/SHAPE.H`;
  - if that header grows to 28 bytes, `Get_Shape_Width()` / `Get_Shape_Original_Height()` read the wrong offsets from raw `.SHP` data, so a real `30x24` mouse frame is misread as `6144x109`, `ASM_Set_Mouse_Cursor()` rejects it against the `48x48` cursor buffer, and the software cursor becomes invisible even though menu code has already unhidden it;
  - once the shape header is packed again, `CODE/MENUS.CPP` can reliably restore `MOUSE_NORMAL` and composite `WWMouse->Draw_Mouse(&HidPage)` into the title/menu blit to keep the front-end cursor visible on the SDL/Win32 path.

## 64-bit ABI pitfalls found during runtime debugging

- `GameInFocus` must stay a Win32-style `BOOL`, not `bool`. Several legacy translation units still declare it as `extern BOOL GameInFocus`, and shrinking it to 1 byte causes out-of-bounds reads on 64-bit builds.
- `strtrim()` in `CODE/READLINE.CPP` must use `memmove()` for the left-trim step because the source and destination ranges overlap.
- CRC accumulation code that historically relied on 32-bit signed wraparound should use explicit `uint32_t` intermediates instead. This preserves the original modulo-2^32 behavior without triggering UB on modern compilers/sanitizers.
- Typed list nodes must be deleted through their concrete type. Deleting derived list nodes through `GenericNode*` triggers `new-delete-type-mismatch` under ASan.
- Enum post-increment helpers used by ordinary `for (x = FIRST; x < COUNT; x++)` loops must advance to the `COUNT` sentinel, not wrap back to `FIRST`, or startup/UI one-time initialization can spin forever.

## Packing/alignment notes

- Old `#pragma pack` state leaks easily through the audio headers; keep those headers balanced with `push` / `pop`.
- The active SDL audio backend still has to preserve the DirectSound-era buffer contract closely enough for legacy sound code to populate formats, lock regions, and play/status flags safely.
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
- The SDL texture format must match how the compat layer packs palette-expanded pixels.
  - The current compat presenter expands indexed pixels to `0xAARRGGBB`.
  - Feeding those values to an `SDL_PIXELFORMAT_RGBA8888` texture makes movies appear as red-tinted shades because SDL interprets the bytes as `R,G,B,A`.
  - Use `SDL_PIXELFORMAT_ARGB8888` and `SDL_BLENDMODE_NONE` for the compat movie presenter.
- Startup trace dumps of `ENGLISH.VQA` show that the remaining "quadrant" complaint is not a literal four-way mirror in the final SDL window.
  - The old quadrant/garbled look was in the decoded `640x400` movie buffer, not in the final SDL present path.
  - Comparing extracted `ENGLISH.VQA` frames against `ffmpeg` was enough to confirm the root cause: the SDL port was feeding the version-2 intro through the wrong `4x4` pointer decoder.

## Runtime tracing notes

- Startup tracing is useful, but per-frame callback tracing is too noisy in the menu loop and can perturb runtime by blocking on `stderr` writes.
- Keep startup tracing under `RA_TRACE_STARTUP`, and only enable callback spam behind a separate `RA_TRACE_CALLBACK` gate when specifically needed.

## Current working baseline

- A full Linux build now links successfully with the SDL3/CMake path.
- Running the copied executable from `GameData/` now reaches the real front-end path with a live `640x480` SDL/Hyprland window titled `Red Alert`.
- The latest validated normal run is no longer visually black; a captured window image has substantial non-black and bright-pixel coverage after the DirectDraw present-path fix.
- The latest validated normal run opens a PipeWire sink input and emits non-silent monitor audio during the front-end/menu baseline.
- The latest validated normal and ASan builds both compile/link the SDL-backed gameplay audio path, and the active `sdl3_compat` library no longer includes `dsound_compat.cpp`.
- The latest validated normal and ASan builds also refresh `GameData/redalert` and `GameData/redalert-asan` automatically, so the runnable launchers match the current build outputs byte-for-byte.
- The latest validated startup run now opens and visibly animates `ENGLISH.VQA` instead of skipping directly to the menu on a black movie surface.
- The latest validated normal and ASan runs both handle compositor-driven window close cleanly through `SDL_EVENT_WINDOW_CLOSE_REQUESTED -> WM_CLOSE -> WM_DESTROY`.
- The latest `RA_TRACE_STARTUP=1 gdb` checks on both `GameData/redalert` and `GameData/redalert-asan` get through window/audio/media startup instead of aborting immediately in compat `GetDriveType()` / virtual-CD probing.
- The latest quiet ASan/UBSan validation reaches that same front-end baseline and shuts down without sanitizer diagnostics.
- The latest traced ASan intro run also survives the old `UnVQ_4x4` crash path after the `4x4` codebook sizing fix and continues streaming `ENGLISH.VQA` without sanitizer output.
- The next likely issues are now deeper validation items such as full real-pointer front-end interaction, gameplay transitions, and Windows-specific runtime behavior.
