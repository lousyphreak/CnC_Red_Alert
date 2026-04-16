# Networking Status

This document inventories the remaining networking code in the current SDL3/CMake tree, explains how it is wired into the game, summarizes what is still active versus dormant, and outlines what it would take to fully re-enable or extend it.

It is intentionally focused on the code that exists in this repository today, not on the original shipping feature matrix.

## Executive summary

- The active supported multiplayer path in the current tree is the LAN UDP path centered on `UDPManagerClass`, `UDPConnClass`, `UDPGlobalConnClass`, `SessionClass`, `NETDLG.CPP`, and `QUEUE.CPP`.
- The old gameplay/session protocol is still present and still integrated. The port did not remove the higher-level lockstep, packet queue, lobby, chat, file transfer, or save/load logic.
- The "Internet" path is only partially present. The lower-level TCP/IP manager (`TcpipManagerClass Winsock`) and the fake host/join dialogs still exist, but the active front-end does not expose them, and the original service-side assumptions are obsolete.
- Westwood Online, TEN, MPath, modem/null-modem, and the old Windows-only helper transports are no longer part of the supported build path. Some headers and classes remain, but they are either compile-gated, source-excluded, or both.
- The cleanest seam for a new backend is still the old connection-manager layer: `ConnManClass` at the top, `ConnectionClass` and its queue/ACK logic underneath, then `Session`/`NETDLG`/`QUEUE` above that.

## Current build status

### Build options and compile-time gates

The current CMake build keeps only the supported network surfaces active:

- `RA_ENABLE_UDP` defaults to `ON` in `CMakeLists.txt`.
- Emscripten forcibly sets `RA_ENABLE_UDP=OFF` and defines `INTERNET_OFF`.
- `RA_ENABLE_UDP` is forwarded into the code as a compile definition.
- `WOLAPI_INTEGRATION` is not defined by the current CMake build.
- `NETWORK_UDP` is not defined by the current CMake build.
- `TEN` and `MPATH` are compiled as `0` in `CODE/DEFINES.H`.

The build also excludes several old transport implementations outright:

- `WSPROTO.CPP`, `_WSPROTO.CPP`, and `WSPUDP.CPP`
- `MPLIB.CPP`, `MPMGRD.CPP`, `MPLPC.CPP`
- `NULLCONN.CPP`, `NULLMGR.CPP`
- additional legacy/non-port targets already removed from the active source list

That means the tree still contains networking history, but the supported build only ships a narrow subset of it.

### UI gating and reachability summary

The networking UI is gated in two different ways:

- some screens are compiled out entirely with `#ifdef WOLAPI_INTEGRATION`, `#if(TEN)`, or `#if(MPATH)`
- some screens still compile, but no active menu or startup path reaches them anymore

The important front-end gates in the current tree are:

- `CODE/MENUS.CPP` exposes `BUTTON_MULTI` / `TXT_MULTIPLAYER_GAME` in the active `FIXIT_VERSION_3` path
- `BUTTON_INTERNET` / `TXT_INTERNET` only exist in the non-`FIXIT_VERSION_3` branch, so the main menu no longer offers a direct internet button in the supported build
- `CODE/MPLAYER.CPP::Select_MPlayer_Game()` actively offers `BUTTON_SKIRMISH` and `BUTTON_UDP`
- `BUTTON_WOL` in `Select_MPlayer_Game()` is compiled only under `WOLAPI_INTEGRATION`, which the current build never defines
- `GAME_INTERNET` still exists in `Session.Type`, but there is no active user-facing selection path that returns it

In practical terms, the active UI offers:

- main menu -> multiplayer
- multiplayer chooser -> skirmish or UDP network
- UDP network -> join/new game lobby flow

What the supported UI does not offer anymore:

- direct-IP host/client choice
- direct internet address entry
- WOL login/chat/matchmaking
- TEN/MPath service entry

## What networking code is still available

### 1. Core transport/session abstraction layer

These files still matter even if the concrete transport changes:

| File | Main type(s) | Status | Purpose |
|---|---|---|---|
| `CODE/CONNMGR.H` | `ConnManClass` | Active | Abstract manager interface used by the game loop. This is the main backend seam. |
| `CODE/CONNECT.H` | `ConnectionClass` | Active | Per-peer reliable/unreliable packet engine with ACK/retry/timeouts. |
| `CODE/SEQCONN.*` | `SequencedConnClass` | Present | Ordered-delivery variant layered under the manager abstraction. |
| `CODE/NOSEQCON.*` | `NonSequencedConnClass` | Present | Non-sequenced variant. |
| `CODE/COMBUF.*` | `CommBufferClass` | Active | Packet staging and queue storage. |
| `CODE/COMQUEUE.*` | `CommQueueClass` | Active | Lower-level queue implementation. |
| `CODE/SESSION.*` | `SessionClass` | Active | Multiplayer state, lobby/game options, player/game lists, save/load state. |
| `CODE/EVENT.*` | `EventClass` | Active | Lockstep gameplay events carried over the transport. |

Important point: the game logic is not written directly against sockets. It is written against a connection manager plus a multiplayer session layer.

### 2. Active LAN UDP transport

This is the live transport in the current Linux/SDL build:

| File | Main type(s) | Status | Purpose |
|---|---|---|---|
| `CODE/UDPMGR.*` | `UDPManagerClass` | Active | Top-level transport manager used by the game. |
| `CODE/UDPCONN.*` | `UDPConnClass` | Active | Per-connection UDP transport implementation. |
| `CODE/UDPGCONN.*` | `UDPGlobalConnClass` | Active | Broadcast/global channel for discovery and lobby traffic. |
| `CODE/UDPADDR.*` | `UDPAddressClass` | Active | Compatibility address container. |
| `CODE/UDP.H` | packet/address structs | Mixed | Still carries old Novell-era layout baggage, but the live path uses it as a compatibility container. |
| `CODE/SOCKETS.H` | thin socket seam | Active | Platform-neutral socket wrapper header used by the live transports. |
| `CODE/SOCKETS_WINDOWS.CPP` | Windows socket backend | Active on Windows | Thin WinSock implementation for `CODE/SOCKETS.H`. |
| `CODE/SOCKETS_LINUX.CPP` | Linux socket backend | Active on Linux | Thin BSD-socket implementation for `CODE/SOCKETS.H`. |

What is live here:

- nonblocking UDP socket open/listen/send/receive
- broadcast discovery
- bridge target support
- per-peer private channels
- ACK/retry timing
- response-time tracking
- lobby discovery and join flow
- in-game chat
- scenario transfer
- lockstep event exchange

### 3. TCP/IP internet path

This code still exists, but it is not fully surfaced in the supported game flow:

| File | Main type(s) | Status | Purpose |
|---|---|---|---|
| `CODE/TCPIP.*` | `TcpipManagerClass` / global `Winsock` | Partially active | Direct TCP/UDP internet manager for head-to-head play. |
| `CODE/INTERNET.*` | helper functions and globals | Partially active | Internet connect helpers, stats packet path, host/client globals. |
| `CODE/NETDLG.CPP` | `Server_Remote_Connect()`, `Client_Remote_Connect()`, fake dialogs | Partially active | UI and setup flow for direct host/join style play. |

What still works in code:

- `Winsock` is constructed globally.
- `TcpipManagerClass` still has `Init()`, `Start_Server()`, `Start_Client()`, `Read()`, `Write()`, and `Close()`.
- both the TCP/IP path and the LAN UDP path now sit on the same thin `CODE/SOCKETS.H` wrapper seam instead of including raw platform socket headers in gameplay/network headers.
- the platform split now lives in `CODE/SOCKETS_WINDOWS.CPP` and `CODE/SOCKETS_LINUX.CPP`, with `TcpipManagerClass` using nonblocking/polling socket behavior through that wrapper layer on both platforms.
- `PlanetWestwoodIPAddress`, `PlanetWestwoodPortNumber`, and `PlanetWestwoodIsHost` still exist.
- `Server_Remote_Connect()` and `Client_Remote_Connect()` still exist in `NETDLG.CPP`.
- `Net_Fake_New_Dialog()` and `Net_Fake_Join_Dialog()` still exist.

What is missing from the supported flow:

- no active menu path exposes this mode in the current build
- the original service assumptions are outdated
- old online stats/reporting targets are obsolete
- some code still carries "internet mode" assumptions that need a deliberate audit before calling the path production-ready again

### 4. WOL, TEN, MPath, and other legacy backends

These are not part of the supported build path anymore.

#### Westwood Online / WOL

Still present:

- `CODE/WOL_MAIN.CPP`
- `CODE/WOL_CHAT.CPP`
- `CODE/WOL_LOGN.CPP`
- `CODE/WOL_OPT.CPP`
- `CODE/WOL_DNLD.CPP`
- `CODE/WOL_CGAM.CPP`
- `CODE/WOL_GSUP.*`
- `CODE/WOLSTRNG.*`
- `CODE/WOLEDIT.*`
- `CODE/WOLDEBUG.H`

But:

- all of it is gated by `#ifdef WOLAPI_INTEGRATION`
- that define is not supplied by the current build
- the old COM/WOL bridge was removed from the repo
- the original service is gone anyway

Practical status: archival only.

#### Winsock helper transport layer

Still present in headers:

- `CODE/WSPROTO.H`
- `CODE/_WSPROTO.H`
- `CODE/WSPUDP.H`

But:

- their `.CPP` files are excluded from the build
- the old `PacketTransport` path is not active in the supported build
- `NETWORK_UDP` is not defined by current CMake

Practical status: dormant compatibility leftovers.

#### TEN / MPath

Still present:

- `CODE/TENMGR.*`
- `CODE/MPMGRW.*`

But:

- `TEN` and `MPATH` are disabled at compile-time
- the service integrations they expected are not part of the supported port

Practical status: not realistically recoverable without larger restoration work.

#### Modem / null-modem / serial

The serial gameplay path was intentionally removed from the supported tree:

- old transport sources are excluded
- `NULLDLG.CPP` was replaced with skirmish-only behavior
- related session settings were already trimmed down in the porting effort

Practical status: deliberately unsupported.

## How the active networking code integrates with the game

### Startup and front-end flow

The multiplayer entry flow still exists in the main game startup path:

1. `INIT.CPP` asks `Select_MPlayer_Game()` in `MPLAYER.CPP`.
2. The active menu currently exposes `GAME_SKIRMISH` and `GAME_UDP`.
3. If `GAME_UDP` is selected:
   - `Init_Network()` is called from `NETDLG.CPP`
   - that initializes `Udp`
   - `Remote_Connect()` drives the host/join dialog flow
4. If successful, the game falls through into the normal gameplay loop with multiplayer state already populated in `Session`.

Important detail:

- `INIT.CPP` still contains old `PacketTransport` creation inside `#ifdef NETWORK_UDP`, but the actual supported path continues into `Init_Network()` and the live `Udp` manager regardless.
- `GAME_INTERNET` handling in `INIT.CPP` is effectively only live under `WOLAPI_INTEGRATION`, so the direct internet path is not currently front-end reachable in the supported build.

### UI integration in detail

The networking code is not just called from gameplay; it is still deeply embedded in the front-end UI flow.

#### Main menu ownership

The top-level entry point is:

- `CODE/MENUS.CPP::Main_Menu()`
- `CODE/INIT.CPP::Select_Game(bool fade)`

`Select_Game()` defines a local `SEL_*` enum whose order must match the main-menu buttons. In the active `FIXIT_VERSION_3` build, the multiplayer entry is:

- `SEL_MULTIPLAYER_GAME`

and it is fed by:

- `BUTTON_MULTI` in `Main_Menu()`
- label `TXT_MULTIPLAYER_GAME`

The older direct internet main-menu path still exists only in the inactive non-`FIXIT_VERSION_3` branch:

- `BUTTON_INTERNET`
- `TXT_INTERNET`
- `SEL_INTERNET`

That code is a useful reference for where an internet-facing button used to live, but it is not part of the supported UI today.

#### Multiplayer chooser ownership

The second-tier chooser is:

- `CODE/MPLAYER.CPP::Select_MPlayer_Game()`

This dialog is the current UI seam between the general front-end and multiplayer-specific backends. It currently owns these choices:

- `BUTTON_SKIRMISH` -> `GAME_SKIRMISH`
- `BUTTON_UDP` -> `GAME_UDP`
- `BUTTON_CANCEL` -> `GAME_NORMAL`

and conditionally:

- `BUTTON_WOL` -> `GAME_INTERNET` under `#ifdef WOLAPI_INTEGRATION`

That means the supported build already has a dedicated place to add a revived direct-IP button or a brand-new backend selector without touching the main menu first.

#### Shared skirmish/network crossover UI

One important surviving UI seam is:

- `CODE/NULLDLG.CPP::Com_Scenario_Dialog(bool skirmish)`

Historically that file mixed skirmish and serial/modem setup. In the current SDL3 port it was reduced to the surviving shared pieces:

- the skirmish scenario picker
- local scenario lookup helper(s) such as `Find_Local_Scenario(...)`

Even though this is not itself the UDP lobby, it still matters to networking because:

- `Select_MPlayer_Game()` uses it directly for skirmish
- `NETDLG.CPP` and `SENDFILE.CPP` still depend on the local scenario lookup path for multiplayer scenario/file-transfer behavior

#### LAN UDP lobby ownership

The active UDP lobby UI lives almost entirely in `CODE/NETDLG.CPP`:

- `Init_Network()`
- `Shutdown_Network()`
- `Remote_Connect()`
- `Net_Join_Dialog()`
- `Net_New_Dialog()`
- `Process_Global_Packet()`
- `Destroy_Connection()`
- `Net_Reconnect_Dialog()`

`Remote_Connect()` is the central dispatcher:

- it reads/writes multiplayer settings through `Session`
- it shows the join dialog first
- from there it branches into the host/new-game dialog
- both dialogs use the global UDP channel for discovery, player/game list maintenance, chat, and join approvals

`Net_Join_Dialog()` is the player-facing browser for available games. It still owns:

- local player name editing
- visible game list
- visible player list for the selected game
- join/new/cancel buttons
- multiplayer message/chat line
- polling loops that keep the game list fresh through UDP global messages

`Net_New_Dialog()` is the host-side setup lobby. It still owns:

- host player list
- scenario selection
- multiplayer game options
- color/house ownership within the session data
- join-approval flow
- launch/go transition into gameplay

#### File-transfer UI ownership

Custom-scenario transfer is not a hidden backend detail; it still has explicit UI in:

- `CODE/SENDFILE.CPP`

Key user-facing flows include:

- `Get_Scenario_File_From_Host(...)`
- `Receive_Remote_File(...)`
- `Send_Remote_File(...)`

These functions still own:

- progress UI while downloading or uploading a scenario
- wait loops around chunk transfer
- redraw handling while the transfer is in progress

This is one reason the current UDP path is more complete than a "socket only" bring-up: the user-facing scenario sync flow is still present.

#### Reconnect and waiting UI ownership

Network fault handling still has dedicated UI:

- `CODE/NETDLG.CPP::Net_Reconnect_Dialog()`

This dialog is still used by the in-game multiplayer flow to show:

- reconnecting-to-player messaging
- waiting-for-player state
- timeout/cancel/retry messaging

It already contains `GAME_UDP` and `GAME_INTERNET` cases, which is useful if direct internet is revived.

#### Compiled-but-unreachable direct internet UI

The old non-WOL direct host/join UI still compiles in the current build:

- `CODE/NETDLG.CPP::Net_Fake_New_Dialog()`
- `CODE/NETDLG.CPP::Net_Fake_Join_Dialog()`
- `CODE/NETDLG.CPP::Server_Remote_Connect()`
- `CODE/NETDLG.CPP::Client_Remote_Connect()`

These are guarded by `#ifndef WOLAPI_INTEGRATION`, so they are present in the current build, but no active menu path calls them.

That makes them different from WOL:

- they are not removed code
- they are not source-excluded
- they are dormant because the UI no longer offers a route into them

#### Fully gated WOL/TEN/MPath UI

The WOL front-end remains spread across several files, but all of it is compile-gated:

- `CODE/WOL_MAIN.CPP`
- `CODE/WOL_LOGN.CPP`
- `CODE/WOL_CHAT.CPP`
- `CODE/WOL_CGAM.CPP`
- `CODE/WOL_GSUP.CPP`
- `CODE/WOL_OPT.CPP`
- `CODE/WOL_DNLD.CPP`

Those files collectively cover:

- WOL login
- chat/channel browsing
- game creation
- game-setup lobby
- options
- download/update progress

They are useful as UI/reference material only.

TEN and MPath follow the same pattern at a higher level:

- related code remains in-tree
- compile guards keep it out of the active UI
- the current build exposes no menu path for those services

### Lobby, discovery, join, and file transfer

The lobby layer is still heavily integrated and still substantial:

- `NETDLG.CPP` contains `Init_Network()`, `Shutdown_Network()`, `Remote_Connect()`, `Net_Join_Dialog()`, `Net_New_Dialog()`, `Process_Global_Packet()`, `Destroy_Connection()`, and the fake internet host/join dialogs.
- `UDPGlobalConnClass` is used as the "global channel" for discovery and lobby commands.
- `SessionClass` stores the visible game and player lists, scenario data, and multiplayer options.
- `SENDFILE.CPP` still handles scenario transfer over the network path.

The global command layer still includes:

- game discovery
- player discovery
- chat announcement/request
- join confirmation/rejection
- game options propagation
- go/start
- ping/response timing
- scenario request/info/chunk transfer
- ready-to-go / no-scenario

From a UI perspective, this means the lobby layer still has all of the expected user-visible surfaces for LAN play:

- game browser
- player list
- host setup/options
- join approval/rejection messaging
- chat/message line
- progress UI for scenario transfer
- reconnect/wait feedback after gameplay begins

### In-game lockstep integration

Once the match starts, networking is deeply integrated into the normal game loop:

- `QUEUE.CPP::Queue_AI()` selects `net = &Udp` for `GAME_UDP` and `GAME_INTERNET`.
- `net->Service()` is called repeatedly through the loop.
- `Send_Frame_Sync_Packet()` pushes lockstep frame info.
- `EventClass` data is packed into `Session.MetaPacket` and sent through the private channel.
- response time influences `Session.MaxAhead`.
- CRCs and frame sync checks are still part of the existing multiplayer flow.

This is the key reason the networking code is still valuable: the hard part of deterministic lockstep integration is still there.

### Save/load and configuration integration

Multiplayer state is not isolated to transient UI; it is part of the saved game and configuration flow:

- `Session.Save()` and `Session.Load()` persist multiplayer-relevant state.
- `SAVELOAD.CPP` uses those session save/load hooks.
- `Session.Read_MultiPlayer_Settings()` and `Write_MultiPlayer_Settings()` store settings in `RA95.INI`.
- scenario/rules loading still branches on multiplayer game types in places such as `SCENARIO.CPP`.

The UI depends on those configuration hooks in several places:

- `Select_MPlayer_Game()` and the lobby dialogs reuse remembered multiplayer identity/options from `Session`
- the host/join dialogs expect player handle/color/house data to survive across runs
- direct-internet restoration would also need to decide where to persist host/client address-entry fields if those are made user-editable again

### SDL-specific networking UI considerations

The SDL3 port changed the lower-level window/input ownership, but the network dialogs still rely on that plumbing in concrete ways.

Important active seams:

- `CODE/WINSTUB.CPP::Main_Window_Show_Maximized()` is still called from the dormant direct host/join dialogs to re-raise the window
- `CODE/WINSTUB.CPP::Main_Window_Handle_Focus_Change(bool focused)` updates `GameInFocus`, restores surfaces, and intentionally ignores post-bootstrap focus loss
- `GameInFocus` and `BootstrapFocusSeen` still participate in focus-wait logic in startup and the dormant direct-internet dialogs
- `AllSurfaces.SurfacesRestored` is checked in `MPLAYER.CPP`, `NETDLG.CPP`, `SENDFILE.CPP`, and other dialogs to force redraw after focus/surface restoration

Practical consequence:

- the old networking dialogs still expect modal polling loops plus explicit redraw-on-focus-return behavior
- the active SDL path supports that pattern, but it is a compatibility bridge, not a newly designed UI architecture

This matters for future networking UI work because new dialogs should follow the existing SDL-native window/focus helpers instead of reintroducing fake Win32 show/focus/message behavior.

### Gameplay/statistics hooks that still care about network mode

Several gameplay files still branch on `Session.Type == GAME_INTERNET` for internet-specific counters/statistics:

- `BUILDING.CPP`
- `CELL.CPP`
- `TECHNO.CPP`
- related score/stat tracking paths

Those do not re-enable internet play by themselves, but they show that internet mode was not a trivial UI switch.

## What is actually available today

### Fully available now

- LAN UDP multiplayer transport
- LAN UDP front-end path from the main menu through the multiplayer chooser
- LAN UDP join/new-game lobby dialogs
- reconnect/wait dialog coverage during gameplay
- scenario-transfer progress UI
- lobby discovery on the supported UDP path
- private peer connections
- lockstep gameplay packet flow
- scenario transfer
- multiplayer save/load state
- multiplayer settings persistence
- in-game multiplayer message flow

### Present but not fully re-enabled

- direct internet TCP/IP manager
- direct host/join helper dialogs
- direct-internet-specific reconnect/UI cases in shared dialog code
- internet-specific statistics plumbing

### Present only as archival/dormant code

- Westwood Online UI and integration files
- WSPROTO/WSPUDP helper transport headers
- TEN and MPath managers
- older direct internet menu entry points in inactive pre-`FIXIT_VERSION_3` UI branches
- legacy real-mode/IPX-era structural baggage in UDP-related headers/comments

## What it would take to fully re-enable the remaining networking paths

### 1. LAN UDP

Scope: already re-enabled.

The current supported port already has the LAN UDP path active. Remaining work here is not "bring-up" so much as:

- runtime validation with multiple real machines
- case-sensitive asset/scenario verification during join
- compatibility testing with bridge/broadcast behavior on modern networks
- bug-fixing in any path only exercised by real multiplayer sessions
- UI-polish validation across host/join/reconnect/file-transfer loops on real SDL builds

This is a testing and stabilization problem, not a missing-feature problem.

### 2. Direct internet head-to-head

Scope: moderate.

This is the most realistic dormant path to revive because the code still exists in meaningful form.

Required work:

1. Re-expose a front-end path for `GAME_INTERNET`.
2. Add or restore a UI for host/client choice plus IP/port entry.
3. Audit `INIT.CPP` so `GAME_INTERNET` reaches the supported `Winsock` flow instead of the dead WOL-only branch.
4. Re-test `TcpipManagerClass` end-to-end on Linux.
5. Decide what to do with obsolete stats reporting and old service addresses in `INTERNET.CPP`.
6. Audit internet-specific gameplay/stat branches to ensure the mode still behaves coherently.
7. Run real two-machine tests through menu, lobby, load, scenario transfer, gameplay, reconnect/loss, and shutdown.

UI-specific observations for that work:

- the best insertion point is probably `Select_MPlayer_Game()` in `CODE/MPLAYER.CPP`, not the main menu
- the compiled `Net_Fake_New_Dialog()` / `Net_Fake_Join_Dialog()` pair provides a starting point for host/join UI, but not an address-entry screen
- `Server_Remote_Connect()` / `Client_Remote_Connect()` are compiled, so much of the dialog orchestration still exists
- the main missing user-facing piece is a supported way to choose host vs client and populate `PlanetWestwoodIPAddress`, `PlanetWestwoodPortNumber`, and `PlanetWestwoodIsHost`
- the shared reconnect dialog and portions of the gameplay/lobby UI already understand `GAME_INTERNET`, which reduces the amount of UI surface that would need to be created from scratch

Main files likely touched:

- `CODE/MPLAYER.CPP`
- `CODE/INIT.CPP`
- `CODE/NETDLG.CPP`
- `CODE/TCPIP.CPP`
- `CODE/TCPIP.H`
- `CODE/INTERNET.CPP`
- possibly `CODE/MENUS.CPP` depending on how the mode is surfaced

Risk notes:

- this path depends on code that is no longer exercised in the normal build
- the old "Planet Westwood" assumptions are not valid anymore
- the menu/startup glue is currently pointed at the wrong historical integration

### 3. Full original online-service restoration

Scope: very large, and likely not worthwhile in original form.

To restore Westwood Online-like behavior, the project would need more than just code bring-up:

- a replacement for the deleted WOL API bridge
- replacement service infrastructure
- replacement login/chat/matchmaking/update semantics
- substantial UI-path restoration
- resolution for many old service-side assumptions that no longer hold

This is not a "flip the old code back on" task. It is effectively a new online-service project using the old UI/protocol code only as reference material.

### 4. TEN / MPath restoration

Scope: very large and likely lower value than a new backend.

Because the original external services/integration DLLs are gone or unsupported, reviving these paths is likely more work than adding a clean new backend against the still-live game/session seam.

## What it would take to add a new network backend

### Recommended seam

The best seam is still the original manager/connection abstraction:

- `ConnManClass` is the interface the game loop wants.
- `ConnectionClass` already contains ACK/retry/queue behavior.
- `Session`, `NETDLG`, and `QUEUE` already know how to drive a connection manager.

That means a new backend does not need to rewrite gameplay lockstep logic if it fits this model.

### Smallest-disruption approach

The lowest-risk strategy is to keep the current multiplayer/session protocol intact and replace only the transport beneath it.

In practice that means:

1. implement a new `ConnManClass`-derived manager
2. implement one or more `ConnectionClass`-derived connection types, or intentionally reuse the existing queue/ACK machinery if the backend still benefits from it
3. provide:
   - global/discovery-style messaging
   - per-peer private messaging
   - connection enumeration by ID/index
   - response-time reporting
   - timing controls
4. preserve the packet boundaries and higher-level packet meanings expected by `NETDLG`, `SENDFILE`, `SESSION`, and `QUEUE`

If the new backend keeps that contract, most of the game-side code can stay unchanged.

### Files and systems a new backend must satisfy

At minimum, a new backend needs to satisfy the expectations of:

- `CODE/CONNMGR.H`
- `CODE/CONNECT.H`
- `CODE/SESSION.*`
- `CODE/QUEUE.CPP`
- `CODE/NETDLG.CPP`
- `CODE/SENDFILE.CPP`
- `CODE/GLOBALS.CPP`

The backend also needs a startup and selection story:

- a `GameType` decision
- menu/dialog exposure
- initialization/shutdown path
- global singleton or equivalent lifecycle consistent with the current codebase

It also needs to fit the current UI layering:

- main menu entry through `Select_Game()`
- multiplayer-mode selection through `Select_MPlayer_Game()` or a comparable new chooser
- lobby/setup/reconnect/file-transfer surfaces that preserve the existing modal-dialog expectations
- SDL-native focus/window behavior through `WINSTUB.CPP` helpers and the existing redraw/focus conventions

### Likely implementation options

#### Option A: Add a new `ConnManClass` backend alongside UDP

Best when:

- the transport is materially different from the current UDP model
- discovery/addressing is not naturally representable as `UDPAddressClass`
- you want a clean split between old LAN UDP and the new transport

Work involved:

- new manager class
- new per-peer connection class or adapted queue path
- new address container or compatibility mapping
- `Session.Type` integration
- `INIT.CPP` / `MPLAYER.CPP` / `NETDLG.CPP` wiring
- test coverage across lobby plus gameplay

#### Option B: Replace or extend the transport underneath the current UDP-named classes

Best when:

- the new backend can still look like broadcast/global plus private peer messaging
- you want to minimize game-layer changes

Work involved:

- modify `UDPConnClass`, `UDPGlobalConnClass`, `UDPAddressClass`, and `UDPManagerClass`
- preserve their public API and existing packet contracts
- reinterpret addressing under the hood as needed

This is often the shortest path if the transport still behaves like packet-based peer networking.

### Design constraints to preserve

A new backend has to respect several historical constraints that are still real in this codebase:

- fixed packet sizes and packet-queue assumptions
- deterministic lockstep timing
- exact-width network/session fields
- save/load persistence of multiplayer state
- scenario/file transfer behavior
- the difference between global discovery traffic and private gameplay traffic
- case-sensitive filesystem behavior around scenarios and assets

Also note:

- the codebase still contains many original 32-bit assumptions, so new transport code should continue using exact-width integer types deliberately
- the active Windows/Linux path now uses `CODE/SOCKETS.H` plus `CODE/SOCKETS_WINDOWS.CPP` / `CODE/SOCKETS_LINUX.CPP` as the repo-owned thin socket seam; any new backend should either use that style of seam or introduce a comparably explicit one
- reviving `PacketTransport`, WOL-only hooks, or other dead branches is likely higher risk than building on the active `ConnManClass` path

## Effort comparison

In rough engineering-scope terms:

| Goal | Relative scope | Why |
|---|---|---|
| Stabilize current LAN UDP | Small | Core path is already active; this is mostly multiplayer testing and bug-fixing. |
| Re-expose direct internet head-to-head | Medium | Core manager code exists, but front-end wiring and service assumptions need repair. |
| Add a clean new backend at `ConnManClass` seam | Medium to large | Good architecture seam exists, but UI/startup/testing work is still substantial. |
| Restore WOL/TEN/MPath as originally shipped | Very large | Missing bridges/services and obsolete assumptions make this closer to a new subsystem. |

## Recommended next steps

If the goal is practical multiplayer progress rather than archival completeness:

1. Treat LAN UDP as the supported baseline and test it thoroughly on real machines.
2. Decide whether "internet multiplayer" means:
   - direct IP/head-to-head only, or
   - a full service-backed online flow.
3. If direct IP is enough, revive `GAME_INTERNET` through the existing `TcpipManagerClass` path rather than touching WOL code.
4. If a new backend is preferred, build it against `ConnManClass` and keep `Session`/`QUEUE`/`NETDLG` packet semantics intact.

If the goal is historical completeness instead, the repo should treat WOL/TEN/MPath as reference material, not as a nearly-finished feature.
