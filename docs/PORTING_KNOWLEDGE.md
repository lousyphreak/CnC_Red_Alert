# Porting Knowledge

_Last updated: 2026-04-01_

## Repo facts

- The repository ships a bundled SDL3 source tree in `extern/SDL3`.
- The main game logic lives in `CODE/`.
- Shared platform/rendering/input/audio support code lives in `WIN32LIB/`.
- The active compatibility include order puts `SDL3_COMPAT/wrappers/` ahead of `CODE/` and `WIN32LIB/INCLUDE`.

## Dead-code cleanup guardrails

- "Not selected by CMake" is not sufficient proof that a file is dead in this tree.
- `CODE/ADPCM.CPP` still directly `#include`s `CODE/ITABLE.CPP` and `CODE/DTABLE.CPP`, so those two `.CPP` files are still active implementation fragments even though they are filtered from the top-level `CODE/*.CPP` build list.
- `CODE/2KEYFRAM.CPP` is still a live build input because the current `CMakeLists.txt` globs `CODE/*.CPP` and does **not** exclude that file. It provides the active `Build_Frame()`, `Get_Build_Frame_Count()`, `Get_Build_Frame_Width()`, `Get_Build_Frame_Height()`, `Get_Shape_Header_Data()`, and `IsTheaterShape` symbols used throughout the game.
- The broad dead-code cleanup safely removed non-built archive/tool trees such as `LAUNCH/`, `LAUNCHER/`, `TOOLS/`, `IPX/`, `WWFLAT32/`, `VQ/VQM32/`, and the non-built `WINVQ/` source subtrees, plus non-built `WIN32LIB` archive/test trees. If more cleanup is needed later, start from those categories before touching `CODE/` implementation fragments or globbed root `.CPP` files.
- A safe second-pass target category is legacy build metadata that the SDL3/CMake build never reads: Borland/Watcom/TASM config files (`*.MAK`, `MAKEFILE*`, `*.CFG`, `*.DEF`, old batch rebuild scripts), old backup files (`*.BAK*`), and empty/obsolete assembler include stubs such as `FUNCTION.I`.
- The remaining `CODE/*.ASM` files that were deleted in the second pass were not live build inputs on the SDL3 port. The only surviving references were comments, old project files, or archive `.INC` declarations; there were no active CMake targets or in-tree source includes depending on those assembler files.
- Orphan headers that belonged only to already-deleted non-built implementation files (`BMP8.H`, `DIBUTIL.H`, `DPMI.H`, `TARCOM.H`, `TURRET.H`) were also safe to remove once repository-wide reference checks came back empty.
- For later cleanup passes, use compile commands plus case-insensitive repository search together:
  - if a file appears in `build/compile_commands.json` or `build-asan/compile_commands.json`, treat it as part of the active build even if it looks feature-dead;
  - only treat remaining headers as safe stragglers when they are absent from compile commands **and** have no surviving case-insensitive include/reference hits elsewhere in the repo.
- That rule is what made the third pass safe: the removable leftovers were orphan headers only, while many seemingly dormant `CODE/*.CPP` units were still compiled because the active CMake target globs almost every root-level `CODE/*.CPP`.
- Raw byte-identical duplicate headers are **not** automatically safe to delete in this tree.
  - Cross-tree duplicates can still be removable when an earlier include root shadows them reliably (for example `WINVQ/INCLUDE/VQM32/*.H` behind `VQ/INCLUDE/VQM32/*.H`, or `VQ/INCLUDE/WWLIB32/{DESCMGMT.H,DIPTHONG.H,PLAYCD.H}` behind `WIN32LIB/INCLUDE/`).
  - Same-library local shadow headers under `WIN32LIB/*/` must be treated more conservatively even when they match `WIN32LIB/INCLUDE/*` byte-for-byte: local `#include "HEADER.H"` in subdirectory sources can otherwise fall through to the global include path and accidentally pick same-basename headers from `CODE/` or other trees. The failed `WIN32LIB/KEYBOARD/KEYBOARD.H` / `MOUSE.H` deletion attempt is the concrete example.
- For the active SDL3 build, `VQ/INCLUDE` is the canonical live VQA/VQM include root; `WINVQ/INCLUDE/VQA32` and `WINVQ/INCLUDE/VQM32` were fully unused.
  - `CMakeLists.txt` adds `VQ/INCLUDE` before `WINVQ/INCLUDE` in both the casefix and game include paths.
  - Dependency-file checks in both `build/` and `build-asan/` showed active references to `VQ/INCLUDE/VQA32` and `VQ/INCLUDE/VQM32`, but zero references to any header under `WINVQ/INCLUDE/VQA32` or `WINVQ/INCLUDE/VQM32`.
  - Keep that evidence in mind before assuming the `WINVQ` tree is the “Windows” canonical copy. In the supported build here, it was dead duplicate baggage and was removed.
- The same conclusion extended to the last top-level `WINVQ/INCLUDE` leftovers:
  - active depfiles still used `VQ/INCLUDE/VQ.H`, not `WINVQ/INCLUDE/VQ.H`;
  - `WINVQ/INCLUDE/VQFILE.H` had no surviving source or depfile references;
  - the remaining `WINVQ/INCLUDE/VQM32/*.I` assembler include files (`VESAVID.I`, `VGA.I`, `VIDEO.I`) had no surviving repository references and were also safe to delete.
  - Net result: `WINVQ/INCLUDE` is entirely dead for the current SDL3/CMake build and has been removed.
- The remaining `WINVQ/LIB` bundle was dead too.
  - No source, CMake file, depfile, or build artifact referenced `WINVQ/LIB` or any of its shipped `.LIB` names.
  - The whole `WINVQ/` tree can now be treated as removed legacy baggage for the supported port.
- The same archive-file rule applied to `VQ/LIB/README.TXT`.
  - Once `WINVQ/` was gone, `VQ/LIB/` contained only an old library-naming note with no surviving build or source references.
  - `VQ/LIB/` was safe to remove, but `VQ/INCLUDE/` and `VQ/VQA32/` are still live and must remain.

## Removed objbase wrapper

- Do not reintroduce `SDL3_COMPAT/wrappers/objbase.h`. Because `SDL3_COMPAT/wrappers/` is first on the include path, an `objbase.h` shim there overrides the real Windows SDK COM header for both Red Alert code and bundled SDL Windows sources.
- `CODE/COMINIT.CPP/.H` are now gone. Do not add a replacement startup COM/OLE initializer for the SDL3 port; game-owned COM setup is intentionally removed on all platforms.
- `WIN32LIB/AUDIO/SOUNDLCK.CPP` and its archive copies never used any `objbase.h` declarations; if they pick up that include again, treat it as dead baggage and remove it instead of adding another wrapper.
- The old Westwood Online client bridge was COM-based all the way down (`RAWOLAPI`, `WOLAPIOB`, generated `WOLAPI` IDL headers, `CoCreateInstance`, `IUnknown` sinks, connection points, `ole2`/`olectl` headers). That entire bridge is deleted now, and the old Watcom build no longer defines `WOLAPI_INTEGRATION`.
- The dead DirectShow movie implementation under `WIN32LIB/MOVIE/` and the trivial `ole2.h` test stubs under `WIN32LIB/AUDIO/`, `WIN32LIB/AUDIO/OLD/`, and `WIN32LIB/SRCDEBUG/` are also deleted. The supported movie/audio path is SDL/VQA-based, not COM-based.

## Removed io wrapper

- `SDL3_COMPAT/wrappers/io.h` is gone. After the earlier file-I/O cleanup removed `filelength()`, it had become a dead passthrough wrapper for `<fcntl.h>` and `<unistd.h>` only.
- Repository-wide search found no remaining Red Alert-owned source file that still includes `<io.h>`.
- The only remaining repository `<io.h>` include is SDL upstream's Windows-only `extern/SDL3/test/childprocess.c`, and that target is outside the supported build here because the top-level CMake forces `SDL_TESTS OFF`. Do not reintroduce an `io.h` wrapper just for that upstream test file.

## Removed dos wrapper

- `SDL3_COMPAT/wrappers/dos.h` and `SDL3_COMPAT/wrappers/dos_compat.cpp` are gone. The full cleanup had two parts:
  - replace the few still-live DOS-era seams in the supported build (`CODE/CONQUER.CPP` free-space query, `CODE/SESSION.CPP` station-ID entropy, `CODE/STARTUP.CPP`/`WIN32LIB/KEYBOARD/TEST/TEST.CPP` drive-directory setup, and the stray `CDFILE`/`WWSTD` transitive include assumptions);
  - strip the much larger pile of stale `<dos.h>` include lines and dead `find_t`-based declarations from archive/backup headers and sources.
- Do not reintroduce `find_t`, `diskfree_t`, `_dos_getdrive`, `_dos_setdrive`, `_dos_getdiskfree`, `_harderr`, `_hardresume`, or the old `_A_*` / `_HARDERR_*` macro surface for the supported SDL3 port. The live game/file-system code already uses direct SDL/native helpers instead.
- If a file still needs Win32 compatibility typedefs/macros such as `LONG`, `BYTE`, `FALSE`, `cdecl`, or `_MAX_PATH`, include `win32_compat.h` directly. Do not rely on a removed DOS shim to drag those definitions in transitively.
- If a future change needs disk-space or path checks, keep that logic at the actual call site and route it through SDL helpers (`SDL_OpenFileStorage`, `SDL_GetStorageSpaceRemaining`, `SDL_GetPathInfo`, etc.) instead of reviving a DOS-shaped compatibility layer or adding `std::filesystem` back in.

## SDL filesystem helper layer

- SDL3 intentionally does **not** provide a real `SetCurrentDirectory` API. Do not sneak raw `chdir()` back into the port to compensate.
- The durable seam is now:
  - `SDL3_COMPAT/wrappers/sdl_fs.cpp` owns the WWFS filesystem implementation: virtual current-directory state, relative/case-insensitive path resolution, virtual-CD path mapping, and the SDL-backed path/storage helpers (`WWFS_SetCurrentDirectory()`, `WWFS_GetCurrentDirectory()`, `WWFS_OpenFile()`, `WWFS_OpenFileStorage()`, `WWFS_GetPathInfo()`, `WWFS_RemovePath()`, `WWFS_CreateDirectory()`, `WWFS_NormalizePath()`, `WWFS_GlobDirectory()`, `WWFS_SplitPath()`, and the virtual-CD helpers used by Win32 drive shims);
  - `SDL3_COMPAT/wrappers/sdl_fs.h` provides the public WWFS-prefixed path plus stdio/fd compatibility surface (`WWFS_NormalizePath`, `WWFS_GlobDirectory`, `WWFS_SplitPath`, `WWFS_FOpen`, `WWFS_FRead`, `WWFS_FWrite`, `WWFS_FSeek`, `WWFS_FTell`, `WWFS_Open`, `WWFS_Read`, `WWFS_Write`, `WWFS_Seek`, `WWFS_Close`, `WWFS_ChangeDirectory`, `WWFS_GetCurrentDirectory`, etc.) for legacy code that still expects those APIs;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` should stay a consumer of that layer for Win32-facing drive/window shims, not the home of the filesystem implementation itself.
  - `SDL3_COMPAT/wrappers/direct.h` now forwards `_splitpath()` to `WWFS_SplitPath()` so even the legacy DOS-shaped path decomposition stays inside the SDL-backed filesystem layer instead of carrying a separate hand-rolled parser.
  - Any wildcard scan that is meant to follow the game's virtual cwd must use `WWFS_GlobDirectory()`, not raw `SDL_GlobDirectory(".")`. The latter uses the host process cwd and will miss files such as `GameData/SAVEGAME.###` when startup has only redirected the WWFS virtual cwd.
- When porting more filesystem code:
  - route new file operations through SDL or these compat helpers;
  - prefer the low-level helper layer over scattering direct `SDL_IOFromFile` / `SDL_GetPathInfo` / `SDL_RemovePath` calls when the code needs relative-path behavior that historically depended on the process cwd;
  - keep upper layers unaware of case sensitivity and cwd semantics; that logic belongs in the file/path abstraction, not in gameplay code.
- Repository-wide searches for direct filesystem/file-I/O calls will still show some benign leftovers:
  - comments/documentation snippets,
  - helper definitions in `direct.h`,
  - container/member-function `remove(...)` calls,
  - socket `close(...)` in `winsock.h`.
  Treat those as non-file-I/O false positives, not as reasons to reintroduce POSIX/stdio access.

## Removed conio surface

- The supported build no longer relies on global `WIN32` compatibility defines, and the old `conio.h` fallback branches were removed during that cleanup. Keep keyboard input on the supported port routed through `WWKeyboardClass` / SDL-backed input; do not reintroduce `<conio.h>` or wrapper shims for `getch()`, `kbhit()`, `getche()`, or `cprintf()`.
- The `TESTVB` utilities only needed `inp()`. Include `PORTIO.H` directly there instead of routing through `conio.h`.
- Clearly orphaned backup/standalone sources that only remained around the removed `conio` utility path were deleted instead of being rewrapped.

## Removed BIOS/register surface

- `SDL3_COMPAT/wrappers/bios.h` is gone. Do not reintroduce `union REGS`, `struct SREGS`, `bioskey()`, `segread()`, or `int386*`-style shims for the SDL3 port.
- The supported port no longer carries any BIOS/DPMI/VESA/IPX real-mode fallback code in active sources.
  - `CODE/IPX.CPP` keeps only the Win32/IPX95 path;
  - `CODE/IPXMGR.CPP::Alloc_RealMode_Mem()` / `Free_RealMode_Mem()` are now explicit no-op success stubs on the supported path;
  - `CODE/CDFILE.CPP` and `CODE/NULLMGR.CPP` no longer keep register-access fallback branches;
  - `WIN32LIB/MEM/ALLOC.CPP` now preserves only the supported malloc/free path.
- The duplicate archive headers under `WIN32LIB/`, `VQ/INCLUDE/WWLIB32/`, and `WWFLAT32/` were trimmed so they no longer expose BIOS/DPMI CD-audio or descriptor-management types.
- Archive-only source files that were wholly tied to BIOS/register access (old VESA backends, VQM32 real-mode helpers, DPMI allocators, and the old `MPLIB` DOS transport sources) were deleted rather than stubbed. If similar dead DOS-era files are found later, prefer deletion over adding compat wrappers.

## Mission-start crash-chain findings

- `VectorClass` callers that sort with `qsort(&vec[0], ...)` must guard empty counts; `operator[]` on an empty vector binds a reference to null before `qsort` even runs.
- `Buffer` self-owned storage is allocated as `new char[]`; releasing it through `void *` selects the wrong delete operator on ASan-enabled LP64 builds.
- `SIDE?NA.SHP`-style theater templates must be copied into writable storage before replacing `'?'`; Linux places string literals in read-only storage.
- `OverlayPack` is a signed one-byte-per-cell stream; `OverlayType` must stay 8-bit or the loader reads multiple packed cells into one enum value (`0xFF0706FF` was the traced symptom).
- `TemplateType` is a 16-bit on-disk value in map/template data. Letting it widen breaks template/icon lookups after scenario load.
- `COORDINATE` / `TARGET` are Win32-sized 32-bit values, not native `long`.
- `CELL_COMPOSITE`, `LEPTON_COMPOSITE`, and `COORD_COMPOSITE` helper packing is compiler-/layout-dependent; map/view helpers should use explicit mask/shift math instead of union layout or short-pointer punning.
- The mission-entry black-map / `"unrevealed terrain"` symptom came from the remaining packed-coordinate helpers in `CODE/INLINE.H` / `CODE/COORD.CPP`, not from the scenario map bounds themselves.
  - The traced bad state still loaded sane map bounds (`MapCell=49,45 size=30x36`) and a sane home waypoint (`6079`), but `TacticalCoord` was poisoned before the mission-home adjustment ran.
  - Rewriting the active `LEPTON` / `COORDINATE` helpers (`Cell_To_Lepton`, `XY_Coord`, `Coord_X/Y`, `Coord_{Whole,Snap,Fraction,Add,Sub,Mid}`, `Coord_Move`, and the spillage-list bounds math) to explicit bit operations removes that platform-sensitive seam.
  - Mission/home-view setup in `CODE/DISPLAY.CPP` / `CODE/SCENARIO.CPP` should use `Coord_Whole(Cell_Coord(...))` so scenario start matches the existing bookmark-restore and computed-start behavior.
- Legacy iconset headers (`IControl_Type` / `IconsetClass`) contain on-disk offsets, not host pointers; the active headers must stay packed/fixed-width and the loader must translate offsets explicitly.
- The active WSA loader has the same basic LP64 hazard pattern as iconsets and `.AUD` files.
  - The WSA "header" read in `WIN32LIB/WSA/WSA.CPP::Open_Animation()` is really the 14-byte file header plus the first two 32-bit frame offsets; reading those offset slots as host `unsigned long` over-reads the table on LP64 and can surface later as bogus LCW back-references/ASan crashes in `LCW_Uncompress()`.
  - `file_header.largest_frame_size` is stored with legacy 32-bit Animate header accounting only through the `flags` field, so the loader must translate from that legacy header size to the current host `SysAnimHeaderType` size before laying out the in-memory animation buffers.
  - The same WSA scratch buffer is used for two different things: the decoded LCW delta stream at the front and a back-loaded compressed frame span at the end before `LCW_Uncompress()` runs. Some assets (for example the map-selection `MSAA.WSA` path) need more room for the compressed span than the recorded decoded-delta size alone, so the loader should size the workspace from the maximum of those two values.
  - The active build uses `CODE/LCWUNCMP.CPP`, whose `LCW_Uncompress()` implementation ignores its length argument and stops on the encoded end marker. That makes it safe to enlarge the WSA scratch workspace to cover the largest compressed frame span without changing decode behavior.
  - `Apply_Delta()` should still bounds-check `frame_data_size` against the reserved WSA workspace, because a bad or inconsistent frame-offset table otherwise turns the back-load `Mem_Copy()` / `Read_File()` into an out-of-bounds walk before LCW decode even starts.
- `Assign_Mission()` only queues `MissionQueue`; when scenario loaders need an immediate live mission before `Enter_Idle_Mode()`, they must use `Set_Mission()`.
- `List_Copy()` callers rely on a trailing `REFRESH_EOL`; if truncation is possible, reserve one slot for the terminator.
- `DisplayClass::Set_Cursor_Shape()` also needs sentinel-aware copying.
  - Its input is not a fixed 50-entry array; it is a variable-length `REFRESH_EOL`-terminated cell-offset list.
  - The building-placement path can pass the temporary 25-entry buffer returned by `BuildingTypeClass::Occupy_List(true)` when bib/smudge cells are appended in `CODE/BDATA.CPP`.
  - Copying that source with `memcpy(sizeof(local_buffer))` reads past the shorter global/static object on ASan builds even though the logical list contents are valid.
  - Keep the owned static cursor buffer in `Set_Cursor_Shape()`, but populate it with `List_Copy(..., ARRAY_SIZE(buffer), ...)` instead of raw fixed-size `memcpy()`.
- `Coord_Spillage_List(COORDINATE, Rect, ...)` can return up to 128 offsets; 32-entry temporary lists in map/display code are not universally safe.
- `HelpClass::OverlapList` is mutable scratch storage despite the legacy `const` declaration. Linux puts the old declaration in read-only storage, so the cast-away-const write crashes.
- In-place `Path[]` compaction in `DriveClass` requires `memmove()`, not `memcpy()`.
- `DriveClass` is a shared base for both `UnitClass` and `VesselClass`; any shared movement/UI logic that wants `UnitClass`-only state such as `Flagged` must guard `What_Am_I() == RTTI_UNIT` before casting.
- The in-tree LZO1X compressor workmem contract is pointer-sized: `LZO1X_MEM_COMPRESS == 16384 * sizeof(lzo_byte *)`.
  - the old fixed `64*1024` workmem allocations in `LZOPipe` / `LZOStraw` only matched 32-bit builds;
  - on LP64, `CODE/LZO1X_C.CPP::do_compress()` immediately writes 8-byte dictionary entries, so callers must allocate the full `LZO1X_MEM_COMPRESS` bytes or the save/compression path overruns the dictionary table;
  - a focused ASan round-trip smoke against `CODE/LZO1X_C.CPP` / `CODE/LZO1X_D.CPP` now reports `workmem=131072`, which is the expected LP64 size.
- `DirType`, `FacingType`, and the active drive-facing control enums must stay 8-bit on the SDL/Linux port.
  - `Path[]`, `FacingClass`, the direction lookup helpers, and `DriveClass::TurnTrackType` all assume original byte-sized facing storage; leaving those enums host-sized recreates movement-state corruption on LP64 hosts and forces fragile “trim back to 8-bit” helpers into the active code.
  - The durable fix is to restore the underlying enum size directly, represent `FACING_NONE` explicitly as `0xFF`, remove `Dir_Index()` / `Normalize_Dir()`, and then convert the few active signed-facing sites to explicit integer math (`FINDPATH.CPP` optimization deltas, `COMBAT.CPP` center-plus-adjacents sweep, `FOOT.CPP` path debug print).
  - `Dir_Facing()` still needs wraparound after rounding. On the active 8-bit port, transient high `DirType` values near `0xFF` are legitimate during facing interpolation; if the helper returns the raw rounded result, those values become `8` instead of wrapping to `0`, which overruns 8-entry facing tables (the traced symptom was `CODE/AIRCRAFT.CPP::Draw_Rotors()` indexing `_stretch[8]`).
- Team scripts can legitimately have `CurrentMission == -1` while regrouping; any direct `MissionList[CurrentMission]` access must guard that state.
- `Cell_Occupier()` may return terrain or other non-techno `ObjectClass` instances; `TechnoClass` AI code must check `Is_Techno()` before casting.
- Fixed-heap pooled classes must not touch `IsActive` from `operator new/delete`.
  - On the active Clang/UBSan build that writes into object storage before construction and after destruction, which shows up as invalid-vptr/member-access UB in pooled classes such as `TeamClass`, `AircraftClass`, `BuildingClass`, `TriggerTypeClass`, and other `TFixedIHeapClass<>` users.
  - The safe seam is constructor/destructor-owned bookkeeping (`AbstractClass` for battlefield objects, plus the standalone house/factory/trigger/team-type families that carry their own `IsActive` bit).
- `InfantryClass::Doing_AI()` is a self-delete seam.
  - When death-animation completion reaches the `delete this` path, callers must return immediately rather than checking more infantry state afterward.
  - On the active UBSan build, continuing into later infantry AI phases after that point reports the object as `AbstractClass` because destruction has already unwound the dynamic type back through the base-class vtables; the fix is to have `Doing_AI()` report deletion explicitly and let `InfantryClass::AI()` bail out before `Movement_AI()`.
- Building availability/prerequisite scan masks are only reliable for the original 32 tracked structure IDs.
  - Use a guarded helper (`Structure_Scan_Bit(...)`) instead of raw `1L << type` shifts on LP64 hosts.
  - For higher structure enums (fake structures, civilian structures, and similar out-of-range cases), use `Get_Quantity(...)` instead of pretending they fit in the building scan bitfield.
- `Select_Game()` already contains a useful no-menu mission-entry path for debugging.
  - If `Debug_Map` is true, it skips the front-end selection flow, sets `Scen.ScenarioName` to `SCG01EA.INI`, and then falls through to the normal `Start_Scenario(...)` call.
  - A `gdb` breakpoint script that flips `Debug_Map = true` at `Select_Game()` is a practical way to reproduce in-mission issues without manual UI input.
- Some shipped `NewINIFormat=3` scenario trigger entries still carry legacy packed speech payloads instead of plain `VoxType` integers.
  - A focused `gdb` load trace against `SCG01EA.INI` showed `ein3`, `shl3`, and `dcop` loading `TACTION_PLAY_SPEECH` data as `-191`, `-143`, and `-192`, with raw bytes `0x41 FF FF FF`, `0x71 FF FF FF`, and `0x40 FF FF FF`.
  - The low byte is the real speech ID (`65`, `113`, `64` respectively); normalize those values during `TActionClass::Read_INI()` when `Action_Needs(action) == NEED_SPEECH`, the stored value is less than `VOX_NONE`, and `value & 0xFF` falls inside `[VOX_FIRST, VOX_COUNT)`.
  - `TActionClass::Data.Value` and `TEventClass::Data.Value` must stay fixed 32-bit fields. Their original layouts are `16` bytes for `TActionClass` and `12` bytes for `TEventClass`; letting the generic payload widen back to native `long` reintroduces LP64 layout drift around trigger/event storage and serialization.
- `AnimClass::operator new()` must stay non-throwing on the active port.
  - The animation pool is a fixed heap (`Anims.Allocate()` -> `FixedIHeapClass::Allocate()`), and exhaustion is a normal legacy condition: once `Rule.AnimMax` objects are active, allocation returns `NULL`.
  - If `AnimClass::operator new(size_t)` is left as a throwing allocation function on modern C++, a plain `new AnimClass(...)` expression can still enter `AnimClass::AnimClass(...)` with `this == NULL`, which is exactly the reported `CODE/ANIM.CPP` crash signature.
  - Declaring the allocator `noexcept` restores the engine's expected semantics: a failed animation allocation makes the `new` expression yield `NULL`, callers that already check keep working, and unassigned cosmetic side-effect spawns are simply skipped instead of crashing.
  - A tiny standalone C++ repro is a good sanity check here: a throwing-style custom `operator new` that returns `nullptr` still ran the constructor on this toolchain, while the `noexcept` version returned null without a constructor call.

## Gameplay audio system observations

- The active gameplay audio path is `CODE/AUDIO.CPP` -> `WIN32LIB/AUDIO/SOUNDIO.CPP` -> `WIN32LIB/AUDIO/SOUNDINT.CPP`.
- The safest porting seam is below the legacy `Audio_*`, `Play_Sample`, and sample-tracker logic. Keep those APIs and refill rules stable; replace only the device/buffer backend under them.
- The active gameplay backend is now `WIN32LIB/INCLUDE/SDLAUDIOBACKEND.H` plus `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP`.
  - `AudioBackendDevice`, `AudioBackendBuffer`, and `AudioBufferFormat` replace the active DirectSound types.
  - The backend uses SDL3 audio streams/device output internally while preserving the old primary/secondary buffer contract expected by `SOUNDIO.CPP` and `SOUNDINT.CPP`.
- `WIN32LIB/AUDIO/SOUNDIO.CPP` no longer uses `DirectSoundCreate()` or WinMM timer callbacks on the active path.
  - maintenance now runs through `Pump_Sound_Service()` plus `Start_Sound_Thread()` / `Stop_Sound_Thread()`;
  - the service thread still uses the generic Win32-compat thread helpers, but audio transport/output is SDL-backed.
- The gameplay `.AUD` loader/parser is separate from the working movie-audio path. If movies sound right but menu music or sound effects do not, investigate `WIN32LIB/AUDIO/SOUNDIO.CPP` and the active `AUDHeaderType` definitions before touching VQA audio code.
- The active gameplay `AUDHeaderType` must stay at the original 12-byte on-disk layout.
  - Leaving legacy `long` fields in `WIN32LIB/AUDIO/AUDIO.H` / `WIN32LIB/INCLUDE/AUDIO.H` grows the header to 20 bytes on LP64 hosts.
  - That shifts sample-data offsets, corrupts flags/compression reads, and can make streamed theme music sound garbled while the separate movie SDL stream still plays correctly.
- `SampleTrackerType::DestPtr` in the active gameplay sound path is a byte offset, not a host pointer.
  - Keeping it as `void *` and routing it through `(unsigned)` casts worked by accident on 32-bit builds but truncates locked-buffer addresses on LP64 hosts.
  - The active fix is to keep it as an integer offset in `SOUNDINT.H` and do buffer math in bytes explicitly inside `SOUNDIO.CPP` / `SOUNDINT.CPP`.
  - The failure mode is an ASan crash in `Play_Sample_Handle()` when the menu theme startup path zero-fills the unwritten tail of the first streamed buffer.
- The SDL gameplay mixer interprets 8-bit PCM as unsigned.
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` / `WIN32LIB/AUDIO/SOUNDINT.CPP` therefore must fill unwritten 8-bit buffer regions with `0x80` silence, not `0x00`.
  - Zero-filled tails/gaps inject a strong negative DC offset and are especially noticeable on streamed front-end music, though the same issue can affect ordinary sound effects too.
- The active front-end score files are not raw PCM blobs: traced `INTRO.AUD` startup playback reports `comp=99`, i.e. the menu music is going through the SOS ADPCM decode path in `WIN32LIB/AUDIO/SOUNDINT.CPP::Sample_Copy()`.
  - Each compressed gameplay frame there is `uint16_t fsize`, `uint16_t dsize`, `uint32_t magic(0xDEAF)`, then `fsize` bytes of compressed payload.
  - Leaving that frame marker as a native `long` on LP64 makes the decoder read 8 bytes instead of 4, which breaks the first streamed decode chunk and can show up as garbled or constantly restarting front-end music (and can affect gameplay sound effects that share the same path).
- The gameplay sound-tracker bookkeeping around streamed file playback also expects 32-bit byte counts.
  - `LockedData.MagicNumber`, `LockedData.StreamBufferSize`, and `SampleTrackerType::FilePendingSize` should stay 32-bit values, not LP64-native `long`, because the legacy code treats them as Win32-sized byte counters and frame constants.
- The SDL gameplay mixer must not trust the live `SDL_AudioSpec` fields after opening the device stream.
  - A traced `INTRO.AUD` menu-theme run showed `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP::MixInto(...)` being called with `output_rate = 0` when the backend reused `MixSpec.freq` from the `SDL_AudioSpec` struct after `SDL_OpenAudioDeviceStream(...)`.
  - That zero-rate path makes the resample step jump by the full source rate each output frame, which turns otherwise sane gameplay `.AUD` decode data into extremely loud/noisy garbage.
  - Keep cached mix rate/channel integers owned by the backend and pass those cached values into `MixInto(...)`; movie audio is unaffected because it uses the separate `VQ/VQA32/AUDIOCOMPAT.CPP` SDL stream.
- The gameplay SDL device stream should use the actual gameplay PCM source format, not a forced float source format.
  - `SDL_OpenAudioDeviceStream(...)` creates a stream from the caller-supplied `spec` into the device format, and its playback callback requests `additional_amount` / `total_amount` in **source-format bytes**.
  - The working in-tree reference is `VQ/VQA32/AUDIOCOMPAT.CPP`, which resolves movie audio to `SDL_AUDIO_U8` / `SDL_AUDIO_S16LE` from the asset bit depth before queuing raw PCM blocks.
  - For the gameplay backend, opening the stream as `SDL_AUDIO_F32` while the legacy engine still fundamentally owns PCM primary/secondary buffers was a bad seam.
  - The durable fix is to keep the internal accumulator float if convenient, but open the gameplay stream in the primary buffer's PCM format (`SDL_AUDIO_U8` / `SDL_AUDIO_S16LE`) and convert back to that PCM format before `SDL_PutAudioStreamData(...)`.
  - A traced `INTRO.AUD` menu-theme run after that change reported `OpenStream src=22050Hz/1ch/S16LE dst=44100Hz/2ch/S16LE`, and SDL disk-audio capture peaked at only `6837/32767`, which is sane.
- `RA_TRACE_STARTUP=1` now has a useful gameplay-audio discriminator in `Play_Sample_Handle prepared ...`.
  - If that trace shows `dest=8192 more=1 oneshot=0`, the first streamed quarter decoded correctly and the stream is ready to run.
  - If playback still fails after that, check focus gating next: `Start_Primary_Sound_Buffer(FALSE)` refuses to launch audio unless `GameInFocus` is true, so headless or focus-flapping runs can produce false theme-restart spam even when decode is already correct.

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
- The low-resolution `320x200 -> 640x400` movie scaler depends on `PaletteInterpolationTable` matching the exact palette that is sent to the screen.
  - `CODE/WINASM.CPP::Asm_Interpolate()` inserts every other pixel with `PaletteInterpolationTable[right][left]`, so stale or mismatched interpolation tables show up as colorful speckle over an otherwise recognizable frame rather than as block decode corruption.
  - `CODE/WINSTUB.CPP::SetPalette()` therefore still needs the legacy movie palette normalization sequence (`&= 63` plus `Increase_Palette_Luminance(..., 15, 15, 15, 63)`) before `Set_Palette(...)` and before any runtime interpolation-table rebuild that falls back to `CurrentPalette`.
  - A good decode-vs-presentation split test is to dump the raw `buffer` argument in `CODE/CONQUER.CPP::VQ_Call_Back()` for a known frame and reconstruct it with `CurrentPalette`; if that matches an external decode (for example `ffmpeg` on a loose `.VQA`), the failure is in interpolation/presentation rather than `UnVQ_*`.
  - The traced mission-failed `BMAP.VQA` regression on Linux was ultimately a filesystem-resolution issue, not a decoder issue:
    - the game asked `CCFileClass` for uppercase `BMAP.VQP`;
    - the loose override on disk was lowercase `GameData/bmap.vqp`;
    - the Win32 `CreateFile()` wrapper only did exact-case opens, so the loose file was skipped on case-sensitive filesystems and `CCFileClass` fell back to a different archived `BMAP.VQP`;
    - the wrong interpolation table then poisoned `Interpolate_2X_Scale()` and produced the recognizable-image-plus-speckle symptom.
  - `SDL3_COMPAT/wrappers/win32_compat.cpp::WWFS_NormalizePath()` now resolves existing paths case-insensitively on non-Windows hosts before opening them, which restores Windows-style loose-file override behavior at the lowest file-system layer where the rest of the port expects it.
  - A practical low-resolution movie smoke test on the current data set is: break on `Play_Intro()` under `gdb` and call `Try_Play_Movie("SIZZLE", THEME_NONE, true)`; this forces the buffered `320x200` movie path even when the old sequenced-debug intro route lands on unavailable assets such as `ANTINTRO.VQA`.

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
- The supported build no longer forces `WIN32` / `_WIN32` globally. Include SDL headers normally, and include `win32_compat.h` directly in files that still need legacy Win32 typedefs or APIs. Do not reintroduce temporary `#undef WIN32` / `#define WIN32` shims around SDL includes.

## Score screen rendering notes

- The mission-complete score screen's animated ornaments are a score-screen-local redraw seam, not a general renderer bug.
- `ScoreTimeClass` and `ScoreCredsClass` in `CODE/SCORE.CPP` advance `TIMEHR.SHP`, `HISC1-HR.SHP`, `HISC2-HR.SHP`, and `CREDS*HR.SHP` frames over mostly static background art.
- On the active SDL/Linux path, those objects must capture the current pixels under their shape bounds when they are created and restore that patch before each new frame draw.
  - If they only draw the next frame on top of the previous one, transparent/high-color ornament frames leave stale pixels behind and the mission-complete screen shows a few localized graphical glitches even though the rest of the render path is fine.
- Keep this fix in `CODE/SCORE.*` unless there is stronger evidence of a shared renderer regression.
  - The tempting `GraphicBufferClass::Offset` / LP64 theory is not the right seam for this Linux report: on the active host ABI, `long` is already 64-bit, so that field does not explain a score-screen-only artifact.

## Linkage and header pitfalls

- The VQM32 headers rely on legacy memory-model and calling-convention keywords such as `far`, `near`, `huge`, `interrupt`, and `cdecl`. The safest fix is to define those macros locally in the VQM32 headers that need them.
- Do **not** pull the full `win32_compat.h` typedef layer into the VQM32/HMI headers just to get `cdecl` or `far`; that causes type collisions with old SOS definitions like `WORD`, `DWORD`, `HANDLE`, `LPSTR`, and `VOID`.
- `VQ/INCLUDE/VQM32/PROFILE.H` declares `GetINIInt()` and `GetINIString()` with normal C++ linkage, so the compatibility implementations in `VQ/VQA32/VQCOMPAT.CPP` must also use normal C++ linkage.
- `VQ/INCLUDE/VQM32/VIDEO.H` declares `TestVBIBit()` and `GetVBIBit()` with normal C++ linkage; only `WaitVB()` / `WaitNoVB()` use C linkage there.
- `VQ/INCLUDE/VQM32/FONT.H` needs `Char_Pixel_Width()` declared with normal C++ linkage so it matches the existing `win32lib` font implementation.
- `VQ/INCLUDE/VQM32/COMPRESS.H` also needs the legacy `cdecl` macro definitions if it is included outside the usual VQA private headers.

## Broader wrapper surface still in play

- The empty forwarding headers `SDL3_COMPAT/wrappers/{mem.h,modem.h,new.h,windows.h,WINDOWS.H,windowsx.h}` are gone.
  - For the SDL/Linux C/C++ compat path, include `win32_compat.h`, `<string.h>`, or `<new>` directly instead of recreating forwarding shims.
  - Genuine Windows SDK `windows.h` includes should stay confined to legacy Windows-only C/resource artifacts, not the active portable compat path.
- The codebase still leans on compatibility wrappers for:
  - file I/O (`CreateFile`, `ReadFile`, `WriteFile`, `CloseHandle`);
  - threading and timing (`Sleep`, generic thread helpers, thread priority helpers);
  - registry access (`RegOpenKeyEx`, `RegQueryValueEx`, `RegCloseKey`);
  - directory enumeration (`FindFirstFile`, `FindNextFile`, `FindClose`);
  - DirectDraw compatibility types used by legacy rendering code.
- The active gameplay audio path no longer depends on the DirectSound compatibility wrapper; any remaining DirectSound-shaped code is archival or inactive.

## Startup/runtime porting notes

- The Linux build no longer forces `WIN32=1` / `_WIN32=1`, but the runtime still expects Win32-era behavior from the remaining compatibility wrappers. Preserve those semantics directly instead of reviving dual-branch `#ifdef WIN32` code.
- For file enumeration and similar wrappers, prefer SDL filesystem/storage helpers in startup-critical compat paths instead of `std::filesystem` or direct POSIX filesystem APIs.
- `SDL3_COMPAT/wrappers/win32_compat.cpp` virtual-CD probing (`virtual_cd_index_for_drive_letter()` / `GetDriveType()`) is part of static startup through `GetCDClass`. Avoid `std::filesystem::path` composition there on Linux; it can still trip early-startup allocator/path issues before the game reaches normal runtime.
- Linux must enter the real `WinMain(...)` startup path. If the Unix stub `main()` is left in place, the program only prints the old placeholder message (`Run C&C.COM.`) instead of running the game.
- `GetModuleFileName()` must return a real executable path on Linux because `CODE/STARTUP.CPP` uses it during startup.
- `CODE/STARTUP.CPP` changes the current working directory to the executable directory. The build now copies the runnable binaries into `GameData/` automatically:
  - normal builds refresh `GameData/redalert`;
  - ASan builds refresh `GameData/redalert-asan`.
  Smoke tests should launch those `GameData/` copies so the executable directory still contains the real assets.
- The old `_dos_getdiskfree()` bridge is removed. Keep free-space checks on the supported path in direct 64-bit byte math at the call site; do not revive DOS cluster-count structs/functions just to answer startup/save disk-space queries.
- The DDE compatibility layer cannot keep startup-critical mutable state in ordinary file-scope globals. Game-side global constructors may call into DDE before those globals are safely initialized.
- SDL/Wayland startup can deliver focus gained and focus lost in the same early pump cycle. The bootstrap path needs a sticky "focus seen once" latch instead of waiting only on the live `GameInFocus` bit.
- On the SDL3 port, that first activation is now also the last time ordinary OS focus changes should touch the legacy pause/resume path.
  - `CODE/SDLINPUT.CPP::handle_focus_event()` should still clear held key/button state on focus loss, but after bootstrap it should not keep posting focus-loss events into the old Win32 suspension logic.
  - `CODE/WINSTUB.CPP::WM_ACTIVATEAPP` should treat post-bootstrap loss as informational only; otherwise `Theme.Suspend()`, `Stop_Primary_Sound_Buffer()`, and the `GameInFocus` render gate recreate the exact black-screen/video-stop/theme-restart bug the SDL3 port is trying to remove.

## Mouse/input notes

- The active keyboard queue path in this build is `CODE/KEY.CPP`, not `CODE/KEYBOARD.CPP`.
- The active SDL input architecture is now:
  - `CODE/SDLINPUT.CPP` / `CODE/SDLINPUT.H` own SDL event pumping/waiting, track key/button/toggle state, keep async low-bit latches, and maintain the shared mouse position snapshot;
  - `CODE/CONQUER.CPP::Call_Back()` pumps SDL every front-end loop iteration and now also runs `WWMouse->Process_Mouse()` on that same main thread;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp::next_message()` only drains posted lifecycle/focus/quit messages and uses `SDL_GameInput_Wait()` when the legacy message loop blocks;
  - `WWKeyboardClass` still owns the legacy circular key/mouse queue, but SDL key/button events are now fed into it directly instead of being translated into fake `WM_KEY*` / `WM_MOUSE*` messages first.
- Main-menu hover logic and click logic still use different legacy consumers, but they now read the same SDL-backed mouse state:
  - hover/selection tracking uses `Get_Mouse_X()` / `Get_Mouse_Y()` from `WIN32LIB/KEYBOARD/MOUSE.CPP`, which poll `GetCursorPos()`;
  - click selection uses `Keyboard->MouseQX` / `Keyboard->MouseQY` from `CODE/KEY.CPP`;
  - `SDL_GameInput_Handle_Mouse_Motion(...)` updates both the shared cursor position and `Keyboard->MouseQX` / `MouseQY`, so the software cursor and menu hit-testing stay synchronized.
- The software cursor in `WIN32LIB/KEYBOARD/MOUSE.CPP` must stay on the main thread on the SDL3 port:
  - the old WinMM `timeSetEvent()` cursor callback could call `GraphicBufferClass::Unlock()`, which in the SDL DirectDraw wrapper presents the frame through SDL/OpenGL;
  - that makes the old timer path unsafe on Wayland/OpenGL because it drives SDL rendering from a background thread;
  - the active path now ticks `WWMouse->Process_Mouse()` from `CODE/CONQUER.CPP::Call_Back()` instead.
- `CODE/WINSTUB.CPP::Windows_Procedure()` no longer forwards `WM_*` input messages into `Keyboard->Message_Handler()`, and `CODE/KEY.CPP` no longer carries that old Win32 input translation path.
  - focus and close still flow through `WM_ACTIVATEAPP` / `WM_CLOSE`, but post-bootstrap `WM_ACTIVATEAPP(active=0)` is intentionally ignored so ordinary unfocus does not pause the game;
  - key and mouse delivery now stay on the SDL-backed path end-to-end, which removes the old split between queued menu input and polled software-cursor motion.
- `GetKeyState()` / `GetAsyncKeyState()` in the SDL compat layer must report ordinary keys and mouse buttons, not just modifiers.
  - `CODE/KEY.CPP::Down()` is just `GetAsyncKeyState(key & 0xFF) != 0`, so returning `0` for non-modifier keys makes the UI look frozen even when the menu loop is still running.
  - Mouse buttons must also drive the shared virtual-key down counts, not just the raw `g_mouse_buttons` mask.
  - `CODE/GADGET.CPP::Input()` synthesizes `LEFTHELD` / `RIGHTHELD` from `Keyboard->Down(KN_LMOUSE)` / `Keyboard->Down(KN_RMOUSE)`, so if `CODE/SDLINPUT.CPP::SDL_GameInput_Handle_Mouse_Button()` only records the click latch and never calls `press_virtual_key_locked()` / `release_virtual_key_locked()`, mission-map drag behavior breaks even though ordinary click/release events still arrive.
  - The concrete symptom is that `DisplayClass::Mouse_Left_Held()` never runs, `Map.IsRubberBand` never becomes true, and no drag-select rectangle appears while the button is held.
- `CODE/MAPSEL.CPP::Map_Selection()` can look “hung” even when the intro loop has already finished.
  - A focused live probe on the active SDL/Linux build showed the map-select intro does reach its end (`frame == Get_Animation_Frame_Count(anim) == 70`) and the mouse hidden-count is already clear (`Get_Mouse_State() == 0`) immediately after `Show_Mouse()`.
  - The visible stall came from `WIN32LIB/WSA/WSA.CPP`, not from the mouse/input path: the resident `MSAA.WSA` offset table goes bad mid-animation, so `Apply_Delta()` starts rejecting later frames with impossible resident `frame_offset` / `frame_data_size` values around frame `34`.
  - Because the reveal loop still increments `frame`, the function eventually reaches its input loop while leaving the screen on the last good partial reveal, which makes the map-select screen look frozen even though it is still running.
  - The practical fix is to stop trusting the legacy `largest_frame_size` field on its own: reserve at least one full frame of XOR-delta workspace (along with the larger compressed-frame span that still has to be back-loaded into that same buffer) before placing the resident file data behind it, and keep a resident-offset sanity check as a fallback guard.
- `CODE/MSGBOX.CPP::WWMessageBox::Process()` must keep its `retval` as an `int`, not a `bool`.
  - The chooser really returns `0`, `1`, or `2` for button 1/2/3.
  - On the active tree, a real click in the centered Soviet button of `WWMessageBox().Process(TXT_CHOOSE, TXT_ALLIES, TXT_CANCEL, TXT_SOVIET)` arrived as `BUTTON_3|BUTTON_FLAG` and set `selection = BUTTON_3`, but the final `retval = 2` was truncated to `true`, so callers received `1`.
  - In `CODE/INIT.CPP::Select_Game()` under `FIXIT_VERSION_3`, that made the Soviet choice look identical to Cancel, which matched the observed symptom: the game fell back to the main menu and only restarted the front-end `INTRO.AUD` theme instead of reaching `Start_Scenario("SCU01EA.INI")`.
- The SDL3 port now lets the main game window resize independently from the logical game framebuffer.
  - The shared letterbox/pillarbox math lives in `RA_GetPresentationRect()`, `RA_WindowToGamePoint()`, and `RA_GameRectToWindowRect()` in `SDL3_COMPAT/wrappers/win32_compat.cpp`.
  - `SDL3_COMPAT/wrappers/ddraw_compat.cpp` must render the primary surface into that computed presentation rectangle and clear the rest of the window to black; otherwise resize stretches or smears the indexed framebuffer.
  - `CODE/SDLINPUT.CPP` and `ClipCursor()` must convert mouse positions and clip rectangles through the same helpers, or menu hit-testing / software-cursor confinement drift away from the displayed image after a non-4:3 resize.
- The active mouse space is not always the same as the primary-surface space.
  - `CODE/STARTUP.CPP` often leaves the primary surface at `640x480` but attaches `SeenBuff` and `HidPage` as `640x400` viewports, usually at `(0,40)`.
  - `RA_WindowToGamePoint()` and `RA_GameRectToWindowRect()` work in primary-surface coordinates, so the SDL input layer must subtract `SeenBuff.Get_XPos()/Get_YPos()` after `RA_WindowToGamePoint()`, while `ClipCursor()` must add that origin back before calling `SDL_SetWindowMouseRect()`.
  - If that viewport offset is ignored, the software cursor and SDL confinement both skew toward the top-left corner after resize because SDL is clipping against the wrong logical rectangle.
- On Wayland compositors such as Hyprland, `SDL_SetWindowMouseRect()` is not just a local cursor helper.
  - SDL maps that API to Wayland pointer-confinement behavior, which prevents compositor window-management gestures like `Meta+left-drag` move and `Meta+right-drag` resize from stealing the real pointer while the game window is clipped.
  - `SDL3_COMPAT/wrappers/win32_compat.cpp::ClipCursor()` therefore must always keep the game-side `SDL_GameInput_SetCursorClip()` state, but skip SDL window confinement and the follow-up `SDL_WarpMouseInWindow()` clamp for windowed Wayland windows.
  - Non-Wayland and fullscreen paths can continue using the SDL mouse rectangle for OS-level confinement.

## General runtime correctness notes

- `CODE/SESSION.CPP::Read_MultiPlayer_Settings()` allocates `InitStrings` entries with `new char[INITSTRBUF_MAX]`.
  - `SessionClass::Free_Scenario_Descriptions()` therefore must free those entries with `delete[]`, not `delete`.
  - ASan reports this as an `alloc-dealloc-mismatch` during shutdown after a normal startup/menu run.
- `ListClass` stores caller-provided string pointers verbatim; it does not take ownership through a copying layer.
  - Callers that populate list-box text with `new char[]` buffers (for example `SoundControlsClass::Process()` in `CODE/SOUNDDLG.CPP`) must preserve a `char *` / `char const *` pointer type all the way to `delete[]`.
  - Deleting those buffers through `void *` reproduces an `alloc-dealloc-mismatch (operator new [] vs operator delete)` under ASan when the dialog tears down its item list.
- Some legacy loops also use `GetAsyncKeyState(vk) & 1`, so the compat layer needs a one-shot low-bit latch for recent press transitions as well as current high-bit state.
- Toggle keys must also keep real Win32 low-bit semantics:
  - `CODE/KEY.CPP` now probes `GetKeyState(VK_CAPITAL/VK_NUMLOCK) & 0x0001` when building queued key codes;
  - if the SDL input state layer or compat wrappers return any fake higher bit there, ordinary keys like `ESC` pick up `WWKEY_SHIFT_BIT` and exact comparisons against `KN_ESC` fail in the intro callback.
- The intro breakout path on this build is now two-stage:
  - `CODE/CONQUER.CPP::VQ_Call_Back()` sets `Brokeout = true` and returns non-zero when it sees exact `KN_ESC`;
  - `VQ/VQA32/TASK.CPP` then has to convert the resulting `VQAERR_EOF` into movie shutdown so `VQA_StopAudio()`, `VQA_Play()` return, and startup continues to the title/menu path.
- `GetCursorPos()`, `Get_Global_Mouse_X()`, and `Get_Global_Mouse_Y()` must all read the same shared SDL input snapshot.
  - reading raw `SDL_GetMouseState()` directly from another thread leaves cursor redraw dependent on whatever SDL happened to pump on the main thread;
  - the current SDL input layer fixes that by letting the main thread own event pumping while other callers read the last synchronized cursor position.
- `Init_Mouse()` leaves the software cursor hidden again before front-end flow continues, so menu entry points that need pointer interaction must drive the Westwood cursor hide count back to visible state explicitly and restore the prior hide state when they exit.
- The menu cursor regression on LP64 was not just a hide-count problem:
  - the active `Shape_Type` file header must stay packed to the original 26-byte on-disk layout in `WIN32LIB/SHAPE/SHAPE.H` / `WIN32LIB/INCLUDE/SHAPE.H`;
  - if that header grows to 28 bytes, `Get_Shape_Width()` / `Get_Shape_Original_Height()` read the wrong offsets from raw `.SHP` data, so a real `30x24` mouse frame is misread as `6144x109`, `ASM_Set_Mouse_Cursor()` rejects it against the `48x48` cursor buffer, and the software cursor becomes invisible even though menu code has already unhidden it;
  - once the shape header is packed again, `CODE/MENUS.CPP` can reliably restore `MOUSE_NORMAL` and composite `WWMouse->Draw_Mouse(&HidPage)` into the title/menu blit to keep the front-end cursor visible on the SDL/Win32 path.
- The latest focused `gdb` validation for the SDL input rewrite was:
  - inject `SDL_GameInput_Handle_Key(0x1B, 41, 0, 0)` from the first `VQ_Call_Back()` hit and confirm startup still reaches `Main_Menu`;
  - push a real `SDL_EVENT_MOUSE_MOTION` with `SDL_PushEvent()` at `Main_Menu`, let the next `Call_Back()` run, and confirm both `Keyboard->MouseQX/MouseQY` and `GetCursorPos()` report `320,200`.
- A focused debug-mission probe against `SCG01EA.INI` is a fast way to validate tactical SDL mouse behavior end-to-end.
  - With the fixed `SDL_GameInput_Handle_Mouse_Button()` path, a synthetic SDL press+drag now shows `Keyboard->Down(KN_LMOUSE)=1` after press and `Map.IsRubberBand=1` after motion.
  - The same full SDL window-coordinate path still selects a player unit and queues an attack click (`OutList.Count` `0 -> 1`), so if enemy orders later fail in live gameplay the first place to look is not `TechnoClass::What_Action()` or `FootClass::Active_Click_With()`.
  - The same probe path is also useful for shared movement/path corruption checks:
    - `((UnitClass*)Units.Raw_Ptr(0))` and `((UnitClass*)Units.Raw_Ptr(1))` are still a good direct attack-order pair in `SCG01EA.INI`;
    - calling `Override_Mission(MISSION_ATTACK, enemy->As_Target(), 0)` there reproduces the real tactical path build without depending on manual clicks.
- The active Win32/SDL shape path has an extra cached-shape wrapper when `UseBigShapeBuffer` is enabled.
  - `Build_Frame()` can return a pointer to a cached `ShapeHeaderType` wrapper plus reserved line-header scratch, not the raw pixel buffer itself.
  - Code that wants raw frame pixels (for example `Buffer_Frame_To_Page()`) must call `Get_Shape_Header_Data()` first.
  - `CODE/CONQUER.CPP::CC_Draw_Shape()` must do this on its ordinary Win32 path; otherwise dialog background assets such as `DD-BKGND.SHP` appear missing because the blitter reads wrapper/header bytes instead of image pixels.
- `GraphicViewPortClass::Pitch` is not a full row stride in the active Win32/SDL path.
  - `Get_Pitch()` returns the end-of-line skip/modulo, so manual pixel walkers must use `Get_Width() + Get_XAdd() + Get_Pitch()` to move between rows.
  - Using `Get_Pitch()` by itself only works by accident on full-width buffers and breaks clipped viewports like `WINDOW_PARTIAL`, where the row advance can become `0` even though the underlying surface stride is still `640`.
  - `CODE/KEYFBUFF.CPP::Buffer_Frame_To_Page()` needs that full-stride calculation for both normal row writes and predator sampling; otherwise dialog/window shapes collapse into one scanline and appear missing.

## 64-bit ABI pitfalls found during runtime debugging

- `GameInFocus` must stay a Win32-style `BOOL`, not `bool`. Several legacy translation units still declare it as `extern BOOL GameInFocus`, and shrinking it to 1 byte causes out-of-bounds reads on 64-bit builds.
- `strtrim()` in `CODE/READLINE.CPP` must use `memmove()` for the left-trim step because the source and destination ranges overlap.
- CRC accumulation code that historically relied on 32-bit signed wraparound should use explicit `uint32_t` intermediates instead. This preserves the original modulo-2^32 behavior without triggering UB on modern compilers/sanitizers.
- Typed list nodes must be deleted through their concrete type. Deleting derived list nodes through `GenericNode*` triggers `new-delete-type-mismatch` under ASan.
- Enum post-increment helpers used by ordinary `for (x = FIRST; x < COUNT; x++)` loops must advance to the `COUNT` sentinel, not wrap back to `FIRST`, or startup/UI one-time initialization can spin forever.
- `PathType::Length` and `Find_Path(..., maxlen, ...)` are facing-count values, not byte counts.
  - `CODE/FOOT.CPP::Basic_Path()` must pass `Find_Path()` the number of `FacingType` slots in its staging buffer, not `sizeof(workpath)` bytes.
  - Any copy of `FacingType` path commands must multiply that count by `sizeof(FacingType)` before calling `memcpy()` / `Mem_Copy()`.
  - On the active Linux/ASan build, treating those counts as raw bytes partially overwrites 32-bit enum slots and produces values like `0xFFFF0007` (`-65529`), which then show up later as invalid `FacingType` / `DirType` values and out-of-bounds `Facing32[]` lookups in `Dir_To_32()`.
- On Linux/glibc, host-endian tests must not use `#ifdef BIG_ENDIAN`.
  - glibc headers define `BIG_ENDIAN` as an integer constant even on little-endian hosts, so `#ifdef BIG_ENDIAN` fires on x86-64 Linux and selects the wrong packed/union layout.
  - The active port now centralizes this in `CODE/ENDIAN.H` via `RA_BIG_ENDIAN_HOST`, and low-level packed helpers must use `#if RA_BIG_ENDIAN_HOST` instead of raw `#ifdef BIG_ENDIAN`.
  - This directly affected `CODE/FIXED.H`: constructor-based fixed-point defaults such as `fixed(1)` were landing as raw `0x0001` (`1/256`) instead of raw `0x0100` (`1.0`).
  - Practical movement symptom: `Rule.GameSpeedBias`, `FootClass::SpeedBias`, and derived `HouseClass::GroundspeedBias` collapsed to `1/256`, `MPHType maxspeed` rounded down to `0`, infantry entered `DO_WALK`, `IsDriving` stayed true, and `HeadToCoord`/`Path[0]` were valid, but `Coord` never changed.
- Once the fixed-point/endian bug is corrected, `CODE/INFANTRY.CPP::Movement_AI()` exposes a second live-path issue at path-step completion.
  - The old `memcpy(&Path[0], &Path[1], ...)` overlaps source and destination by one `FacingType` entry.
  - ASan reports this as `memcpy-param-overlap` at `CODE/INFANTRY.CPP:3965` on the real infantry attack movement path.
  - The correct helper there is `Mem_Copy()` (backed by `memmove()`), not `memcpy()`.
- A useful end-to-end revalidation script for this regression in `SCG01EA.INI` is:
  - force `Debug_Map = true` at `Select_Game()`;
  - use player-order dispatch (`Player_Assign_Mission(...)`) rather than direct `Override_Mission(...)` calls when checking queued order transitions;
  - for the first player-owned infantry (`Infantry.Raw_Ptr(13)` on the active probe), confirm that attack orders now produce nonzero walk-step values (`maxspeed=10`, `dist=10`) and steadily changing `Coord`, then queue a later `MISSION_MOVE` to `::As_Target(cell)` and confirm the mission changes from `1` to `2` without reviving the old `Dir_To_32()` crash.
- `ReadyToQuit` must not be a `bool` on the active Win32/SDL path.
  - `CODE/STARTUP.CPP` and `CODE/WINSTUB.CPP` use it as a multi-state shutdown latch: `0` = forced shutdown, `1` = waiting for graceful `WM_DESTROY`, `2` = graceful shutdown complete, `3` = emergency shutdown path.
  - If it is declared as `bool`, assignments like `ReadyToQuit = 2` collapse back to `true`, and the `while (ReadyToQuit == 1)` cleanup loops never finish even though `WM_DESTROY` already ran.

## Packing/alignment notes

- Old `#pragma pack` state leaks easily through the audio headers; keep those headers balanced with `push` / `pop`.
- The active SDL audio backend still has to preserve the DirectSound-era buffer contract closely enough for legacy sound code to populate formats, lock regions, and play/status flags safely.
- Some translation units include `windows.h` under `#pragma pack(4)`. The compatibility `CRITICAL_SECTION` therefore needs explicit pointer alignment (`alignas(void*)`) so its internal mutex pointer is not placed at 4-byte-aligned addresses on 64-bit Linux.
- The runtime sound bookkeeping structs (`SampleTrackerType`, `LockedDataType`) need native alignment for pointer-bearing members and `CRITICAL_SECTION`. Keeping them under forced 4-byte packing produces sanitizer-reported misaligned accesses during `Audio_Init()`.
- `SOSDATA.H` / `SOSFNCT.H` are declaration-only headers; any `#pragma pack` there is just state pollution. Leave them unpacked and let the real struct-owning headers manage their own layout.
- The VQA/VQ file headers (`VQAHeader`, `VQHeader`) are already naturally laid out with their current fixed-width field ordering; prefer size assertions to redundant header-wide packing there.
- In the old VQM32/WINVQ mix headers, only the on-disk `MIXHeader` / `MIXSubBlock` should stay packed. The in-memory `MIXHandle` carries a host pointer and should not live under the packed region.
- When an old translation unit still wants a temporary non-default pack (for example the legacy audio `.CPP` files that include Win32/HMI-era headers under `pack(4)`), scope that with `push` / `pop` around the full file or include region instead of relying on `pack()` resets.
- Asset/file formats that were historically read through raw pointer casts now need fixed-width structs or `memcpy`-based header loads in active Linux/ASan paths. Unaligned font, shape, IFF/CPS, and keyframe reads were all real runtime issues in this port.
- The same LP64 rule applies to gameplay audio sample headers: `.AUD` headers must use fixed-width 32-bit size fields instead of legacy `long`, or the loader silently reads the wrong header size on Linux/x86-64.
- The same LP64 rule also applies to pointer-shaped bookkeeping in the old audio code: fields that really hold byte offsets must be converted to integer offsets before the SDL/Linux port can safely reuse the legacy refill logic on 64-bit hosts.

## DirectDraw/SDL presentation notes

- The software renderer can draw correctly and still show an all-black SDL window if the DirectDraw compatibility layer never presents the updated primary surface.
- In the current compat layer, primary-surface presentation must happen after:
  - `IDirectDrawSurface::Unlock()` on the primary surface;
  - `IDirectDrawSurface::Blt()` when the destination is the primary surface;
  - `IDirectDrawSurface::SetPalette()` when a primary surface palette is attached or changed.
- For front-end overlays, those primary-surface updates must be coalesced before hitting SDL.
  - Many menus and dialogs draw directly to `SeenBuff`, and the primitive helpers (`Print`, `Fill_Rect`, `Put_Pixel`, etc.) lock/unlock the destination repeatedly.
  - If every primary-surface unlock/blit triggers an immediate SDL present, overlay redraw becomes visibly incremental and buttons repaint one by one.
  - Queue the present in the DirectDraw wrapper, flush once per callback tick, and use an explicit batch around `GadgetClass::Draw_All()` so the whole overlay lands in one SDL frame.
- VQA movie playback needs an explicit present flush on the Win32/SDL path.
  - `CODE/CONQUER.CPP::VQ_Call_Back()` blits each decoded movie frame to `SeenBuff`, but unlike many front-end loops it does not call `Call_Back()` afterward.
  - After switching the DirectDraw wrapper to queued presents, movie frames therefore need `DirectDraw_Flush_Present()` directly from the callback or the SDL window stays black while the decoded frame buffer keeps updating underneath.
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

## Watcom enum size differences

The original Red Alert was built with Watcom C, which sizes enums to the smallest type that fits their value range. Most game enums (e.g., `HousesType` with range -1..21) were stored as **1-byte signed values**. GCC/Clang default to 4-byte enums.

This matters for any code that uses unions containing both an enum field and a wider integer field (like `int32_t Value`). Writing to the 1-byte enum member only modifies the low byte of the union; the rest retains whatever was there before. The scenario data in the original MIX files was written with this 1-byte assumption, so 4-byte reads of `Data.Value` contain garbage in the upper bytes (typically `0xFF` from constructor initialization with `Data.Value = -1`).

**Pattern**: A value like `-255` (`0xFFFFFF01`) in trigger INI data means "enum value 1 stored in the low byte, with 0xFF garbage in upper bytes". Sign-extend from byte (`(int32_t)(int8_t)(value & 0xFF)`) to recover the correct value.

**Known affected types** (1-byte in Watcom, 4-byte in GCC):
- `HousesType` (-1..21)
- `ThemeType` (-3..41)
- `SpecialWeaponType` (-1..7)
- `QuarryType` (0..10)
- `VQType` (-1..103)
- `VoxType` (-1..118)

The current traced reminder that this still matters is `CODE/LOADDLG.CPP:683`, where the ASan startup smoke reports an invalid `HousesType` load while the front-end/load-dialog path is active. Treat save/load UI metadata reads as another likely enum-width seam.

**Types that are 2+ bytes even in Watcom** (range exceeds signed byte):
- `VocType` (-1..167) — likely 2-byte short in Watcom
