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
- Gameplay `.AUD` parsing is now LP64-safe again:
  - the active `AUDHeaderType` definitions in `WIN32LIB/AUDIO/AUDIO.H`, `WIN32LIB/INCLUDE/AUDIO.H`, and `VQ/INCLUDE/WWLIB32/AUDIO.H` now use fixed-width 32-bit size fields and are asserted to stay at the original 12-byte on-disk layout;
  - this fixes the 64-bit Linux case where legacy `long` fields had grown the header to 20 bytes, corrupting streamed theme/music parsing and potentially affecting `.AUD`-backed sound effects too.
- The gameplay mixer now preserves correct unsigned-8-bit silence semantics:
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` and `WIN32LIB/AUDIO/SOUNDINT.CPP` now fill unused 8-bit PCM regions with `0x80` instead of `0x00`;
  - this matches the active SDL backend's unsigned 8-bit interpretation and avoids garbled tails/gaps when streamed or partially filled sound buffers run past valid sample data.
- The gameplay sound refill path is now LP64-safe:
  - `SampleTrackerType::DestPtr` in `WIN32LIB/AUDIO/SOUNDINT.H` / `WIN32LIB/INCLUDE/SOUNDINT.H` is now an integer byte offset instead of a fake pointer;
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` / `WIN32LIB/AUDIO/SOUNDINT.CPP` no longer truncate locked-buffer addresses through `(unsigned)` casts when comparing, updating, or using that offset;
  - this fixes the ASan crash at `Play_Sample_Handle()` during menu/theme startup on 64-bit Linux after streamed music reaches `Stream_Sample_Vol(...)`.
- Active sound servicing no longer depends on WinMM multimedia timers:
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` now uses `Pump_Sound_Service()` and a dedicated sound thread around the existing maintenance callback logic.
- The runnable `GameData` executables are now refreshed automatically from the active build trees:
  - normal builds copy `build/redalert` to `GameData/redalert`;
  - ASan builds copy `build-asan/redalert` to `GameData/redalert-asan`.
- Startup movie playback is working again in the Linux SDL3 build:
  - `ENGLISH.VQA` now opens through the buffered VQA path instead of failing with `VQAERR_NOTVQA`;
  - the intro no longer jumps straight to the main menu;
  - focused captures of the intro window now show changing non-black frames instead of a static black surface.
- The ASan intro playback no longer overruns the buffered `4x4` VQA codebook:
  - `ENGLISH.VQA` traces as `block=4x4`, `group=8`, `cb=2000`, `one=256`;
  - `VQ/VQA32/LOADER.CPP` now grows `Max_CB_Size` for buffered `4x4` playback to cover the full positive 15-bit pointer range consumed by `UnVQ_4x4`, instead of only `CBentries * 16 + 250`.
- The buffered intro movie path now applies movie palettes again:
  - `VQ/VQA32/DRAWER.CPP` now handles `VQAFRMF_PALETTE` and deferred `VQADRWF_SETPAL` updates in `DrawFrame_Buffer()` before invoking the game callback;
  - this fixes the SDL/Linux startup case where `ENGLISH.VQA` decoded non-zero frames and played audio, but stayed visually black because the buffered callback path never uploaded the VQA palette.
- The SDL primary-surface presentation path no longer forces indexed movie colors into red-tinted output:
  - `SDL3_COMPAT/wrappers/ddraw_compat.cpp` now presents through an `SDL_PIXELFORMAT_ARGB8888` texture with blending disabled, matching the `0xAARRGGBB` palette-expanded pixels generated by the DirectDraw compat layer.
- The startup movie audio path is working again:
  - the intro plays through the same restored VQA audio decode/output path validated under ASan;
  - normal focused runs continue into the front end with active audio output.
- The remaining intro-movie decode corruption is fixed:
  - `ENGLISH.VQA` is a version-2 palettized `4x4` VQA stream, so its pointer map uses the older split low-byte/high-byte layout rather than the direct 16-bit offset scheme used by newer `4x4` paths;
  - `VQ/VQA32/UNVQCOMPAT.CPP` now provides a version-aware buffered `UnVQ_4x4_V2` decoder and `VQ/VQA32/DRAWER.CPP` selects it for VQA header versions `<= 2`;
  - dumped startup frames now line up with independently decoded `ffmpeg` reference frames instead of showing the old quadrant/garbled output.
- Menu click hit-testing is no longer using stale mouse coordinates:
  - `CODE/KEY.CPP` now updates `MouseQX` / `MouseQY` on `WM_MOUSEMOVE`;
  - queued mouse-button messages now capture the current coordinates immediately when the event is enqueued.
- The SDL/Win32 input bridge now matches the legacy menu/movie expectations closely enough for the front end to keep moving past the intro:
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` now maps SDL keyboard events to Win32 virtual keys instead of forwarding raw SDL keycodes;
  - `GetKeyState()` / `GetAsyncKeyState()` now report ordinary keys and mouse buttons, and `GetAsyncKeyState()` keeps a one-shot press latch for callers that check the low bit;
  - SDL mouse motion/button events now preserve `MK_*` button flags and distinguish middle mouse from right mouse correctly.
- Manual intro breakout is restored again on the SDL/Linux path:
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` now exposes toggle-key state with the normal Win32 low-bit convention instead of the earlier compat-only `0x0008` pollution;
  - this stops `CODE/KEY.CPP` from spuriously adding `WWKEY_SHIFT_BIT` to `ESC` when NumLock/CapsLock are set, so the intro callback can match exact `KN_ESC` again.
- The main-menu software cursor is visible again:
  - `CODE/MENUS.CPP` now restores the default `MOUSE_NORMAL` shape on menu entry, forces the Westwood software cursor hide-count back to visible state, and composites the software cursor into the menu blit on the active Win32/SDL front-end path;
  - `WIN32LIB/SHAPE/SHAPE.H` and `WIN32LIB/INCLUDE/SHAPE.H` now keep the raw on-disk `Shape_Type` header packed to 26 bytes, so `Get_Shape_Width()` / `Get_Shape_Original_Height()` read real cursor dimensions on LP64 hosts instead of rejecting valid `.SHP` cursor frames with bogus sizes.
- The active front-end input path is now SDL-native instead of relying on fake Win32 keyboard/mouse messages:
  - `CODE/SDLINPUT.CPP` / `CODE/SDLINPUT.H` now own SDL event pumping, virtual-key state, async low-bit latches, toggle state, mouse position, and button state for the game;
  - `CODE/KEY.CPP` now pumps that SDL input module directly, queues key/button events through `WWKeyboardClass`, and uses the normal Win32 low toggle bit (`0x0001`) when composing queued key modifiers;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` now keeps only lifecycle/focus messages on the compat queue while forwarding SDL key/mouse events straight into the new game input module;
  - `GetKeyState()`, `GetAsyncKeyState()`, `GetCursorPos()`, and `CODE/MOUSEUTIL.CPP` now all read the same shared SDL input state.
- Post-startup focus loss no longer suspends the SDL3/Linux game:
  - `CODE/SDLINPUT.CPP` now treats focus changes as bootstrap-only after the first successful activation while still clearing held input on focus loss;
  - `CODE/WINSTUB.CPP` now ignores post-bootstrap `WM_ACTIVATEAPP` loss instead of dropping `GameInFocus`, suspending theme playback, or stopping the primary sound buffer;
  - a scripted `gdb` probe at `Main_Menu` now forces `WM_ACTIVATEAPP(active=0)` and confirms `GameInFocus` stays `1` and `Start_Primary_Sound_Buffer(FALSE)` still succeeds immediately afterward.
- Dialog background shapes render again on the active SDL/Win32 front-end path:
  - `CODE/CONQUER.CPP::CC_Draw_Shape()` now unwraps Win32 cached `Build_Frame()` results through `Get_Shape_Header_Data()` before calling `Buffer_Frame_To_Page()`;
  - a focused `gdb` probe at `Main_Menu` against `DD-BKGND.SHP` confirms `UseBigShapeBuffer=1` on the current machine and that the cached wrapper pointer differs from the raw payload pointer, which matches the missing-background symptom the user reported.
- Reworked the Westwood software cursor so it no longer redraws from its old WinMM timer callback:
  - `WIN32LIB/KEYBOARD/MOUSE.CPP` no longer starts a `timeSetEvent()` cursor thread on the active SDL path;
  - `CODE/CONQUER.CPP::Call_Back()` now runs `WWMouse->Process_Mouse()` on the same main thread that pumps SDL input;
  - this keeps SDL surface unlock/present work off background threads, which was the root cause behind the latest menu freeze / ASan crash investigation on Wayland/OpenGL.
- Focused debugger validation now shows the repaired cursor/update path end-to-end:
  - the first `WWMouseClass::Process_Mouse()` hit now comes from `CODE/CONQUER.CPP::Call_Back()` on thread 1 instead of the old timer shim;
  - a forced-intro-skip `GameData/redalert-asan` smoke run reaches `Main_Menu` and no longer reproduces the earlier `Buffer_Fill_Rect()` menu crash while sitting in the front end.
- The movie-to-menu audio handoff is now isolated to the VQA stream itself:
  - `VQ/VQA32/AUDIOCOMPAT.CPP` now pauses and resumes the movie SDL audio stream directly instead of routing those state changes through broader device helpers;
  - traced startup runs now show the front-end theme successfully opening and starting `INTRO.AUD` after the intro completes.
- Intro `ESC` abort no longer hard-locks the process:
  - `VQ/VQA32/TASK.CPP` now treats callback-driven `VQAERR_EOF` from the active buffered draw path as a real movie-stop condition instead of ignoring it and continuing to spin the player loop;
  - this lets the intro abort unwind through normal VQA shutdown, return from `VQA_Play()`, and continue startup exactly like a completed movie.
- A compositor close request now shuts the game down cleanly through the SDL/Win32 message bridge instead of leaving the process hung until `SIGKILL`.
- Validation status:
  - `cmake --build build --target redalert -j4` passes.
  - `cmake --build build -j4` passes.
  - `cmake --build build-asan --target redalert -j4` passes.
  - `ctest --test-dir build --output-on-failure` still reports that the repository currently has no registered tests.
  - Running `GameData/redalert` reaches a live `640x480` `Red Alert` window with visible framebuffer updates.
  - `RA_TRACE_STARTUP=1 gdb ./redalert` from `GameData/` now gets through window/audio/media startup instead of aborting immediately in `GetDriveType()`.
  - A captured window image from the rebuilt normal run reports `mean_rgb=22,6,6`, `nonblack_ratio=0.444277`, `bright_ratio=0.365957`.
  - A short monitor capture from the rebuilt normal run reports `abs_mean=1224.67`, `rms=2406.59`, `peak=21253`, `nonzero_ratio=0.808433`.
  - A traced startup run now logs `ENGLISH.VQA` with `block=4x4`, and the first callback samples are non-zero from frame `0` onward instead of staying all black.
  - Two focused intro captures taken a few seconds apart now differ across tens of thousands of pixels, confirming that the intro is animating on-screen instead of presenting a static black frame.
  - Dumped `ENGLISH.VQA` reference frames from `ffmpeg` now match the in-engine buffered output for sampled early and later intro frames (`frame 7` and `frame 15`), replacing the previous quadrant/garbled decode.
  - A fresh traced startup run now reaches `Play_Movie VQA_Play complete`, `Init_Game Play_Intro complete`, `Load_Title_Page complete`, and `Init_One_Time_Systems ... complete` instead of stalling at the intro→menu boundary.
  - A standalone temporary harness linked against the built `libsdl3_compat.a` now passes direct checks for the repaired input bridge:
    - `SDL_SCANCODE_Q` translates to `WM_KEYDOWN` + `VK_Q`;
    - `SDL_SCANCODE_UP` translates to `WM_KEYDOWN` + `VK_UP`;
    - `SDL_EVENT_MOUSE_MOTION` preserves `MK_LBUTTON` in `wParam`;
    - middle and left mouse button events translate to `WM_MBUTTONDOWN` / `WM_LBUTTONDOWN`;
    - `GetAsyncKeyState()` now exposes the expected low-bit press latch for keyboard and mouse buttons.
  - A second standalone temporary harness now confirms that `GetKeyState(VK_CAPITAL)` / `GetKeyState(VK_NUMLOCK)` report only the Win32 low toggle bit (`0x0001`) and no compat-only `0x0008` bit, which is the condition needed for exact intro `KN_ESC` breakout matching in `CODE/CONQUER.CPP`.
  - Running `GameData/redalert-asan` with `ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:new_delete_type_mismatch=0` reaches the same front-end baseline, handles window close, and emits no sanitizer diagnostics on the latest quiet run.
  - `RA_TRACE_STARTUP=1 gdb ./redalert-asan` from `GameData/` now gets through the same startup path without the earlier immediate ASan abort.
  - Re-running the user-visible ASan intro path (`cmake --build build-asan -j32`, copy `build-asan/redalert` to `GameData/redalert`, then `RA_TRACE_STARTUP=1 ./redalert`) now stays alive past the old `UnVQ_4x4` overflow point and emits no sanitizer diagnostics on the traced intro run.
  - A fresh `RA_TRACE_STARTUP=1` ASan run now logs the menu-theme startup path immediately after `Init_One_Time_Systems complete`:
    - `Theme.Play_Song(THEME_INTRO)` reaches `File_Stream_Sample_Vol("INTRO.AUD", ...)`;
    - `File_Stream_Sample_Vol()` returns valid streaming handles for the intro theme startup path;
    - `Stream_Sample_Vol()` returns matching play IDs for those handles.
  - A forced callback-abort `gdb` run now reaches `reached task shutdown`, `entered VQA_StopAudio`, and `returned from VQA_Play` instead of wedging after the callback returns early.
  - A second forced `key == KN_ESC` `gdb` run now confirms the real intro-breakout branch follows the same path:
    - `Brokeout=1` at `Play_Movie VQA_Play complete`;
    - startup then proceeds through `Init_Game Play_Intro complete`, `Load_Title_Page complete`, `Init_One_Time_Systems complete`, and the first main-menu `Theme.Play_Song(THEME_INTRO)` call.
- A focused main-menu `gdb` probe now reaches the cursor-present path in `CODE/MENUS.CPP` with the software cursor visible (`state=0`) and a valid default cursor shape:
  - `MouseShapes` count remains `222`;
  - the extracted menu cursor shape now reads as `30x24`;
  - `WWMouse` records matching runtime dimensions (`curw=30`, `curh=24`) instead of the earlier bogus `6144x109` reject case.
  - A scripted `gdb` intro/menu probe against `GameData/redalert` now validates the SDL-native input rewrite end-to-end:
    - forcing `SDL_GameInput_Handle_Key(0x1B, 41, 0, 0)` from the first `VQ_Call_Back()` hit still breaks out of the intro and reaches `Main_Menu`;
    - pushing a real `SDL_EVENT_MOUSE_MOTION` with `SDL_PushEvent()` at `Main_Menu`, then letting the next `Call_Back()` pump run, updates both `Keyboard->MouseQX/MouseQY` and `GetCursorPos()` to `320,200`;
    - `CODE/CONQUER.CPP::Call_Back()` now pumps SDL every front-end callback, while `SDL3_COMPAT/wrappers/win32_compat.cpp::next_message()` only drains posted lifecycle/focus/quit messages and no longer owns key/mouse translation.
  - A follow-up `gdb` probe at `Main_Menu` now fetches `DD-BKGND.SHP` directly and confirms the active cached-shape layout that motivated the dialog fix:
    - `UseBigShapeBuffer` is `1` on the current host;
    - `Build_Frame()` returns a cached wrapper pointer for that shape;
    - `Get_Shape_Header_Data()` returns a later raw-pixel pointer, so the normal Win32 draw path must unwrap before blitting.
  - Fresh `timeout 20s env RA_TRACE_STARTUP=1 ./redalert` and `timeout 20s env ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:new_delete_type_mismatch=0 RA_TRACE_STARTUP=1 ./redalert-asan` smoke runs both reach the front-end startup baseline and exit without new crashes or sanitizer diagnostics.

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
- Fixed LP64-breaking gameplay `.AUD` parsing by replacing legacy `long` fields in the active `AUDHeaderType` definitions with fixed-width 32-bit integers, restoring the original 12-byte sample header layout used by streamed music and sound effects.
- Fixed LP64-breaking gameplay audio buffer bookkeeping by changing `SampleTrackerType::DestPtr` from a pointer-shaped offset to an integer byte offset and removing the matching pointer-truncating casts in `WIN32LIB/AUDIO/SOUNDIO.CPP` / `WIN32LIB/AUDIO/SOUNDINT.CPP`.
- Replaced the active gameplay DirectSound backend with `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP`, keeping the legacy `Audio_*` / `Play_Sample` behavior and refill semantics intact while moving transport/mixing to SDL3.
- Replaced active WinMM sound-maintenance timer usage in `WIN32LIB/AUDIO/SOUNDIO.CPP` with `Pump_Sound_Service()` plus a sound worker thread.
- Fixed gameplay 8-bit PCM silence handling in `WIN32LIB/AUDIO/SOUNDIO.CPP` / `WIN32LIB/AUDIO/SOUNDINT.CPP` by filling unwritten buffer regions with unsigned silence (`0x80`) instead of signed-style zero bytes.
- Fixed LP64-breaking gameplay compressed `.AUD` frame parsing in `WIN32LIB/AUDIO/SOUNDINT.CPP` by restoring the per-frame `0xDEAF` marker to a 32-bit read instead of a native `long`. `INTRO.AUD` traces now confirm the active front-end music path is `comp=99` (SOS ADPCM), the first streamed quarter decodes to a full `8192` bytes again, and the old front-end theme restart loop no longer reproduces in a traced startup run.
- Fixed the active gameplay sound-tracker bookkeeping sizes in `WIN32LIB/AUDIO/SOUNDINT.H` / `WIN32LIB/INCLUDE/SOUNDINT.H` by keeping the stream magic and byte-count fields (`MagicNumber`, `StreamBufferSize`, `FilePendingSize`) at 32-bit widths instead of LP64-native `long`.
- A timed `RA_TRACE_STARTUP=1 ./redalert-asan` smoke run from `GameData/` now repeatedly reaches `File_Stream_Sample_Vol("INTRO.AUD", ...)`, `Stream_Sample_Vol(...)`, and later front-end theme restarts without reproducing the old `Play_Sample_Handle()` segfault; the remaining forced-timeout ASan output is unrelated leak noise.
- A follow-up `RA_TRACE_STARTUP=1 timeout 25s ./redalert-asan` run from `GameData/` now shows exactly one `Theme.Play_Song(INTRO)` / `Stream_Sample_Vol("INTRO.AUD")` start, no repeated song churn, and no ASan errors before the forced timeout; only the pre-existing forced-timeout leak summary remains.
- Fixed the shutdown-time `SessionClass::Free_Scenario_Descriptions()` allocation mismatch by changing the `InitStrings` cleanup from `delete` to `delete[]`, matching the `new char[INITSTRBUF_MAX]` allocations in `Read_MultiPlayer_Settings()`.
- Fixed the active public/private `VQAConfig` layout mismatch by keeping `SoundObject` / `PrimaryBufferPtr` as backend-agnostic placeholder pointers on the game include path and in `CONFIG.CPP`.
- Fixed the active movie ADPCM ABI mismatch by restoring the `_SOS_COMPRESS_INFO` field order expected by the game-side decoder and routing movie decode through `General_sosCODECDecompressData()`.
- Restored buffered `4x4` VQA decode support by porting the old `UnVQ_4x4` helper into `VQ/VQA32/UNVQCOMPAT.CPP` and enabling the `VQABLOCK_4X4` path used by the startup intro movies.
- Fixed buffered `4x4` VQA codebook sizing in `VQ/VQA32/LOADER.CPP`: startup movies use direct 15-bit codebook offsets in `UnVQ_4x4`, so `Max_CB_Size` must cover the full addressable `4x4` pointer range rather than only `CBentries * 16`; this removes the ASan `heap-buffer-overflow` and avoids truncating the decompressed intro codebook.
- Fixed buffered version-2 `4x4` VQA decode in `VQ/VQA32/UNVQCOMPAT.CPP` / `VQ/VQA32/DRAWER.CPP` by restoring the split low-byte/high-byte pointer map used by palettized VQA `4x4` streams like `ENGLISH.VQA`; the earlier direct-16-bit decoder choice was what produced the remaining quadrant/garbled intro frames.
- Fixed buffered VQA palette upload on the active SDL/Linux movie path by teaching `VQ/VQA32/DRAWER.CPP::DrawFrame_Buffer()` to apply frame palettes and deferred skipped-frame palettes before the game callback blits the decoded image to `SeenBuff`.
- Fixed SDL movie color presentation in `SDL3_COMPAT/wrappers/ddraw_compat.cpp` by switching the compat texture format from `SDL_PIXELFORMAT_RGBA8888` to `SDL_PIXELFORMAT_ARGB8888` and disabling texture blending; the old format mismatch made palette-expanded movie pixels show up as dark/red shades.
- Fixed SDL/Win32 menu click handling by updating `CODE/KEY.CPP` so `WM_MOUSEMOVE` refreshes `MouseQX` / `MouseQY` and queued mouse-button events latch the current coordinates immediately.
- Fixed SDL/Win32 input translation/state handling in `SDL3_COMPAT/wrappers/win32_compat.cpp`:
  - SDL key events now translate to Win32 virtual keys instead of raw SDL keycodes;
  - `GetKeyState()` / `GetAsyncKeyState()` now report ordinary keys and mouse buttons correctly;
  - `GetAsyncKeyState()` now keeps a one-shot low-bit press latch for callers that rely on Win32 transition semantics;
  - middle mouse now maps to `WM_MBUTTON*`, and mouse motion/button events preserve `MK_*` state.
- Fixed compat toggle-key state reporting in `SDL3_COMPAT/wrappers/win32_compat.cpp` so `GetKeyState(VK_CAPITAL)` / `GetKeyState(VK_NUMLOCK)` now match Win32 low-bit semantics instead of polluting legacy key queueing with a fake `0x0008` shift bit.
- Replaced the active fake SDL→Win32 keyboard/mouse bridge with a dedicated SDL game-input layer in `CODE/SDLINPUT.CPP` / `CODE/SDLINPUT.H`.
  - `WWKeyboardClass::Fill_Buffer_From_System()` now pumps SDL input directly before draining the remaining compat window-message queue;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` no longer fabricates `WM_KEY*` / `WM_MOUSE*` messages for normal gameplay input, but still posts focus/close messages for the legacy front-end flow;
  - `GetCursorPos()` and `CODE/MOUSEUTIL.CPP` now use the shared SDL input snapshot instead of raw `SDL_GetMouseState()`, so the software cursor redraw thread and the menu queue consume one coherent mouse position.
- Fixed `CODE/KEY.CPP` to test the real Win32 toggle low bit (`0x0001`) when adding synthetic shift state for CapsLock/NumLock, matching the repaired SDL input state layer.
- Fixed main-menu cursor visibility in `CODE/MENUS.CPP` by normalizing the Westwood software-cursor hide count to visible-on-entry and restoring the prior hide count on exit.
- Fixed VQA/movie audio pause-resume scoping in `VQ/VQA32/AUDIOCOMPAT.CPP` by pausing/resuming the movie SDL stream directly instead of routing those state changes through broader device helpers.
- Fixed buffered VQA movie abort handling in `VQ/VQA32/TASK.CPP` so callback-driven `VQAERR_EOF` (including intro `ESC` breakout) is treated as movie completion instead of being ignored and leaving the player loop spinning with update state still set.
- Added `RA_TRACE_STARTUP` diagnostics around `Theme.Play_Song()` and streamed score startup in `WIN32LIB/AUDIO/SOUNDIO.CPP` so intro→menu theme handoff regressions can be traced without reintroducing always-on callback spam.
- Fixed post-bootstrap focus-loss handling in `CODE/SDLINPUT.CPP` / `CODE/WINSTUB.CPP` so ordinary SDL focus loss no longer clears `GameInFocus`, stops rendering/video playback, or blocks `Start_Primary_Sound_Buffer()`; this removes the black-screen/audio-stop/theme-restart behavior seen when the window is unfocused.
- Fixed the active Win32/SDL cached-shape draw path in `CODE/CONQUER.CPP::CC_Draw_Shape()` by unwrapping `Build_Frame()` results with `Get_Shape_Header_Data()` before `Buffer_Frame_To_Page()`; on modern-memory hosts with `UseBigShapeBuffer` enabled, the old code was blitting the wrapper/header scratch area instead of the real raw pixels, which left dialog backdrops like `DD-BKGND.SHP` effectively blank.
- Fixed the active software-shape blitter in `CODE/KEYFBUFF.CPP::Buffer_Frame_To_Page()` to advance rows with the real viewport stride (`Get_Width() + Get_XAdd() + Get_Pitch()`) instead of `Get_Pitch()` alone.
  - On this codebase, `GraphicViewPortClass::Pitch` is the end-of-line skip, not the full bytes-per-row stride.
  - The old Linux/SDL port only happened to work for full-width buffers; clipped sub-viewports such as `WINDOW_PARTIAL` in `Dialog_Box()` advanced by `0` on `640x480 -> 500x160` dialog draws, collapsing multi-row shapes like `DD-BKGND.SHP` into a single scanline and leaving the dialog background effectively missing.
  - A focused `gdb` probe that first cleared `HidPage` to zero now shows `Dialog_Box(70, 120, 500, 160)` writing non-zero pixels into the underlying hidden/visible DirectDraw surface buffers where the pre-fix probe stayed all-zero across the dialog region.
- Fixed the SDL DirectDraw presentation cadence in `SDL3_COMPAT/wrappers/ddraw_compat.cpp`, `CODE/GADGET.CPP`, and `CODE/CONQUER.CPP` by queuing primary-surface presents and flushing them coherently instead of presenting on every intermediate UI primitive.
  - Primary-surface `Unlock()`, `Blt()`, and palette updates now mark a pending present instead of calling `SDL_RenderPresent()` immediately.
  - `GadgetClass::Draw_All()` now brackets list redraws in a present batch, and `Call_Back()` flushes any pending primary update once per front-end tick.
  - This matches the old whole-frame/front-buffer behavior much more closely on SDL and prevents dialog/button overlays drawn directly on `SeenBuff` from repainting one control at a time across multiple visible frames.
- Fixed the follow-on VQA movie regression in `CODE/CONQUER.CPP::VQ_Call_Back()` by flushing queued DirectDraw presents after each movie frame blit.
  - The deferred-present change above fixed front-end overlays, but the Win32 movie callback does not call `Call_Back()`, so VQA frames were reaching `SeenBuff` without any later present flush and the window stayed black.
  - The latest traced normal and ASan startup runs now show `Play_Movie callback frame=N` immediately followed by `[ddraw] present ...` lines, and both runs exit with status `0` instead of hanging until timeout.
- Fixed the clean-shutdown hang by restoring `ReadyToQuit` to an integer state variable in `CODE/GLOBALS.CPP` / `CODE/EXTERNS.H`.
  - The shutdown code in `CODE/STARTUP.CPP` / `CODE/WINSTUB.CPP` uses `ReadyToQuit` as a 4-state flag (`0`, `1`, `2`, `3`), but the active port had it declared as `bool`.
  - That truncated `ReadyToQuit = 2` back to `true`, so the `while (ReadyToQuit == 1)` shutdown waits never completed even though `WM_DESTROY` had already run and logged.
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

1. Re-test front-end interaction with real user-driven pointer input on SDL/Wayland to confirm that visible cursor motion, click behavior, and intro `ESC` breakout now match the repaired compat/input/VQA-stop behavior in live use.
2. Re-check additional VQA playback beyond `ENGLISH.VQA`/`REDINTRO` to confirm no other movie variants rely on version-specific pointer handling that is still missing on the SDL3/Linux path.
3. Validate the same CMake target on Windows and fix any remaining case, type-width, or wrapper issues that only surface there.
4. Continue tightening remaining legacy Win32/DirectDraw/audio assumptions only where runtime behavior or sanitizers still prove they are wrong on 64-bit/SDL3 builds.
5. Keep the porting docs aligned with each verified runtime or cross-platform milestone.
