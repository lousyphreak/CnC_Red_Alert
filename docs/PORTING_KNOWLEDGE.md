# Porting Knowledge

## Formation/audio sanitizer pitfalls

- `WIN32LIB/AUDIO/SOUNDIO.CPP::Play_Sample_Handle(...)` must validate the requested sample handle before indexing `LockedData.SampleTracker[id]`.
  - `Get_Free_Sample_Handle(...)` legitimately returns `-1` when no slot is available.
  - Practical symptom if the validation comes after `SampleTracker[id]`: UBSan/ASan reports an immediate `SampleTracker[-1]` out-of-bounds access even though the function later checks for `-1`.
  - Safe rule: reject any handle where `(uint32_t)id >= MAX_SFX` before taking the tracker pointer, so both `-1` and oversized values fail cleanly through the existing `-1` return path.
- `CODE/TEAM.CPP::TeamClass::TMission_Formation()` cannot use the 10-entry `TeamSpeed` / `TeamMaxSpeed` globals as scratch storage for AI teams.
  - Player control groups live in `FootClass::Group` values `0..9`, which is why the shared globals are only 10 entries wide and why UI code like `TECHNO.CPP` only renders group numbers for `< 10`.
  - AI formation groups intentionally use `ID + 10`, so a team with `ID == 0` already writes index `10` if `TMission_Formation()` uses those globals directly.
  - Safe rule: keep AI formation speed/max-speed calculation local in `TMission_Formation()` and copy the result straight into each member's `FormationSpeed` / `FormationMaxSpeed` fields instead of widening unrelated global state or save/recording surfaces.

## Mission-pack visibility is gated by compat-layer install detection

- The unified executable path for Counterstrike and Aftermath is already compiled in this tree.
  - `CODE/DEFINES.H` already enables both `FIXIT_CSII` and `FIXIT_VERSION_3`, so the expansion-aware menu flow, scenario filtering, rules loading, and packet loading are live without any extra CMake define.
  - `CODE/MENUS.CPP`, `CODE/EXPAND.CPP`, `CODE/SESSION.CPP`, and `CODE/INIT.CPP` all route expansion visibility through `Is_Counterstrike_Installed()` / `Is_Aftermath_Installed()`, not through a build-system option.
- On the SDL3/Linux port, the practical gate was the fake registry implementation in `SDL3_COMPAT/wrappers/win32_compat.cpp`.
  - `RegQueryValueEx(..., "CStrikeInstalled", ...)` and `RegQueryValueEx(..., "AftermathInstalled", ...)` were hardcoded to return `0`, so the front end behaved as if neither pack was installed even when `EXPAND.MIX` and `EXPAND2.MIX` were present in `GameData/`.
  - Practical symptoms: main-menu expansion entries disappear, `AFTRMATH.INI` never loads, and `CSTRIKE.PKT` / `AFTMATH.PKT` scenario lists stay filtered out by `Is_*_Installed()` checks.
- Safe rule for this port: make the compat-layer install flags reflect real expansion data presence.
  - Current fix policy: report `CStrikeInstalled=1` when `EXPAND.MIX` exists and `AftermathInstalled=1` when `EXPAND2.MIX` exists, using the low-level SDL filesystem wrapper (`WWFS_GetPathInfo(...)`).
  - This preserves the original runtime gating model while making it work correctly on a case-sensitive, registry-free SDL3 build.
- One startup trap becomes visible immediately once Aftermath is detected correctly.
  - `CODE/INIT.CPP` used `Rule.Process(AftermathINI)` during boot, but `RulesClass::Process(...)` always runs `Heap_Maximums(...)`, which clears and rebuilds global type heaps such as `Weapons`.
  - By that point the first `RULES.INI` pass has already left live pointers inside type objects (`PrimaryWeapon`, `SecondaryWeapon`, etc.), so the second full `Process(...)` creates a use-after-free during object rule loading.
  - The safe startup path is the one already used in `CODE/SCENARIO.CPP` and `CODE/SAVELOAD.CPP`: overlay the individual rule sections from `AftermathINI` (`General`, `Recharge`, `AI`, `Powerups`, `Land_Types`, `Themes`, `IQ`, `Objects`, `Difficulty`) and do **not** call `Heap_Maximums(...)` again.

## EVA speech state must track the actual speech buffers

- The EVA voice path in `CODE/AUDIO.CPP` uses two reusable sample buffers: `SpeechBuffer[2]`.
  - `Speak_AI()` starts playback from one concrete buffer (`SpeechBuffer[_index]`), and `Is_Sample_Playing(...)` / `Stop_Sample_Playing(...)` operate on the exact sample pointer that was queued into the audio system.
  - Practical rule: never pass the `SpeechBuffer` array object itself to those helpers; iterate `SpeechBuffer[index]` and operate on each live buffer individually.
- Practical symptom when this goes wrong:
  - `Stop_Speaking()` can fail to stop the currently playing EVA sample even though callers think they forcibly cleared speech;
  - `Is_Speaking()` can return false immediately after `Speak(...)` starts playback, because it is checking the wrong pointer and therefore misses the live sample;
  - scenario/menu transition code then races ahead while EVA is still active, which can leak or glitch mission-exit speech such as `"battle control terminated"` into the front end.
- Safe fix pattern:
  - keep a tiny helper that answers "is any speech buffer playing?" by looping over `SpeechBuffer[index]`;
  - make `Stop_Speaking()` clear `SpeakQueue`, reset `CurrentVoice`, and stop every loaded speech buffer explicitly.

## Movie audio volume routing

- The active VQA movie playback path does **not** automatically inherit either gameplay-audio slider.
  - `CODE/INIT.CPP::Anim_Init()` seeds `AnimControl` from `VQA_DefaultConfig(...)`, and the VQA defaults still set `VQAConfig::Volume` to full scale (`0x00FF`).
  - The SDL3 VQA audio backend in `WIN32LIB/VQA32/AUDIOCOMPAT.CPP` already applies `config->Volume` directly as a 0..255 gain scalar, so if movies sound too loud the fix belongs at the `AnimControl` setup site, not in the backend mixer.
- Practical rule for this tree: when launching a movie from `CODE/CONQUER.CPP::Try_Play_Movie(...)`, explicitly assign `AnimControl.Volume` from game options before `VQA_Open(...)`.
  - Current policy: use the louder of `Options.Volume` (sound effects) and `Options.ScoreVolume` (music), since the UI exposes no separate movie slider and that preserves audibility without adding a new setting.

## Fog-of-war shadow-table regression after fixed-width cleanup

- The shroud/fog soft edge depends on the generated `ShadowTrans` remap table, not just on the final SDL presentation path.
  - A direct current-vs-old table dump is practical with a small standalone harness linked against the existing game objects and run from the real `GameData/` directory, because it can call the real mix/key/bootstrap palette code without needing the full game loop to reach tactical mode.
  - In this regression, current `HEAD` and `9ee4c9e` produced the same collapsed `ShadowTrans` rows, while `46e5b76` produced varied remap rows that feather the shroud edge. That proved the bug was in shadow-table generation rather than in `DisplayClass::Redraw_Shadow()` or SDL output/filtering.
- When converting legacy search/comparison code to fixed-width integers, the initializer must be converted with it.
  - `WIN32LIB/MISC/LEGACYCOMPAT.CPP::Build_Fading_Table(...)` originally used `long best_value = LONG_MAX;`.
  - Changing only the variable type to `int32_t` but leaving `LONG_MAX` on LP64 Linux narrows the initializer to `-1`.
  - Practical symptom: `match_value <= best_value` never succeeds for ordinary non-perfect matches, so the remap row stays stuck on its default destination color. For the shroud path that default is black, which turns the previously feathered edge into a hard dark edge.
  - Safe rule: if a working variable becomes `int32_t`, seed it with `INT32_MAX` (or equivalent fixed-width limit), not `LONG_MAX`.

## CODE runtime LP64 regressions after broad type sweeps

- Startup/movie/shapecache validation needs to continue past "it builds" after any large CODE-wide type cleanup.
  - One LP64 regression survived the compile pass in `CODE/CONQUER.CPP::MixFileHandler(...)`: `VQAHandle::VQAio` is an opaque IO cookie and the live VQA headers already model it as `uintptr_t`, but the `VQACMD_OPEN` path still stored the `CCFileClass *` through a `uint32_t` cast. Practical symptom: the game crashes immediately during the first intro movie (`ENGLISH.VQA`) on the first `VQACMD_READ`.
  - Another LP64 regression hid behind the first one in the shape-cache path: `CODE/2KEYFRAM.CPP::Build_Frame(...)` semantically returns a pointer to built shape data, not a numeric handle. The uncached path and the cached-frame fast path both break on Linux if they return that pointer through `uint32_t`, and callers like `CC_Draw_Shape(...)` must not store the result in `uint32_t` temporaries either.
  - Safe rule: only keep the shape-cache metadata itself as 32-bit relative offsets (`shape_data`, `KeyFrameSlots[...]` entries, etc.). Any value that is a live in-process address at the point of use must stay a pointer or at least `uintptr_t`.
- Once the startup crash is gone, UBSan becomes useful immediately on the same launch path.
  - `CODE/SHA.CPP::Process_Block(...)` still assumed the source block could be read as aligned `uint32_t[]`; on Linux/SDL3 that can trip misaligned-load UB during normal startup/setup flows. A small local `std::memcpy(...)` into a temporary word preserves behavior and removes the UB.
  - `CODE/RANDOM.CPP` still built bit masks with signed all-ones constants (`~0`, `~0L`), which trips UBSan for left-shifting negative values even though the intended logic is just an unsigned mask. Build those masks in unsigned space first, then cast back if needed.

## VQ callback and opaque-handle gotchas on LP64

- The VQA movie loader still uses two ABI-sensitive surfaces that must not be flattened blindly during the type sweep:
  - `MixFileHandler` must exactly match the private VQ callback type in `VQAPLAYP.H` (`int32_t (*)(VQAHandle *, int32_t, void *, int32_t)`). Leaving it as `long` on Linux keeps source compatibility at the call site but breaks the callback ABI because `long` is 64-bit on LP64.
  - `VQAHandle::VQAio` / `VQAHandleP::VQAio` are not 32-bit counters; they are opaque IO-manager storage that currently carries a live `CCFileClass*`. They must therefore be `uintptr_t`, not `uint32_t`, or startup movie playback will truncate the pointer and crash in `MixFileHandler` on the first `VQACMD_READ`.
- Practical symptom when either of the above is wrong: startup survives general game/bootstrap init, then crashes during the first intro movie (`ENGLISH.VQA`) inside `MixFileHandler` / `VQA_Open`.

## CODE tree sweep gotchas

- A CODE-wide primitive type sweep is safest when it is token-aware **and** preserves the whitespace that follows replaced type tokens.
  - Replacing `unsigned short key` with `uint16_t` is fine, but if the rewrite accidentally consumes the trailing separator too then declarations collapse into invalid tokens like `uint16_tkey`.
  - Practical rule: only replace the matched type tokens themselves; do not absorb the original post-type spacing.
- Primitive alias names can collide with real project identifiers.
  - `CODE/MONOC.H` still has an intentional enum member named `DOUBLE`; blindly replacing every `DOUBLE` token with the C++ type keyword `double` breaks the class definition immediately.
  - Practical rule: after a broad alias sweep, grep/build specifically for keyword collisions around historically alias-looking all-caps names before assuming the pass is purely mechanical.
- Stripping legacy modifier defines can leave malformed compatibility stubs behind.
  - In `CODE/LCW.H`, removing the now-dead `__cdecl` shim without checking the surrounding preprocessor block left an empty `#ifndef` / `#define` pair that broke every translation unit including `FUNCTION.H`.
  - Practical rule: after deleting old modifier macros, scan for blank preprocessor directives and empty guard blocks.
- Fixed-width types introduced into standalone CODE sources/headers often need local `<stdint.h>` includes even if much of the tree sees them indirectly through `FUNCTION.H`.
  - `CODE/BASE64.CPP` was the first compile break in this pass because it uses `uint8_t`/`uint32_t` directly but does not include `FUNCTION.H`.
  - Practical rule: after a type sweep, audit repo-owned files that now spell `uint*_t`/`int*_t` directly and add explicit `<stdint.h>` includes where they are not already reachable.

## Gameplay audio still has live duplicate SOS headers

- `WIN32LIB/AUDIO/` does **not** consistently compile against only the canonical `WIN32LIB/INCLUDE/SOS*.H` headers. The active gameplay audio path still has local duplicate headers in `WIN32LIB/AUDIO/`, and at least `WIN32LIB/AUDIO/SOSCOMP.H` is part of the live build through `SOUND.H` / `SOUNDINT.H`.
- This matters on LP64 hosts because `_SOS_COMPRESS_INFO` is shared with `CODE/ADPCM.CPP`. If the audio-local copy still uses old `long`/`unsigned long` spellings while the decoder side uses fixed-width 32-bit fields, the structure layout diverges and gameplay music/SFX decoding silently breaks even though VQA movie audio still works.
- Practical symptom when `WIN32LIB/AUDIO/SOSCOMP.H` drifts from the canonical fixed-width form: movie audio can still work, but normal music and sound effects fail because the non-movie stream/sample path in `WIN32LIB/AUDIO/{SOUNDIO,SOUNDINT}.CPP` passes a mismatched `_SOS_COMPRESS_INFO` into the shared ADPCM decoder.

_Last updated: 2026-04-02_

## Repo facts

- The repository ships a bundled SDL3 source tree in `extern/SDL3`.
- The main game logic lives in `CODE/`.
- Shared platform/rendering/input/audio support code lives in `WIN32LIB/`.
- The live movie stack now lives in `WIN32LIB` too:
  - the compiled VQA runtime is under `WIN32LIB/VQA32/`;
  - the public VQ/VQA/VQM headers are under `WIN32LIB/INCLUDE/{VQ.H,VQA32/,VQM32/}`;
  - the old `VQ/VQA32` and `VQ/INCLUDE` paths are historical only in older notes/changelog entries and should not be treated as the active build roots anymore.
- The active compatibility include order puts `SDL3_COMPAT/wrappers/` ahead of `CODE/` and `WIN32LIB/INCLUDE`.
- The main menu entry point is another good audio containment boundary.
  - `CODE/INIT.CPP::Select_Game(...)` is the first front-end step after leaving gameplay.
  - Practical rule: stop EVA speech there as well as during scenario clearing, so no scenario-local voice can leak into the title/menu flow even if a specific exit path forgets to wait correctly.

## Integer-width audit findings

- Not every old 32-bit-looking draw helper is actually carrying a numeric quantity.
  - `WIN32LIB/DRAWBUFF/SOFTDRAW.CPP::Buffer_Print(...)` still used integer temporaries as cursor state, but those values are really live framebuffer pointers (`buffer + y * row_bytes + x`), not logical pixel coordinates.
  - On LP64, storing them in `uint32_t` truncates the address and causes an immediate startup crash when the title-screen `"Stand By"` text is drawn after `Load_Title_Page()`.
  - Safe rule: for draw-buffer cursor/base values, keep pointer arithmetic in pointer types (`uint8_t *` / `const uint8_t *`) or `uintptr_t` only when an integer carrier is truly unavoidable.
- Startup reproduction has one launch-path gotcha in this tree.
  - `STARTUP.CPP` switches the virtual current directory to the executable's directory early in `WinMain`.
  - Practical rule: for runtime validation, use the copied `GameData/redalert` / `GameData/redalert-asan` binaries (or otherwise launch from the real game-data directory). Running `../build[-asan]/redalert` while your shell is inside `GameData/` is a false lead because the program immediately pivots its working directory back to `build/` or `build-asan/` and then cannot see `REDALERT.INI`.

- For broad legacy-type cleanup in `WIN32LIB`, a token-aware pass is much safer than naive search/replace.
  - Replacing identifiers while preserving all non-token text avoids the destructive whitespace/comment fallout that happens if the pass normalizes spaces or glues preprocessor lines together.
  - Practical rule: only rewrite code tokens, then let the compiler identify the few coupled seams outside `WIN32LIB` that must be updated to match the new declarations.
- The `WIN32LIB` tree still contains multiple type vocabularies even after the earlier VQ work, and they must stay split during cleanup.
  - `WWSTD` descendants historically used signed `WORD`-family spellings, so those interfaces should become `int16_t`/`int32_t`/`uint32_t` as appropriate rather than blindly inheriting the SOS/HMI unsigned mapping.
  - The audio-local SOS/HMI headers (`WIN32LIB/AUDIO/SOS*.H`) still follow the unsigned HMI convention and should stay aligned with the already-modern canonical public copies under `WIN32LIB/INCLUDE/SOS*.H`.
- The legacy root headers matter as much as the use sites.
  - `WIN32LIB/{INCLUDE,MISC}/WWSTD.H` and `WIN32LIB/AUDIO/SOSDEFS.H` can silently reintroduce old spellings if their alias-definition blocks remain after the user code is converted.
  - Safe cleanup rule: once the users are migrated, delete the alias-definition blocks instead of trying to keep “harmless” compatibility macros around.
- Expect direct ABI seams outside `WIN32LIB` after a declaration-wide sweep.
  - In this pass the two coupled fixes were `CODE/INIT.CPP::Calculate_CRC(...)`, which had to match the fixed-width `WIN32LIB/MISC.H` declaration, and `CODE/CCFILE.CPP`, where the correct endpoint was to make the real exported helpers match the underlying 32-bit `FileClass` / `RawFileClass` / `CCFileClass` contract directly rather than keeping temporary narrowing overloads around.
  - Practical rule: if a cleaned public header disagrees with the implementation but the underlying engine class hierarchy is already consistent, fix the implementation surface to that real contract and sync any duplicated public headers, instead of stacking adapter overloads on top.

- The active VQ build does not stay entirely within `VQ/INCLUDE`; some of its SOS/HMI surface is still taken from the canonical `WIN32LIB/INCLUDE/SOS*.H` headers through include-order.
  - Practical rule: when modernizing the VQ-side SOS declarations, validate the actual include path used by the VQ translation units before assuming the duplicated VQ headers are the only live copies.
  - In this pass, the build only went green once the shared canonical copies (`WIN32LIB/INCLUDE/SOS{,COMP,DATA,DEFS,FNCT}.H`) were kept in sync with the VQ copies.
- Legacy DOS/Watcom declaration modifiers in this tree are pure baggage on the SDL3/Linux build and can be removed outright once the surrounding type spellings are modernized.
  - Confirmed removable in the active VQ/SOS surface: `far`, `near`, `huge`, `interrupt`, `cdecl`, `__cdecl`, `_saveregs`, `_loadds`, and the local no-op define blocks that used to spell them in.
  - After stripping them, re-scan for declaration-only leftovers: old `extern` declarations are easy to miss because they often live in the duplicated SOS data/function headers even after the struct/prototype bodies are updated.
- Old DOS EOF markers still survive in a number of legacy headers in this repository.
  - When copying or normalizing old HMI/Westwood headers, watch for trailing `0x1A` (`^Z`) bytes; modern GCC/Clang treat them as stray characters in source and will fail the build.

- The VQ tree contains at least two incompatible legacy type vocabularies, so a blind global `WORD`/`BYTE`/`BOOL` replacement is unsafe.
  - `VQ/INCLUDE/WWLIB32/WWSTD.H` historically defined `WORD` as `signed short`, so its descendants should use `int16_t` when preserving that surface directly.
  - The HMI/SOS headers (`VQ/**/SOS*.H`) historically defined `WORD` as unsigned, so those declarations should become `uint16_t` instead.
- For the VQ/HMI support headers, do not blindly convert `BOOL` to `bool`.
  - The original aliases were 32-bit integer storage, and many of those declarations sit inside packed or externally-shaped movie/audio structures.
  - Safe rule: prefer `int32_t` for those legacy `BOOL` spellings unless the code is clearly a pure C++ semantic boolean with no layout sensitivity.
- After the VQ sweep, the practical cleanup rule for future work there is:
  - replace the type spellings at the use sites, not by introducing new compatibility aliases;
  - clean the root alias headers (`WWSTD.H`, `SOSDEFS.H`) once the users are converted, otherwise the old spellings tend to creep back in;
  - verify with both a repo-owned regex sweep and full `build` / `build-asan` rebuilds, because the duplicated VQ public/private headers make text-only spot checks easy to miss.

- Do not classify every remaining `long` in gameplay/network code the same way; the recent multiplayer sweep split into at least three distinct categories:
  - radio-message payload carriers, where the old code used `long & param` but the payload is actually the engine's encoded `TARGET` token rather than a native pointer;
  - true 32-bit protocol/state/file-format values such as packet IDs, CRCs, version numbers, retry timers, last-heard timestamps, and frame counters;
  - genuine pointer/address-like legacy paths, which still need separate handling and must **not** be blindly collapsed to 32-bit integers.
- The radio path is a concrete example of why the audit must follow call sites instead of the spelling alone.
  - `Transmit_Message(..., techno)` uses the separate `RadioClass *to` overload for object pointers.
  - The `param` payload itself is assigned values like `NavCom`, `TARGET_NONE`, `As_Target(cell)`, and `As_Target()` and is later consumed through `Assign_Destination(param)` / `As_Techno(param)`.
  - Safe rule: for `Receive_Message` / `Transmit_Message` payload references, use `TARGET`, not `long` and not `intptr_t`.
- `LParam` in this codebase is not Windows message `LPARAM`.
  - In the active radio code it is just a fallback storage slot for the no-parameter `Transmit_Message(...)` helper.
  - Safe rule: keep it aligned with the real radio payload type (`TARGET`), not with host pointer width.
- The connection/session layer still carries a lot of original Win32-32-bit assumptions, but most of them are legitimate 32-bit data model fields and should become `uint32_t`/`int32_t`, not pointer-width types.
  - Confirmed-safe 32-bit categories in the current pass:
    - `ConnectionClass` packet IDs, retry intervals, retry counts, timeout windows, sequence tracking, and response-time values;
    - `SessionClass` unique IDs, per-node last-heard timestamps, multiplayer version/CRC fields, `MaxAhead`, `FrameSendRate`, and sync-debug frame markers;
    - PlanetWestwood start-time and port globals used by the networking/statistics path.
- When changing a base timing/protocol API, check the whole inheritance surface before declaring success.
  - `ConnManClass::Response_Time()` / `Set_Timing()` still used `unsigned long` after the first `IPXManagerClass` conversion.
  - The correct fix was to move the abstract interface and the alternate implementations (`TENMGR`, `MPMGRW`, `MPMGRD`) to fixed-width types too, not to backslide the derived SDL/Linux code.
- The old retry-forever sentinel is still semantically required in the connection layer.
  - Original code compared retry counts/timeouts against `-1`.
  - After converting those fields to `uint32_t`, preserve that behavior explicitly with a 32-bit all-bits-set sentinel (`static_cast<uint32_t>(-1)` / equivalent), and update nearby templated arithmetic so mixed `unsigned long` literals do not leak back in through overload resolution.

## Include-case audit findings

- After the generated casefix headers were removed, the remaining failures split into two different classes:
  - true case-sensitive include misses, which were fixed by rewriting include text to the real on-disk filename casing;
  - semantic header-resolution changes, where a formerly-lowercase quoted include had previously resolved through the generated casefix layer and include-order to a canonical public header, but an exact-case quoted include on Linux started binding to a stale local duplicate instead.
- In this tree, "fix the include case" is therefore not just a search-and-replace problem.
  - Example pattern: many `WIN32LIB/*` sources historically wrote lowercase quoted includes, and the build-tree casefix headers plus include order effectively redirected those names into `WIN32LIB/INCLUDE`.
  - After the casefix layer was deleted, blindly changing those to exact-case quoted includes (`"WWMEM.H"`, `"WWSTD.H"`, `"BUFFER.H"`, `"PALETTE.H"`, `"SHAPE.H"`, `"WW_WIN.H"`, etc.) often changed which physical header was chosen on Linux.
  - The safe recovery rule was: if the local same-basename header is stale, malformed, or semantically different, include the canonical public header directly (usually with angle brackets) instead of "fixing" the local duplicate.
- Another large fallout class was comment-loss during the bulk rewrite/repair pass.
  - A number of files had old commented optional blocks such as `//#include ...`, `//#define ...`, `//#else`, and `//#endif` accidentally turned live.
  - Those lines caused cascading parse failures in files like `CODE/{DEFINES,SIDEBAR,EGOS,INIT,MENUS,MOUSE,NETDLG,NULLDLG,SCORE,SPRITE,WINSTUB}` and in headers such as `WIN32LIB/INCLUDE/RAWFILE.H` and `CODE/CCFILE.H`.
  - When this happens, comparing against `HEAD` is usually faster and safer than reasoning from the compiler errors alone, because one uncommented `#else`/`#endif` can make the rest of the file look unrelatedly broken.
- `WIN32LIB/WINCOMM/WINCOMM.CPP`'s `O_TEXT` usage is a legitimate portability issue, not a casefix artifact.
  - On Linux/SDL3 the correct behavior is to treat `O_TEXT` as a no-op, so a local `#ifndef O_TEXT / #define O_TEXT 0` in that translation unit is sufficient and preserves the original intent without changing behavior.
- The deleted `build/generated/casefix/...` headers were still masking a large amount of wrong-case repository includes. After a full source rewrite pass, the repository-owned case-only include mismatches are now down to **zero**.
- Practical rule for future include-case work:
  - validate against the actual compiler include roots from `build/compile_commands.json` / `build-asan/compile_commands.json`, not just the local source directory;
  - only count repository-owned paths as include-case bugs when the exact include text does not resolve but a case-insensitive match exists somewhere under the repo;
  - ignore standard library / system headers in these scans, otherwise names like `stdio.h` or `fcntl.h` create false positives.
- When bulk-rewriting legacy include names in this tree, preserve line endings carefully.
  - A naive text replacement can turn adjacent preprocessor lines into invalid glued directives such as `#include "X"#include "Y"` or `#include "X"#endif`.
  - If that happens, repair the directive boundaries first before trusting the next compiler failure; otherwise the build noise hides the real remaining include misses.
- Duplicate same-basename headers still need path-aware handling even after the case pass.
  - `WIN32LIB/AUDIO/SOUND.H` is the concrete example: a bare `SOS.H` include can bind to the local `WIN32LIB/AUDIO/SOS.H` copy before the intended public include-root path, and both `SOS.H` variants still carry old HMI typedef baggage.
  - The next sound-porting cleanup should treat the surviving `SOS*` headers as a real compatibility problem, not as another include-case issue.

## CPU and threading findings

- On the supported SDL3/Linux port, the extra game-owned threads around startup were legacy compatibility carryovers, not the main cause of high CPU.
  - The active timer path used to emulate WinMM `timeSetEvent()` behavior through the SDL compat layer, which created a worker thread just to advance Westwood timer state.
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` also kept a game-owned sound-maintenance thread even though regular sound pumping already happens from `Call_Back()` / `Sound_Callback()`.
  - Those game-owned helpers were removable on the active path without changing game logic: the timer backend now derives ticks from `SDL_GetTicksNS()` and sound service is pumped on the main thread.
- Measured runtime behavior after that cleanup showed that the main thread still dominated CPU, so the real hotspot is legacy polling on the main thread.
  - Important hot paths are `CODE/CONQUER.CPP::Sync_Delay()`, palette fade/countdown waits, speech waits in `CODE/SCENARIO.CPP`, menu/dialog `while (process)` loops, and the focus-loss restore loop in `CODE/WINSTUB.CPP`.
  - These loops historically assumed short waits and old Win32 timing behavior, so on a modern SDL port they can spin continuously unless they explicitly yield.
- For the SDL renderer path, remember that SDL3 renderer vsync is **disabled by default**.
  - `SDL3_COMPAT/wrappers/sdl_draw.cpp` presents from `WWDraw_Flush_Present()`, and `Call_Back()` reaches that path frequently.
  - If vsync is not explicitly enabled, the front-end can run presents flat out even after timer-thread cleanup.
- Current measured effect of the cleanup:
  - before: roughly `~95%` CPU on the main thread with about `8` total `redalert` threads;
  - after removing the game-owned timer/audio helpers, adding cooperative sleeps to the known busy waits, and enabling renderer vsync: roughly `~77%` CPU on the sampled startup/menu path with about `6` total threads.
- Practical rule for further CPU reduction work:
  - do not chase SDL/PipeWire backend threads unless they are actually active CPU users; in the samples they were sleeping in audio/event waits;
  - focus on front-end/main-thread pacing, especially loops that repeatedly call `Call_Back()` or `commands->Input()` without a meaningful idle wait;
  - preserve gameplay timing semantics by keeping `FrameTimer`/`WWTickCount` behavior intact and only adding cooperative yielding or present pacing around waits/idle UI loops.

## Movie-player CPU findings

- The VQA movie path is its own timing world and must be checked separately from the game/menu loop.
  - Startup and briefing movies go through `CODE/CONQUER.CPP::Try_Play_Movie()`, which calls into `VQ/VQA32/TASK.CPP::VQA_Play()`.
  - The usual `Sync_Delay()` / menu-loop pacing fixes do **not** affect this path.
- The key movie-specific hotspot was `VQ/VQA32/TASK.CPP::VQA_Play()`.
  - Its main loop repeatedly calls `VQA_LoadFrame()` and the configured draw function until both load and draw are done.
  - `VQ/VQA32/DRAWER.CPP::Select_Frame()` returns `VQAERR_NOT_TIME` when playback is ahead of the desired movie frame time.
  - The drawer and loader also return `VQAERR_SLEEPING` while waiting on `VQADATF_UPDATE` page-flip handoff or while audio blocks are still occupied.
  - Before the fix, those states caused immediate retries with no yield, so the movie player busy-spun even when it was explicitly waiting for time, the flipper, or audio buffer space.
- Safe fix for the SDL3 port:
  - keep the original frame-timing logic;
  - add a tiny cooperative delay only when a `VQA_Play()` iteration makes no progress (no frame loaded and no frame drawn).
  - This keeps movie timing behavior intact while turning the wait state into real sleep instead of a hot poll.
- Validation note:
  - after adding the `SDL_Delay(1)` yield in `VQ/VQA32/TASK.CPP`, a sampled intro-movie run dropped to about `~6% CPU`, and the main thread showed `hrtimer_nanosleep` instead of running flat out.

## Score-screen CPU findings

- The mission-end score presentation has its own pacing path in `CODE/SCORE.CPP` and needs separate treatment from menus and movies.
  - `Call_Back_Delay()` is the central helper used by score animations, graph step delays, and the final continue loop.
  - Before the fix it used countdown timers but still busy-polled: it repeatedly called `Animate_Score_Objs()` until the countdown expired with no `Sleep()`, so the score screen could burn CPU during every decorative delay.
- The hall-of-fame name-entry path also had an idle polling loop.
  - `ScoreClass::Input_Name()` repeatedly called `Call_Back()`, `Animate_Score_Objs()`, `Animate_Cursor()`, and keyboard checks while waiting for the next key, again with no yield.
- Safe fix for this path:
  - add a tiny `Sleep(1)` while the score countdown is still active in `Call_Back_Delay()`;
  - add a matching idle `Sleep(1)` in `ScoreClass::Input_Name()` while waiting for Enter/next keystroke.
  - This preserves the original score timing and animation order while preventing end-screen hot-spin.

## Remaining front-end/UI CPU findings

- After the menu, movie, and score fixes, the remaining high-CPU reports still followed the same pattern: private modal/UI loops outside the normal game pacing path.
- The most important single bug in that follow-up pass was `CODE/GOPTIONS.CPP`.
  - The options dialog had already gained a sleep after button-press handling, but its idle path still had no yield, so simply opening the in-game options screen could keep the loop polling flat out.
  - Fix: put the cooperative sleep on the general `process` path, not only the pressed-button path.
- Several other active front-end dialogs use the same `while (process) { Call_Back()/Main_Loop(); ... Input(); }` structure and are safe to pace the same way when idle.
  - Files fixed in this pass: `CODE/{SPECIAL,EXPAND,LOADDLG,GAMEDLG,CONFDLG,VISUDLG,DESCDLG,MPLAYER,SCENARIO}.CPP`.
  - The safe rule is: if the loop is a modal/front-end wait for input and not core per-frame gameplay simulation, add a small cooperative sleep when `process` stays true after one iteration.
- A related class of hotspots is timer/message waits that call `Call_Back()` but never yield.
  - Additional examples fixed in this pass: timed waits in `CODE/{LOADDLG,SCENARIO,EVENT}.CPP`.
  - Safe rule: if the code is waiting on a countdown, voice completion, or a UI message dwell timer and already calls `Call_Back()`, add a tiny `Sleep(1)` so the wait becomes scheduler-friendly without changing the logical order of operations.
- Scope note:
  - The broad pass focused on active single-player/front-end UI and common selector dialogs.
  - There are still many editor, modem/null-modem, WOL, and other niche multiplayer dialogs with similar loop shapes. Those should be handled by targeted profiling if they are reported as real CPU users, rather than by blindly touching every archival or niche path.
- `CODE/NETDLG.CPP` needed a second, more targeted audit after the broad UI pass.
  - The main dialog loops there already matched the normal modal pattern and now yield on the `process` path, but the more important remaining hotspots were the small internal waits that do not look like UI loops at first glance.
  - Hot examples were:
    - `while (Ipx.Global_Num_Send() > 0 && Ipx.Service() != 0)` packet-drain loops used during sign-off, join/startup handshakes, and per-player message fanout;
    - `while (Ipx.Global_Num_Send() > 0)` ACK waits after broadcasting `NET_GO` / `NET_LOADGAME`;
    - `while (TickCount - ok_timer < i)` grace-period waits before accepting OK/start when a new player just joined;
    - `while (TickCount - starttime < i)` post-ACK settle windows meant to give remote systems time to receive acknowledgements before local loading begins;
    - the `response_timer`-bounded `do { ... Ipx.Service(); ... } while (...)` loops waiting for `NET_READY_TO_GO` / `NET_REQ_SCENARIO`.
  - Safe rule for this class of code: if the loop is only waiting for network progress or a timer window and it already calls `Ipx.Service()`, add a tiny `Sleep(1)` so it becomes scheduler-friendly without changing handshake order or timeout semantics.
  - `CODE/{VISUDLG,SOUNDDLG}.CPP` should sleep whenever `process` remains true, not only for `GAME_NORMAL` / `GAME_SKIRMISH`; otherwise multiplayer/network invocations of the same dialogs can still spin even after the single-player pass is fixed.

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

## Reduced win32_compat surface

- `SDL3_COMPAT/wrappers/win32_compat.h/.cpp` had accumulated a lot of early-port compatibility baggage that no longer belongs in the active SDL3 build.
- Safe audit method for this wrapper:
  - start from the active compile database / actual build outputs, not from raw repository text search alone;
  - then confirm candidate call sites with repo searches, because some rare-looking symbols are still genuinely live in supported code;
  - as of the current tree, `WIN32LIB/WINCOMM/{MODEMREG,WINCOMM}.CPP` still depends on the registry-enumeration plus overlapped/event-style serial surface, while the old named-event / file-mapping trace subset became removable once `CODE/W95TRACE.CPP` stopped using it.
- The current port no longer needs the old process/thread/mutex/GDI shim layer in `win32_compat`.
- Practical follow-up rule:
  - do not keep Win32 wrapper entry points just because they belong to the same historical API family;
  - for example, serial overlapped events are still live, but that did **not** imply that the trace-only `OpenEvent` / `CreateFileMapping` / `MapViewOfFile` subset still needed to remain once `W95TRACE` was stubbed out.
  - Removed dead exports included: `UnregisterClass`, `IsWindow`, `DestroyWindow`, `LoadCursor`, `SetLastError`, `timeBeginPeriod`, `timeEndPeriod`, `GetCurrentThreadId`, `GetCurrentThread`, `GetCurrentProcess`, `DuplicateHandle`, `SetThreadPriority`, `CreateThread`, `WaitForInputIdle`, `GetExitCodeProcess`, `CreateMutex`, `ReleaseMutex`, `CreateProcess`, `GetFileSize`, `UnmapViewOfFile`, `RegQueryValue`, and the unused paint/palette/DIB stubs (`BeginPaint`, `EndPaint`, `GetDC`, `ReleaseDC`, `CreatePalette`, `SelectPalette`, `RealizePalette`, `DeleteObject`, `StretchDIBits`, `SetDIBitsToDevice`).
  - Matching dead internal baggage was also removable: `ProcessHandle`, `ThreadHandle`, `MutexHandle`, their `HandleKind` cases, the unused command-line splitter, dormant startup trace helpers, and the unused registry cache globals.
  - Matching dead header baggage was removable too: `MMRESULT`, `HDC`, `HPALETTE`, `PAINTSTRUCT`, `BITMAPINFO`, `LOGPALETTE`, `STARTUPINFO`, `PROCESS_INFORMATION`, `STILL_ACTIVE`, `DUPLICATE_SAME_ACCESS`, `THREAD_ALL_ACCESS`, and `THREAD_PRIORITY_TIME_CRITICAL`.
- After that cleanup, `WaitForSingleObject()` only needs to handle the kinds that are still real in the supported build: events. File handles, mappings, global allocations, registry handles, and windows still keep their own active helper paths where needed, but there is no longer a fake process/thread/mutex emulation layer underneath.
- Practical rule going forward:
  - if a future porting change is tempted to add a Win32 symbol to `win32_compat`, first prove that an active supported code path still needs it;
  - if the only hits are old comments, dead branches, or unused declarations in the wrapper itself, delete that baggage instead of extending the shim;
  - keep filesystem behavior in `sdl_fs`, drawing in `sdl_draw`, audio in the SDL audio backend, and only leave genuinely shared Win32-shaped glue in `win32_compat`.
- The adjacent calling-convention/storage-class macro block also needs symbol-by-symbol auditing, not bulk deletion.
  - In the current tree, `WINAPI`, `CALLBACK`, `PASCAL`, `FAR`, `far`, `near`, `__cdecl`, `cdecl`, `__stdcall`, and `_export` are still justified by live compiled declarations.
  - Important concrete live anchors:
    - `WINAPI`: the still-compiled `Sound_Thread` declaration in `WIN32LIB/AUDIO/SOUNDIO.CPP`;
    - `CALLBACK`: `WIN32LIB/TIMER/TIMERINI.CPP::Timer_Callback`;
    - `PASCAL` / `FAR` / `_export`: `CODE/WINSTUB.CPP::Windows_Procedure`;
    - `__stdcall`: active `CODE/IPX95.H` declarations and function-pointer typedefs;
    - `far` / `near`: surviving HMI/SOS-era headers and declarations that are still compiled.
  - `APIENTRY` was the only macro in that contiguous block that had no surviving in-repo usage beyond its own definition, so it was safe to remove.
  - `__declspec` is currently only referenced by the inactive `CODE/MOVIE.H` DLL interface (`DLLCALL`). That makes it a follow-up cleanup candidate tied to the dead movie-DLL surface, not evidence that `APIENTRY` or the rest of the block must remain.
- The old string helper aliases in `win32_compat.h` should be treated the same way: direct SDL callers are better than keeping Win32/C-runtime spellings alive when they only forward to SDL.
  - In this tree, a quick local search undercounted the real usage because some live callers were in less-obvious `.CPP` paths and one VQA file had its own local `stricmp` compatibility define. The reliable method was: make the obvious replacements, rebuild, then use repo-wide `git grep` on `CODE/`, `WIN32LIB/`, `VQ/`, and `SDL3_COMPAT/` to sweep the remaining compiled callers.
  - The active migration set for this pass was: `stricmp` / `_stricmp` / `strcmpi` -> `SDL_strcasecmp()`, `strnicmp` / `_strnicmp` / `memicmp` -> `SDL_strncasecmp()`, `strupr` -> `SDL_strupr()`, `strrev` -> `SDL_strrev()`, and `_strlwr` / `strlwr` -> `SDL_strlwr()`.
  - `VQ/VQA32/CONFIG.CPP` had a private non-MSVC `#define stricmp strcasecmp` fallback. Once the callers were switched to SDL, that local macro could be deleted too; add `#include <SDL3/SDL_stdinc.h>` there rather than keeping a second compatibility shim.
  - Safe cleanup rule: when a legacy helper name is just a thin alias for an SDL API, update the active callers to spell the SDL function directly and add an explicit SDL include at the use site if needed; then delete the alias from `win32_compat.h` instead of preserving it as permanent baggage. Treat comments and obsolete text snippets separately so the code cleanup stays behavior-neutral.
- `WIN32LIB/AUDIO/SOUNDIO.CPP` is still a live implementation file and must not be deleted yet.
  - It is still compiled by the active CMake build.
  - It still provides live entry points used directly by the game: `Audio_Init`, `Sound_End`, `Sound_Callback`, `Play_Sample`, `Stop_Sample`, `Sample_Status`, `Fade_Sample`, `File_Stream_Sample_Vol`, `Set_Primary_Buffer_Format`, `Start_Primary_Sound_Buffer`, `Stop_Primary_Sound_Buffer`, `Suspend_Audio_Thread`, and `Resume_Audio_Thread`.
  - Concrete callers include `CODE/{STARTUP,NULLDLG,CONQUER,EGOS,THEME,AUDIO,ENDING,INTRO,MAPSEL,SCENARIO,SCORE,OPTIONS,WINSTUB}.CPP` plus `WIN32LIB/PALETTE/PALETTE.CPP`.
  - Safe cleanup direction for later work is **inside** `SOUNDIO.CPP`: trim dead helper paths or stale thread-era leftovers after proving they have no remaining callers. Do not treat the whole translation unit as dead just because some old compatibility internals inside it are no longer needed.

## Removed DDE/WChat surface

- `SDL3_COMPAT/wrappers/ddeml.h` and `SDL3_COMPAT/wrappers/ddeml_compat.cpp` are now deleted.
- The old engine-side DDE layer (`CODE/DDE.*`) and the game-facing bridge (`CODE/CCDDE.*`) were removed together with the non-`WOLAPI_INTEGRATION` WChat integration that depended on them.
- The removed WChat path previously covered:
  - single-instance/DDE handoff in `CODE/STARTUP.CPP`;
  - WChat startup/start-packet handling in `CODE/INIT.CPP` and `CODE/INTERNET.CPP`;
  - WChat-specific timing/state adjustments in `CODE/EVENT.CPP` and `CODE/IPXMGR.CPP`;
  - WChat failure/return/reporting hooks in `CODE/SCENARIO.CPP`, `CODE/SAVELOAD.CPP`, `CODE/NETDLG.CPP`, and `CODE/CONQUER.CPP`;
  - the separate fake/internal `WWChat` session mode in the multiplayer dialog flow.
- Practical rule going forward:
  - do **not** reintroduce `ddeml.h`, `ddeml_compat.cpp`, `CODE/DDE*`, `CODE/CCDDE*`, `SpawnedFromWChat`, `Special.IsFromWChat`, or the internal `WWChat` mode for the supported SDL3 port;
  - if later internet/session work needs replacement behavior, implement it directly on supported SDL/network abstractions instead of reviving DDE-era launcher glue.

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
  - `SDL3_COMPAT/wrappers/sdl_fs.h` provides the public WWFS-prefixed path plus stdio/fd compatibility surface (`WWFS_NormalizePath`, `WWFS_GlobDirectory`, `WWFS_SplitPath`, `WWFS_MakePath`, `WWFS_FOpen`, `WWFS_FRead`, `WWFS_FWrite`, `WWFS_FSeek`, `WWFS_FTell`, `WWFS_Open`, `WWFS_Read`, `WWFS_Write`, `WWFS_Seek`, `WWFS_Close`, `WWFS_ChangeDirectory`, `WWFS_GetCurrentDirectory`, `WWFS_MakeDirectory`, `WWFS_GetCurrentDriveNumber`, `WWFS_GetDriveCount`, `WWFS_ChangeToDrive`, and the `_MAX_*` constants) for legacy code that still expects those APIs;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp` should stay a consumer of that layer for Win32-facing drive/window shims, not the home of the filesystem implementation itself.
  - `SDL3_COMPAT/wrappers/direct.h` is removed. Keep the supported surface on `WWFS_*` names inside `sdl_fs`, not in a parallel wrapper header or through revived `_foo` aliases.
  - Any wildcard scan that is meant to follow the game's virtual cwd must use `WWFS_GlobDirectory()`, not raw `SDL_GlobDirectory(".")`. The latter uses the host process cwd and will miss files such as `GameData/SAVEGAME.###` when startup has only redirected the WWFS virtual cwd.
- When porting more filesystem code:
  - route new file operations through SDL or these compat helpers;
  - prefer the low-level helper layer over scattering direct `SDL_IOFromFile` / `SDL_GetPathInfo` / `SDL_RemovePath` calls when the code needs relative-path behavior that historically depended on the process cwd;
  - keep upper layers unaware of case sensitivity and cwd semantics; that logic belongs in the file/path abstraction, not in gameplay code.
- Repository-wide searches for direct filesystem/file-I/O calls will still show some benign leftovers:
  - comments/documentation snippets,
  - helper definitions in `sdl_fs.h` / `sdl_fs.cpp`,
  - container/member-function `remove(...)` calls,
  - socket `close(...)` in `winsock.h`.
  Treat those as non-file-I/O false positives, not as reasons to reintroduce POSIX/stdio access.

## SDL drawing helper layer

- The repo-owned `<ddraw.h>` wrapper is gone from the supported SDL3 port. Do not reintroduce a DirectDraw-shaped public drawing surface for active code.
- The durable drawing seam is now:
  - `SDL3_COMPAT/wrappers/sdl_draw.cpp` owns the SDL-backed drawing implementation: software-surface storage, palette attachment/update, primary-surface presentation, queued present batching, and the SDL renderer/texture upload path;
  - `SDL3_COMPAT/wrappers/sdl_draw.h` provides a reduced public surface under `WW*` / `WWDRAW_*` names:
    - `WWDraw` for window binding, display-mode setup, palette/surface creation, video-memory totals, simple hardware feature booleans, and vertical-blank waits;
    - `WWSurface` for lock/unlock, blit, fill, restore, palette attach/query, and system-memory checks;
    - `WWPalette` for entry upload/readback;
    - `WWDraw_*` present helpers for deferred present batching;
  - `WIN32LIB/MISC/DDRAW.CPP` and the remaining legacy video-management code should stay consumers of that layer; the SDL renderer/present implementation belongs in `sdl_draw`, not in higher gameplay or Win32-shim code;
  - active buffer/palette call sites should use the renamed seam (`Get_Video_Surface()`, `Get_IsVideoSurface()`, `Set_Video_Palette()`, `Process_Draw_Result()`, `VideoDrawObject`) instead of reviving `Get_DD_Surface()`, `Set_DD_Palette()`, `DirectDrawObject`, or similar compatibility names.
- Keep the abstraction operation-based, not descriptor/caps based.
  - Do **not** reintroduce `WWVideoCaps`, `WWSurfaceCaps`, `WWSurfaceDescription`, `WWBlitFx`, `GetCaps()`, `GetBltStatus()`, descriptor-based `CreateSurface(...)`, or old `WWDRAW_BLT_*` flag sets just to mimic DirectDraw.
  - The remaining callers already map cleanly to direct operations such as `CreatePrimarySurface()`, `CreateSurface(width,height,system_memory,...)`, `IsSystemMemory()`, `CanBlit()`, `IsBlitDone()`, `FillRect()`, and boolean hardware capability queries on `WWDraw`.
- The only `ddraw` references that should remain after this cleanup are outside Red Alert-owned active port code, such as SDL upstream Windows-specific sources under `extern/SDL3/`.
- When porting more rendering code:
  - keep upper gameplay/UI code unaware of SDL renderer details just like the filesystem code stays unaware of host cwd/case-sensitivity details;
  - add new behavior to `sdl_draw` first, then expose the smallest necessary `WW*` surface upward rather than leaking SDL or legacy DirectDraw vocabulary across the tree;
  - repository-wide searches for `<ddraw.h>` should only hit SDL upstream's Windows-specific sources, not Red Alert-owned active code.

## SDL audio helper layer

- The repo-owned `dsound.h` / `dsound_compat.cpp` wrapper is gone from the supported SDL3 port. Do not reintroduce a DirectSound-shaped public audio surface for active code.
- The durable audio seam is already:
  - `WIN32LIB/INCLUDE/SDLAUDIOBACKEND.H` for the public backend types and return/status codes used by the game (`AudioBackendDevice`, `AudioBackendBuffer`, `AudioBufferFormat`, `RAAUDIO_*`);
  - `WIN32LIB/AUDIO/SDLAUDIOBACKEND.CPP` for the SDL-backed mixing, stream creation, primary/secondary buffer behavior, and focus handling;
  - `WIN32LIB/AUDIO/SOUNDIO.CPP` and related audio manager code as consumers of that backend, not as owners of another compatibility layer.
- `SDL3_COMPAT/wrappers/dsound.h` had become dead baggage: it was not part of the active CMake build, and its DirectSound-shaped types were no longer used by the supported sound path.
- The remaining VQA/movie-facing config header should stay lightweight:
  - `VQ/{INCLUDE/VQA32,VQA32}/VQAPLAY.H` may refer to `AudioBackendDevice*` / `AudioBackendBuffer*`, but it should do so with forward declarations only;
  - do **not** include `SDLAUDIOBACKEND.H` directly there, because that header pulls in `win32_compat.h`, which conflicts with the legacy `SOS.H` typedefs still used by `VQAPLAYP.H` and the VQA internals.
- Practical rule going forward:
  - if more audio behavior is needed, add it to `SDLAUDIOBACKEND.H/.CPP` and expose the smallest necessary operation upward;
  - do not add `IDirectSound*`, `DSBUFFERDESC`, `DirectSoundCreate`, `DSERR_*`, `DSBSTATUS_*`, or similar compatibility-shaped types back into Red Alert-owned active code;
  - repository-wide searches for `dsound.h` / `DirectSound` should only hit SDL upstream Windows-specific sources under `extern/SDL3/` or clearly archival comments that have not been cleaned yet.

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
- The old DirectShow/DirectDraw/MCI MPEG path has been removed from the SDL3 port; buffered VQA playback is the only remaining movie path.

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
  - timing / Win32 scheduling helpers (`Sleep`, thread priority helpers);
  - registry access (`RegOpenKeyEx`, `RegQueryValueEx`, `RegCloseKey`);
  - directory enumeration (`FindFirstFile`, `FindNextFile`, `FindClose`);
  - DirectDraw compatibility types used by legacy rendering code.
- `SDL3_COMPAT/wrappers/process.h` was removed after confirming there were no remaining non-generated source includes or `_beginthread()` call sites in the tree.
  - Keep new porting work off detached compatibility threads unless a subsystem is proven to need one on the active SDL path.
- `SDL3_COMPAT/wrappers/mmsystem.h` and the old MCI movie compatibility layer were removed after confirming both sides were dead on the SDL3/Linux port.
  - The active build has no remaining game-side `timeSetEvent()` / `timeKillEvent()` users.
  - `MCIMPEG` / `MPEGMOVIE` were not enabled in the active build, so the MCI movie sources and the compat `mciSendCommand()` shim were just dead baggage.
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
  - `CODE/SDLINPUT.CPP` / `CODE/SDLINPUT.H` own SDL event pumping/waiting, track key/button/toggle state, and maintain the shared mouse position snapshot;
  - the same SDL input layer now also owns the remaining public mouse compat surface (`SetCursor`, `ShowCursor`, `ClipCursor`, and `GetCursorPos()`), so keyboard and mouse input no longer have to reach back into `SDL3_COMPAT/wrappers/win32_compat.cpp` for live runtime behavior;
  - `CODE/CONQUER.CPP::Call_Back()` pumps SDL every front-end loop iteration and now also runs `WWMouse->Process_Mouse()` on that same main thread;
  - `SDL3_COMPAT/wrappers/win32_compat.cpp::next_message()` only drains posted lifecycle/focus/quit messages and uses `SDL_GameInput_Wait()` when the legacy message loop blocks;
  - `WWKeyboardClass` still owns the legacy circular key/mouse queue, but SDL key/button events are now fed into it directly instead of being translated into fake `WM_KEY*` / `WM_MOUSE*` messages first;
  - the state/query side of that path is now SDL-native (`SDL_Scancode`, `SDL_Keymod`, SDL mouse buttons), and the conversion to legacy `KN_*` values happens only at the queue boundary.
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
- The SDL3 port no longer needs Win32 virtual-key polling helpers in the compat wrapper.
  - `CODE/KEY.CPP::Down()` and the WOL/UI polling sites now query SDL-backed helpers directly instead of routing through `GetAsyncKeyState()` / `GetKeyState()`;
  - mouse buttons still have to drive the held-state checks that gameplay/UI rely on, because `CODE/GADGET.CPP::Input()` synthesizes `LEFTHELD` / `RIGHTHELD` from `Keyboard->Down(KN_LMOUSE)` / `Keyboard->Down(KN_RMOUSE)`;
  - the practical rule is: keep stored/queryable state in SDL terms, and only emit the old `KN_*` values when pushing into `WWKeyboardClass`.
- Exact `KN_*` comparisons are sensitive to how `CODE/KEY.CPP::Put_Key_Message()` synthesizes modifier bits.
  - `VQ_Call_Back()` checks `key == KN_ESC` exactly when deciding whether to break out of intro/video playback;
  - many dialogs and front-end flows also switch directly on `KN_ESC` / `KN_RETURN`, so CapsLock/NumLock must not set `WWKEY_SHIFT_BIT` on unrelated keys;
  - only alpha keys should inherit CapsLock-derived shift state, and only keypad-style keys should inherit NumLock-derived shift state.
- `Keyboard->Down(...)` compatibility depends on the reverse mapping in `CODE/SDLINPUT.CPP::SDL_GameInput_IsLegacyKeyPressed()`, not just the event queue.
  - special keys alone are not enough; gameplay and UI hotkeys also poll letters, digits, space, and punctuation through legacy `KN_*` values;
  - if those ordinary keys are not mapped back to SDL scancodes, features like letter-bound orders, number shortcuts, and modifier combos such as `Ctrl+Q` silently stop working even though queued input still looks fine.
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
  - `CODE/SDLINPUT.CPP` now owns `ClipCursor()` too, and it must keep converting mouse positions and clip rectangles through the same helpers, or menu hit-testing / software-cursor confinement drift away from the displayed image after a non-4:3 resize.
- The active mouse space is not always the same as the primary-surface space.
  - `CODE/STARTUP.CPP` often leaves the primary surface at `640x480` but attaches `SeenBuff` and `HidPage` as `640x400` viewports, usually at `(0,40)`.
  - `RA_WindowToGamePoint()` and `RA_GameRectToWindowRect()` work in primary-surface coordinates, so the SDL input layer must subtract `SeenBuff.Get_XPos()/Get_YPos()` after `RA_WindowToGamePoint()`, while `ClipCursor()` must add that origin back before calling `SDL_SetWindowMouseRect()`.
  - If that viewport offset is ignored, the software cursor and SDL confinement both skew toward the top-left corner after resize because SDL is clipping against the wrong logical rectangle.
- On Wayland compositors such as Hyprland, `SDL_SetWindowMouseRect()` is not just a local cursor helper.
  - SDL maps that API to Wayland pointer-confinement behavior, which prevents compositor window-management gestures like `Meta+left-drag` move and `Meta+right-drag` resize from stealing the real pointer while the game window is clipped.
  - `SDL3_COMPAT/wrappers/win32_compat.cpp::ClipCursor()` therefore must always keep the game-side `SDL_GameInput_SetCursorClip()` state, but skip SDL window confinement and the follow-up `SDL_WarpMouseInWindow()` clamp for windowed Wayland windows.
  - Non-Wayland and fullscreen paths can continue using the SDL mouse rectangle for OS-level confinement.

## General runtime correctness notes

- The old serial multiplayer support was not isolated to the transport backend.
  - `CODE/NULLDLG.CPP` historically mixed modem/null-modem setup with the still-live skirmish scenario picker, so deleting the file outright would also remove `Com_Scenario_Dialog(true)`.
  - The safe cleanup is to keep the file in place, replace it with a skirmish-only dialog implementation, and retain `Find_Local_Scenario(...)` because `NETDLG` and `WOL_GSUP` still rely on that helper.
- Once modem/null-modem support is removed, `SessionClass` can drop the serial settings/phone-book/init-string state entirely.
  - Those structures were only used by the deleted serial dialogs/backend, and removing them also lets `VECTOR.CPP` / `DYNAVEC.CPP` drop their `PhoneEntryClass *` explicit template instantiations.
  - Preserve the surviving `GameType` numeric values explicitly when deleting `GAME_MODEM` / `GAME_NULL_MODEM`; some multiplayer/session state is serialized and should not be renumbered accidentally.
- `CODE/SENDFILE.CPP`'s scenario-transfer path is shared, but the active SDL3 port only uses the network side of it.
  - After the serial backend is removed, keep the existing transfer flow and packet layout, but switch the command usage to the network `NET_*` file-transfer commands and treat the legacy `gametype` parameter as ignored compatibility baggage until callers are cleaned further.
- The modem removal unlocks a useful follow-up cleanup in `SDL3_COMPAT/wrappers/win32_compat.{h,cpp}`.
  - The live SDL3/Linux tree still needs the generic Win32-style file I/O and simple registry lookup surface (`CreateFile` / `ReadFile` / `WriteFile`, `RegOpenKeyEx` / `RegQueryValueEx` / `RegCloseKey`) for startup, WOL, and debug paths.
  - It does not need the comm-port compatibility layer anymore: the `DCB` / `COMMTIMEOUTS` / `COMSTAT` structs, related modem-control constants, and the stubbed `GetComm*` / `SetComm*` / `PurgeComm` / `EscapeCommFunction` / overlapped-serial helpers were only there for the deleted modem/null-modem backend.
  - The registry enumeration helpers (`RegQueryInfoKey`, `RegEnumKeyEx`) also become removable once `MODEMREG` is deleted, because the remaining code only queries a few known values directly.
  - After those are gone, a second pass is still worthwhile: `OVERLAPPED` / `LPOVERLAPPED`, `FindWindow()`, and a handful of old Win32 constants/aliases may remain in the compat header even though nothing in the live tree references them anymore.
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

## SDL drawing presentation notes

- The software renderer can draw correctly and still show an all-black SDL window if the SDL drawing layer never presents the updated primary surface.
- In the current drawing layer, primary-surface presentation must happen after:
  - `WWSurface::Unlock()` on the primary surface;
  - `WWSurface::Blt()` when the destination is the primary surface;
  - `WWSurface::SetPalette()` when a primary surface palette is attached or changed.
- For front-end overlays, those primary-surface updates must be coalesced before hitting SDL.
  - Many menus and dialogs draw directly to `SeenBuff`, and the primitive helpers (`Print`, `Fill_Rect`, `Put_Pixel`, etc.) lock/unlock the destination repeatedly.
  - If every primary-surface unlock/blit triggers an immediate SDL present, overlay redraw becomes visibly incremental and buttons repaint one by one.
  - Queue the present in `sdl_draw`, flush once per callback tick, and use an explicit batch around `GadgetClass::Draw_All()` so the whole overlay lands in one SDL frame.
- VQA movie playback needs an explicit present flush on the Win32/SDL path.
  - `CODE/CONQUER.CPP::VQ_Call_Back()` blits each decoded movie frame to `SeenBuff`, but unlike many front-end loops it does not call `Call_Back()` afterward.
  - After switching the drawing layer to queued presents, movie frames therefore need `WWDraw_Flush_Present()` directly from the callback or the SDL window stays black while the decoded frame buffer keeps updating underneath.
- The SDL drawing surface needs the owning SDL window/`HWND` stored with it so presentation can target the correct window during those updates.
- The SDL texture format must match how the drawing layer packs palette-expanded pixels.
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
