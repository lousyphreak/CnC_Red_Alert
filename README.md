# Command & Conquer Red Alert SDL3 Port

This repository is an in-progress SDL3/CMake port of the archived Red Alert source tree. The current branch has a validated Linux runtime baseline, a restored Windows/MSVC build, and an Emscripten/browser target, while keeping original game behavior and data handling as intact as possible.

## What changed in this port

- **Build system and targets**
  - Replaced the old platform-specific build flow with a top-level CMake build.
  - Bundled SDL3 under `extern/SDL3` and moved the active platform layer onto SDL3-backed code.
  - The main target is `redalert`, with desktop and browser build variants driven from the same top-level CMake project.
  - Active targets now cover Linux, Windows/MSVC, and Emscripten/browser builds.
- **Platform migration**
  - Replaced large parts of the Win32-facing filesystem, window, input, audio, timing, and rendering paths with SDL3-backed implementations.
  - Added a repo-owned config layer (`sdl_config`) instead of fake Windows registry plumbing for live settings lookups.
  - Fixed repository include casing so the tree works on case-sensitive filesystems without generated case-fix headers.
  - Continued a broad fixed-width integer and LP64 audit to remove 32-bit Windows assumptions that broke Linux and sanitizer runs.
- **Runtime and data fixes**
  - Restored the active VQA movie path, SDL-backed movie audio, palette handling, startup/front-end presentation, and multiple sanitizer-found runtime issues.
  - Fixed expansion-pack detection so Counterstrike and Aftermath are detected from `EXPAND.MIX` and `EXPAND2.MIX`, and corrected the Aftermath rules overlay loading path.
  - Reduced front-end/movie/score-screen busy-loop CPU usage by pacing legacy wait loops without changing gameplay timing.
  - Finalized the browser `.MIX` sparse cache / lazy-fetch path so web builds can stream game data more reliably.
- **Multiplayer changes**
  - Kept the live UDP multiplayer path.
  - Added a direct-IP TCP transport that reuses the existing internet multiplayer flow and supports both UI-driven and command-line host/connect startup.
  - Split the socket layer into thin Windows and Linux implementations behind a shared neutral header.
  - Removed the legacy modem/null-modem backend and its dead compatibility surface.
  - Westwood Online code is still present as archival reference, but it is not active in the build and cannot be re-enabled directly because the old bridge/backend pieces are missing.

## Dependencies

- CMake
- A C11/C++17-capable compiler
- SDL3 is vendored in-tree under `extern/SDL3`
- Emscripten is only needed for the browser build

You must also own the original game data to run the executable. The C&C Ultimate Collection is available on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc) and [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).

## Configurable CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `RA_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer. |
| `RA_ENABLE_UBSAN` | `OFF` | Enable UndefinedBehaviorSanitizer. |
| `RA_ENABLE_UDP` | `ON` | Enable the UDP multiplayer transport. |
| `RA_ENABLE_TCP` | `ON` | Enable the direct-IP TCP multiplayer transport. |
| `RA_ENABLE_WOL` | `ON` | Enable the WebSocket/WOL-replacement multiplayer transport. |
| `RA_ENABLE_DVD_MOVIES` | `OFF` | Enable DVD-specific movie handling. |
| `RA_EMSCRIPTEN_PACKAGE_GAMEDATA` | `OFF` | Preload the game data into the browser build output. |
| `RA_EMSCRIPTEN_LAZY_FETCH_GAMEDATA` | `ON` | Stream game data over HTTP on demand in the browser build. |

`RA_ENABLE_UDP` and `RA_ENABLE_TCP` are forced off for Emscripten builds, and `RA_ENABLE_WOL` is forced on so the browser build keeps the WebSocket multiplayer path available by default.

## Building

Standard desktop build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

ASan/UBSan build:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DRA_ENABLE_ASAN=ON -DRA_ENABLE_UBSAN=ON
cmake --build build-asan -j
```

Emscripten build:

```sh
emcmake cmake -S . -B build-emscripten -DCMAKE_BUILD_TYPE=Debug
cmake --build build-emscripten -j
```

## Running

Desktop builds produce the executable in the selected build directory, for example:

- `build/redalert`
- `build-asan/redalert`

### Game data directory and `-gamedata`

The game expects the original downloaded Red Alert data directory, not the repository's development-only asset copy.

Use `-gamedata <dir>` to point the executable at your real game data location:

```sh
./build/redalert -gamedata "/path/to/your/downloaded/red-alert-data"
```

Sanitized run:

```sh
./build-asan/redalert -gamedata "/path/to/your/downloaded/red-alert-data"
```

At startup, the engine checks for game data in this order:

1. the directory passed via `-gamedata <dir>`
2. the current working directory
3. the executable directory

The selected directory must contain `REDALERT.MIX` and `MAIN1.MIX` through `MAIN4.MIX`.

Mission packs are detected from the downloaded game data files:

- `EXPAND.MIX` -> Counterstrike
- `EXPAND2.MIX` -> Aftermath

## Direct-IP TCP multiplayer

The SDL3 port keeps the original UDP path and also adds a direct-IP TCP path for internet-style multiplayer without Westwood Online.

Command-line startup is supported:

```sh
./build/redalert -gamedata "/path/to/your/downloaded/red-alert-data" -host [port] -name Host
./build/redalert -gamedata "/path/to/your/downloaded/red-alert-data" -connect <ip> <port> -name Guest
```

If the host port is omitted, the default internet port is `34835` (`0x8813`).

## Browser build and container images

The repository includes a multi-stage `Dockerfile` that builds the browser target and the Zig WOL server, then serves `redalert.html`, `/GameData/...`, and `/ws` from the same HTTP port.

Build the plain runtime image:

```sh
docker build --target web-runtime -t redalert-web:latest .
```

Build the runtime plus the filtered `GameData` layer. For this Docker workflow only, copy the original downloaded game data into the repository's `GameData/` directory first so the filter can run:

```sh
docker build --target web-runtime-with-gamedata -t redalert-web-gamedata:latest .
```

The `web-runtime-with-gamedata` target copies the filtered `GameData` layer before the HTML/JS/wasm output so Docker can reuse the asset layer across binary rebuilds. It only carries browser-relevant assets derived from `ra-assets-manifest.txt`; desktop-only extras under `GameData/RED ALERT/` and browser-local files such as savegames are intentionally excluded.

In the Emscripten/browser build, WOL always derives its WebSocket endpoint from the current page origin. For example, if the game is served from `https://ra.example.com/redalert.html`, the browser transport uses `wss://ra.example.com/ws`.

The browser multiplayer chooser also defaults to the WOL/WebSocket option, so the hosted network path is the default multiplayer backend in Emscripten builds.

## Web container basic auth

The Zig-hosted image can enforce HTTP basic auth itself for both static hosting and the `/ws` upgrade, independent of ingress behavior.

Enable it with a username and password:

```sh
docker run --rm -p 8080:80 \
  -e RA_BASIC_AUTH_USERNAME=redalert \
  -e RA_BASIC_AUTH_PASSWORD='change-me' \
  redalert-web-gamedata:latest
```

Without auth:

```sh
docker run --rm -p 8080:80 \
  redalert-web-gamedata:latest
```

If neither `RA_BASIC_AUTH_USERNAME` nor `RA_BASIC_AUTH_PASSWORD` is set, image-level auth stays disabled. The web page, lazy `GameData` range fetches, and WOL WebSocket relay all share that same listener on port `80`.

## License

This repository and its contents are licensed under the GPL v3 license, with additional terms applied. See [LICENSE.md](LICENSE.md) for details.
