# Westwood Online (WOL) Inventory and Reimplementation Guide

This document inventories the dormant Westwood Online code still present in this tree, reconstructs the original feature flow from the UI and service/control side, shows approximate UI layouts, and outlines a practical path to rebuild a similar feature on a modern backend while keeping the in-game UI close to the legacy version.

## Current status in this repository

- The WOL front-end code is still present, but it is effectively deactivated.
- `CMakeLists.txt` still glob-loads `CODE/*.CPP`, so the WOL translation units remain part of the project source set.
- The feature is compile-gated by `WOLAPI_INTEGRATION`, and the current tree does not define that macro anywhere in the active build.
- The old bridge object (`WolapiObject`) is referenced from many files, but its implementation/type definition is not present in the repository anymore.
- The practical result is: the old WOL UI and glue code survive as archival reference material, but the backend bridge/service implementation does not.

## What WOL actually was in this codebase

WOL was **not** a second complete gameplay networking stack. In this tree it is best understood as a **service-side control plane** wrapped around the normal internet multiplayer game flow:

1. **Menu/UI layer**: login, saved nicknames, chat, channels, lobbies, game creation, game setup, patch/download prompts, options, ladder/rank views.
2. **WOL service/control layer**: account login, channel browsing, user lists, page/find, game room membership, host-authoritative pre-game setup messages, post-game stats submission.
3. **Existing gameplay transport layer**: once the match actually starts, the code hands off to the normal `GAME_INTERNET` session/network code, using host/player addresses and the normal packet/session machinery.

That distinction matters for any replacement: a modern WOL-like feature can preserve the legacy UI and pre-game flow without cloning the gameplay lockstep code.

## Inventory scope and search method

The file lists below were built from:

- direct WOL file names (`WOL_*.CPP`, `WOL*.H`, `WOLEDIT.*`);
- direct WOL symbols such as `WOLAPI_INTEGRATION`, `TXT_WOL_*`, `WolapiObject`, `PlanetWestwood*`, `WOL_GAMEOPT_*`, and `CREATEGAMEINFO`;
- manual tracing of menu entry points, session handoff, in-game maintenance, and post-game reporting.

## 1. File inventory

### 1.1 Core WOL implementation files

These files are the heart of the dormant feature.

| File | Role |
| --- | --- |
| `CODE/WOL_MAIN.CPP` | Top-level orchestrator. Creates/owns `pWolapi`, enters login, chat, and game setup, and returns to the game/session flow. |
| `CODE/WOL_LOGN.CPP` | Login dialog, saved nickname list, password entry, save/delete nickname handling, registration trigger, and patch-server decision path. |
| `CODE/WOL_CHAT.CPP` | Main chat/lobby/game browser UI. Manages channel levels, chat log, user list, toolbar actions, create/join/leave flow, and transition into game setup. |
| `CODE/WOL_CGAM.CPP` | Create-game dialog. Lets the player choose player count, tournament/private flags, and RA/Counterstrike/Aftermath game kind. |
| `CODE/WOL_GSUP.CPP` | Main host/guest game setup dialog and protocol logic. Handles scenario tabs, player slots, color/house ownership, parameter synchronization, ready/start negotiation, and game handoff. |
| `CODE/WOL_GSUP.H` | Public definitions for the WOL game setup dialog, result enum, and game parameter container. |
| `CODE/WOL_OPT.CPP` | WOL options dialog for page/find/language/local-lobby/rank display settings. |
| `CODE/WOL_DNLD.CPP` | Patch/download progress dialog. |
| `CODE/WOLSTRNG.CPP` | Main WOL string catalog used by dialogs, buttons, errors, ladder text, draw strings, patch text, and labels. |
| `CODE/WOLSTRNG.H` | WOL string declarations. |
| `CODE/WOLEDIT.CPP` | WOL-specific text edit control support used by the WOL dialogs. |
| `CODE/WOLEDIT.H` | Header for the WOL edit control helpers. |
| `CODE/WOLDEBUG.H` | WOL-specific debug macros/helpers. |

### 1.2 WOL integration and runtime support files

These files are not the primary WOL UI files, but they contain WOL gates, WOL state, or WOL-specific runtime behavior.

| File | Why it is WOL-related |
| --- | --- |
| `CMakeLists.txt` | Explains why WOL code is still present but dormant: sources are globbed in, but `WOLAPI_INTEGRATION` is not defined by the build. |
| `CODE/MPLAYER.CPP` | Adds the WOL button in the multiplayer selector under `WOLAPI_INTEGRATION`. |
| `CODE/INIT.CPP` | Routes `GAME_INTERNET` into `WOL_Main()` when WOL is enabled and performs WOL-aware internet startup/cleanup branches. |
| `CODE/EXTERNS.H` | Declares `PlanetWestwoodIPAddress`, `PlanetWestwoodPortNumber`, `PlanetWestwoodIsHost`, and `PlanetWestwoodGameID`. |
| `CODE/INTERNET.CPP` | Defines the `PlanetWestwood*` globals and command-line/session helpers reused by internet multiplayer and the old WOL handoff path. |
| `CODE/INTERNET.H` | Declares the internet globals and helpers. |
| `CODE/SESSION.H` | Gives `GAME_INTERNET` its mode identity and contains the WOL-specific `ScenarioInfo.ShortFileName` size split. |
| `CODE/NETDLG.CPP` | Contains WOL-specific internet dialog branches, WOL-aware cancel/forfeit messaging, and the shared internet match setup/startup code. |
| `CODE/TCPIP.CPP` | Uses `PlanetWestwoodIPAddress` and `PlanetWestwoodPortNumber`; part of the internet transport seam WOL eventually handed off into. |
| `CODE/TCPIP.H` | Declares the same WOL-era internet globals and conditional behavior. |
| `CODE/SENDFILE.CPP` | Pumps WOL messages during scenario file transfer so the WOL layer stays alive while the map transfer dialog is open. |
| `CODE/STATS.CPP` | Sends WOL-specific end-game statistics using `PlanetWestwoodGameID`, `PlanetWestwoodStartTime`, and `pWolapi->pNetUtil->RequestGameresSend(...)`. |
| `CODE/QUEUE.CPP` | Adds WOL-specific reconnect/tournament/disconnect-ping behavior in the live match loop. |
| `CODE/CONQUER.CPP` | Supports WOL page response from inside the game, pumps WOL while in match, and surfaces WOL connection-loss messaging. |
| `CODE/EVENT.CPP` | Implements WOL draw proposal/retract event messaging. |
| `CODE/SCENARIO.CPP` | Displays the WOL draw result text and has WOL-specific compile-gated branches. |
| `CODE/GAMEDLG.CPP` | Adds a WOL options button to the in-game options dialog. |
| `CODE/GOPTIONS.CPP` | Uses WOL draw button text and draw confirmation text in the game options flow. |
| `CODE/HOUSE.H` | Carries WOL compile-gated state used by the game/player runtime. |

### 1.3 WOL UI helper/support files

These files are not conceptually "the WOL feature", but they were extended or compile-gated to support the WOL dialogs.

| File | Why it is WOL-related |
| --- | --- |
| `CODE/BIGCHECK.CPP` | WOL-gated UI helper for the large checkbox widgets used in WOL dialogs. |
| `CODE/BIGCHECK.H` | Header for the same large checkbox support. |
| `CODE/DIALOG.CPP` | WOL-gated dialog behavior support used by the WOL UI. |
| `CODE/DROP.CPP` | WOL-gated dropdown/list helper support. |
| `CODE/DROP.H` | Header for the WOL-related dropdown/list support. |
| `CODE/ICONLIST.CPP` | WOL-gated icon list support used heavily by the login/chat/player/channel lists. |
| `CODE/ICONLIST.H` | Header for the icon list support. |
| `CODE/SEDITDLG.CPP` | WOL-gated string edit dialog support. |
| `CODE/SEDITDLG.H` | Header for the same dialog support. |
| `CODE/TEXTBTN.CPP` | WOL-gated text button behavior. |
| `CODE/TOOLTIP.CPP` | WOL-gated tooltip support for the chat/lobby toolbar. |
| `CODE/TOOLTIP.H` | Header for the tooltip support. |
| `CODE/STARTUP.CPP` | Contains WOL compile-gated startup references. |

### 1.4 Incidental UI/string consumers

These files are not part of the old service implementation, but they consume WOL-named strings or WOL-themed text.

| File | Why it appears in the inventory |
| --- | --- |
| `CODE/MENUS.CPP` | Uses `TXT_WOL_CS_MISSIONS` / `TXT_WOL_AM_MISSIONS` for expansion menu labels. |
| `CODE/EXPAND.CPP` | Uses the same WOL-named expansion mission caption strings. |

### 1.5 Missing/removed external pieces

These are important because they explain why the feature is archival instead of live.

| Missing piece | Impact |
| --- | --- |
| `WolapiObject` implementation/type definition | The current tree still references the object widely, but the bridge itself is gone. |
| Original WOL service/backend | Login, channel, page, game room, ladder, patch, and stats traffic cannot function without a replacement service. |
| Old WOL transport helper sources (`WSPROTO.CPP`, `_WSPROTO.CPP`, `WSPUDP.CPP`) | These are explicitly excluded in `CMakeLists.txt`, reinforcing that the original integration is not expected to build today. |
| `PassEdit.h` included by `CODE/WOL_LOGN.CPP` | This header is not present in the repository, another sign that the old WOL integration surface is incomplete. |

## 2. High-level architecture

### 2.1 Top-level flow

The original WOL flow in this tree is:

1. `Select_MPlayer_Game()` in `CODE/MPLAYER.CPP` exposes a WOL button when `WOLAPI_INTEGRATION` is enabled.
2. That button returns `GAME_INTERNET`, just like the old TCP internet path.
3. `CODE/INIT.CPP` detects the WOL-enabled internet case and calls `WOL_Main()`.
4. `WOL_Main()` performs WOL initialization and enters:
   1. `WOL_Login_Dialog(...)`
   2. `WOL_Chat_Dialog(...)`
   3. `WOL_GameSetupDialog(...)` when a game is created or joined
5. Once setup is complete, the code fills normal `Session` state and hands off into the existing internet gameplay network stack.
6. During the match, several shared gameplay files keep the WOL service alive for page/draw/stats/reconnect handling.
7. At the end, `STATS.CPP` reports the result back to the WOL service.

### 2.2 The important split: control plane vs gameplay plane

The code clearly separates two layers:

#### WOL control plane

- login and saved nicknames
- channel list and lobby browser
- user list and presence
- private/public chat
- game room creation/joining
- host-authoritative pre-game option negotiation
- paging/find/moderation/tooling
- post-game stats submission

#### Gameplay plane

- actual multiplayer packet transport
- `Session` state
- host/guest game startup
- scenario transfer
- live in-game events and lockstep traffic

WOL mainly supplied the first layer and then drove the second layer.

## 3. UI-side behavior

### 3.1 Multiplayer entry

The multiplayer protocol chooser adds a WOL button next to the existing network choices. Both the legacy TCP internet option and WOL returned `GAME_INTERNET`; the later startup code decided whether that meant "standalone internet transport" or "enter the WOL front-end first".

### 3.2 Login dialog

`CODE/WOL_LOGN.CPP` builds a compact login dialog with:

- nickname field
- password field
- saved nickname list
- save checkbox
- delete entry button
- connect/cancel controls

Important behaviors:

- Saved nicknames are read and written through the WOL bridge (`GetNick` / `SetNick`).
- If the user has no saved nick, the flow can branch into registration.
- If the server discovery/login step reports a patch condition, the code branches into the patch download flow before re-entering login.

### 3.3 Chat/lobby/game browser

`CODE/WOL_CHAT.CPP` is the main front-end shell. It contains:

- a large chat transcript pane
- a channel list
- a user list
- an input field for outgoing chat
- bottom action buttons (`Back`, `Join`, `New`, `Action`)
- a top toolbar for disconnect, leave, refresh, squelch, ban, kick, page/find, options, ladder, and help
- lobby level/rank toggle controls

The dialog acts like a state machine over several logical "levels":

1. top
2. official chat
3. user chat
4. game categories
5. lobbies
6. inside a lobby
7. inside a chat channel

Behavior notes:

- selecting a channel changes the current level and repopulates the lists;
- joining private channels can prompt for a password;
- creating a game is only permitted from the correct Red Alert game lobby level;
- ladder/rank views are only meaningful in the lobby-oriented levels.

### 3.4 Create-game dialog

`CODE/WOL_CGAM.CPP` is a small modal dialog launched from the chat/lobby screen. It collects:

- player count
- tournament flag
- private-game flag
- game kind: Red Alert / Counterstrike / Aftermath

Behavior notes:

- tournament games force the player count down to two;
- Counterstrike and Aftermath options are disabled when the expansions are not installed.

### 3.5 Game setup dialog

`CODE/WOL_GSUP.CPP` is the most important file in the feature.

The dialog contains:

- scenario tabs: RA / CS / AM / User
- scenario list
- player slots
- ready/accept/start controls
- map/game-option controls
- color and house ownership controls
- host/guest status messaging

The source contains a long block comment describing the protocol. That comment matches the implementation: the **host owns the authoritative parameter set**, and guests largely request/acknowledge changes rather than editing everything directly.

Host-side behavior:

- chooses the scenario
- maintains the current parameter generation ID
- serializes and broadcasts authoritative parameters
- assigns or reassigns player colors
- accepts house/color requests
- tracks guest accept/ready state
- starts or cancels the countdown/start process

Guest-side behavior:

- requests house/color changes
- receives parameter snapshots from the host
- acknowledges a specific parameter generation
- answers the start request with either "ready" or "ready but I need the scenario file"

### 3.6 Patch download dialog

`CODE/WOL_DNLD.CPP` is a simple progress/status box. It exists because the old service could refuse login until the local build was patched.

### 3.7 In-game WOL UI

WOL does not disappear when the match starts. Shared runtime code exposes several WOL-linked in-game behaviors:

- an in-game WOL options button (`CODE/GAMEDLG.CPP`)
- draw proposal / accept / retract UI text (`CODE/GOPTIONS.CPP`, `CODE/EVENT.CPP`, `CODE/SCENARIO.CPP`)
- page response from inside the match (`CODE/CONQUER.CPP`)
- WOL disconnect/connection-loss messaging (`CODE/CONQUER.CPP`, `CODE/QUEUE.CPP`)

## 4. Network/service-side behavior

### 4.1 Login and account state

From the game's point of view, the WOL bridge provided several service-like capabilities:

- chat server lookup
- login submission
- saved nick storage
- registration entry point
- patch requirement reporting
- connection state flags

The game UI does not talk directly to raw sockets here. It talks to the WOL bridge object and its chat/net utility sub-objects.

### 4.2 Channel and lobby model

The service model exposed at least these concepts:

- channels
- users
- public/private chat rooms
- game rooms/lobbies
- presence/user lists
- moderation actions (squelch, ban, kick)
- paging/find

The chat UI navigates these service objects and reuses the same left/right list presentation for both normal chat channels and game-oriented channels.

### 4.3 Game creation and game room semantics

Creating a game does not immediately start a match. It creates a WOL-side game room with metadata such as:

- game kind (RA / CS / AM)
- player limit
- tournament flag
- private/public state

Joining that room then transitions players into the game setup protocol handled by `WOL_GSUP.CPP`.

### 4.4 Game setup protocol

The pre-game room protocol is the most important service behavior to replicate.

The code uses a set of WOL-specific game option/control messages, including:

- request color
- inform color
- request house
- inform house
- inform params
- accept params
- start
- cancel start
- go
- "new guest player info" bulk sync

The protocol is host-authoritative:

1. host picks/changes scenario and options;
2. host serializes the authoritative option blob with `SendParams()`;
3. guest parses it with `AcceptParams()`;
4. guest replies with acceptance of a specific param generation;
5. host tracks whether every guest has accepted;
6. host initiates start;
7. each guest either confirms readiness or reports that it needs the scenario file;
8. when all requirements are satisfied, the host sends `GO`;
9. the dialog hands off into the normal network game.

### 4.5 Parameter serialization

`SendParams()` in `CODE/WOL_GSUP.CPP` serializes a large space-delimited blob containing the match-defining data, including:

- scenario description
- scenario file length
- short filename
- file digest
- official/unofficial flag
- credits
- bases / ore / crates
- build level
- unit count
- AI player count
- random seed
- rules/special flags
- game speed
- version
- Aftermath-unit flag
- slow-unit-build flag
- `RuleINI.Get_Unique_ID()` for rules mismatch detection

This tells us two things:

1. the old service protocol was fairly "application-level" and texty, not a hidden opaque binary blob;
2. any replacement backend must preserve the same logical payload, even if the new wire format changes.

### 4.6 Scenario download

If the guest does not have the selected scenario locally, the setup flow can still continue:

1. the host advertises the scenario metadata;
2. the guest checks for a local match with filename/length/digest/official flag;
3. if missing, the guest reports that it needs the file;
4. on game launch, the code enters the normal file-transfer path;
5. `CODE/SENDFILE.CPP` keeps pumping WOL messages during that transfer dialog.

This means the old WOL feature used the service layer to negotiate that the file was needed, but the actual file transfer used the game's shared networking/file-transfer machinery.

### 4.7 Match handoff into gameplay

After `GO`, the setup dialog populates normal runtime state:

- `Session.Handle`
- `Session.GameName`
- `Session.Players`
- per-player IP data
- session/network timing values such as `MaxAhead`, `FrameSendRate`, and `CommProtocol`

Then it calls the normal network initialization path. This is the clearest sign that WOL was a front-end/control wrapper around the shared multiplayer backend, not a separate in-game networking implementation.

### 4.8 In-game WOL behaviors

Once the match is live, WOL still contributes:

- page response handling inside the match
- draw proposal/retract messaging
- tournament disconnect-ping/reconnect logic
- server connection-lost detection and user messaging
- end-game stats reporting

### 4.9 End-game statistics

`CODE/STATS.CPP` packages a WOL-specific result payload. It includes fields such as:

- game ID
- start time
- initial player count
- remaining player count
- tournament flag
- game kind / expansion context

The payload is then sent to the game result server through the WOL net utility object.

## 5. Approximate UI mockups

These mockups are approximate reconstructions from the control layout code and the WOL string tables. They are intended to preserve the visual structure and affordances, not exact pixel-perfect coordinates.

### 5.1 Multiplayer chooser

```text
+--------------------------------------+
| Select Multiplayer Game              |
|--------------------------------------|
| [ Skirmish ]                         |
| [ Network ]                          |
| [ Internet ]                         |
| [ Westwood Online ]                  |
|                                      |
|                        [ Cancel ]    |
+--------------------------------------+
```

### 5.2 Login dialog

```text
+--------------------------------------------------------------+
| Westwood Online                                              |
|--------------------------------------------------------------|
| Nickname: [____________________]   Saved nicknames           |
| Password: [____________________]   > PlayerOne               |
| [x] Save password                  | PlayerTwo               |
|                                    | ClanNick                |
| [ Delete ] [ Register ]                                      |
|                                            [ Connect ] [Cancel]|
+--------------------------------------------------------------+
```

### 5.3 Main chat / lobby browser

```text
+--------------------------------------------------------------------------------+
| Disconnect  Leave  Refresh  Squelch  Ban  Kick  Page/Find  Options  Ladder Help|
|--------------------------------------------------------------------------------|
| Chat / status log                         | Channels / game rooms               |
|-------------------------------------------|------------------------------------|
| <server text>                             | Official Chat                      |
| <channel text>                            | User Chat                          |
| <player messages>                         | Red Alert Games                    |
|                                           | Counterstrike Games                |
|                                           | Aftermath Games                    |
|                                           |                                    |
|-------------------------------------------|------------------------------------|
| Message: [______________________________________________________________]       |
|--------------------------------------------------------------------------------|
| Users in current view                  | [ Back ] [ Join ] [ New ] [ Action ] |
|----------------------------------------|---------------------------------------|
| HostNick                               | Rank view: [ RA ] [ AM ]             |
| GuestNick                              |                                       |
| Moderator                              |                                       |
+--------------------------------------------------------------------------------+
```

### 5.4 Create-game dialog

```text
+---------------------------------------------+
| Create Game                                 |
|---------------------------------------------|
| Players:      [ 2 <====o====> 8 ]           |
| [ ] Tournament                              |
| [ ] Private                                 |
|                                             |
| Game type:                                  |
| (o) Red Alert                               |
| ( ) Counterstrike                           |
| ( ) Aftermath                               |
|                           [ OK ] [ Cancel ] |
+---------------------------------------------+
```

### 5.5 Game setup dialog (host view)

```text
+--------------------------------------------------------------------------------+
| Game Setup                                                                     |
|--------------------------------------------------------------------------------|
| Tabs: [ RA ] [ CS ] [ AM ] [ User ]                                            |
|--------------------------------------------------------------------------------|
| Scenario list                      | Players                                    |
|------------------------------------|--------------------------------------------|
| Official Map 1                     | 1. HostNick   Color: Red   House: USSR     |
| Official Map 2                     | 2. GuestNick  Color: Blue  House: Greece   |
| User Map A                         | 3. ----                                      |
| User Map B                         | 4. ----                                      |
|------------------------------------|--------------------------------------------|
| Credits [____]   Bases [x]   Ore [x]   Crates [ ]   Units [____]               |
| Build level [__]   AI [__]   Speed [__]   Tournament [ ]                       |
|--------------------------------------------------------------------------------|
| Host status: all guests accepted params.                                       |
|                                        [ Accept ] [ Start ] [ Cancel ]          |
+--------------------------------------------------------------------------------+
```

### 5.6 Game setup dialog (guest view)

```text
+--------------------------------------------------------------------------------+
| Game Setup                                                                     |
|--------------------------------------------------------------------------------|
| Tabs: [ RA ] [ CS ] [ AM ] [ User ]   (scenario/options are read-only)         |
|--------------------------------------------------------------------------------|
| Scenario: Official Map 2                                                       |
| Players:                                                                       |
|   1. HostNick   Color: Red   House: USSR                                       |
|   2. GuestNick  Color: Blue  House: Greece   [ Request next color ]            |
|--------------------------------------------------------------------------------|
| Current settings received from host:                                           |
|   Credits 10000  Bases On  Ore On  Crates Off  Units 10  Speed Normal          |
|--------------------------------------------------------------------------------|
| Status: Waiting for host start...                                              |
|                                          [ Accept ] [ Ready ] [ Cancel ]        |
+--------------------------------------------------------------------------------+
```

### 5.7 Download / patch dialog

```text
+------------------------------------------------------+
| Downloading Update / Scenario                        |
|------------------------------------------------------|
| Status: Contacting server...                         |
|                                                      |
| [##########################------------]  68%        |
|                                                      |
| Bytes: 348160 / 512000                               |
|                                    [ Cancel ]        |
+------------------------------------------------------+
```

### 5.8 In-game WOL options / draw/page affordances

```text
+----------------------------------------------+
| Game Options                                 |
|----------------------------------------------|
| [ Propose Draw ] / [ Accept Draw ]           |
| [ WOL Options ]                              |
| [ Surrender ]                                |
| [ Resume ]                                   |
+----------------------------------------------+
```

## 6. Practical reimplementation guide

### 6.1 Goals for a replacement

A modern replacement should aim to preserve:

- the overall menu flow;
- the dialog layouts and control groupings;
- the host-authoritative game setup behavior;
- saved player identity handling;
- chat/channel/game-room browsing semantics;
- scenario-download and post-game-reporting affordances.

It should **not** try to resurrect the original proprietary WOL backend or DLL/COM-style bridge.

### 6.2 Recommended architecture

The cleanest replacement is a three-layer model:

1. **Legacy-close UI layer**
   - preserve the existing menu/login/chat/create/setup screens;
   - keep the string vocabulary and screen structure close to `WOLSTRNG.*` and the existing dialog code;
   - keep the current "levels" and button groupings so the UX stays recognizable.
2. **Modern service/control client**
   - replace `WolapiObject` with a new in-process service client;
   - use an explicit stateful API for login, channel lists, user presence, room membership, paging, options, and stats upload;
   - run it on top of a modern transport such as WebSocket over TLS or another persistent message channel.
3. **Existing gameplay transport**
   - keep the actual multiplayer match handoff feeding into the existing `GAME_INTERNET`/`Session`/packet flow;
   - only change this layer if direct peer connect, NAT traversal, or relays truly require it.

### 6.3 Recommended seam: replace `pWolapi`, not the entire UI first

The fastest route to a working legacy-close feature is to treat the missing WOL bridge as the seam:

- keep the existing dialog code as a reference, and reuse it where practical;
- define a new replacement client object with the service concepts the dialogs expect;
- map old concepts (`Channel`, `User`, `GameInfoCurrent`, `pChat`, `pNetUtil`, connection flags) onto a new internal API;
- avoid dragging old DLL registration logic or Windows-specific glue back into the tree.

In other words: emulate the **behavioral contract** of `pWolapi`, not its historical binary integration model.

### 6.4 Backend responsibilities to recreate

At minimum, the replacement service needs:

| Legacy function | Modern equivalent |
| --- | --- |
| Login / saved nick validation | Account login and token/session management |
| Patch requirement check | Version compatibility API |
| Official/user/game channel lists | Lobby directory service |
| Channel chat and presence | Chat room service |
| Page/find | Direct messages / user lookup |
| Create/join private/public game room | Matchmaking / room service |
| Host-authoritative setup messages | Room-scoped control messages |
| Game ID / start time allocation | Match metadata service |
| End-game stat upload | Result reporting service |
| Ladder/rank display | Read-only stats/ranking endpoint |

### 6.5 Control-plane transport recommendation

For the service/control side, use a persistent message transport:

- **Recommended:** WebSocket over TLS for login/session continuity, room membership updates, presence, chat, and room-control messages.
- **Alternative:** TCP with a compact binary or JSON protocol if WebSocket is undesirable.

Why this fits the old design well:

- the old WOL pre-game flow is mostly room/chat/control traffic;
- it benefits from ordered, persistent, low-latency message delivery;
- it does not need the exact same transport as the in-match gameplay packets.

### 6.6 Gameplay transport recommendation

For the actual match, the easiest compatibility-preserving plan is:

1. keep the current internet gameplay handoff model;
2. let the replacement service distribute player/host addressing and match metadata;
3. keep using the existing gameplay packet/session code once the room reaches `GO`.

That keeps the most fragile gameplay code untouched and limits the new backend to the part WOL already controlled historically.

If NAT traversal or anti-cheat requirements later force a different gameplay transport, change it at the existing transport seam (`Winsock` / internet session setup), not inside the WOL-style UI.

### 6.7 Room/setup protocol to preserve

Even if the wire format changes, the following semantics should stay the same:

- one host is authoritative for scenario and options;
- guests can request limited changes (color, house, ready state);
- every parameter update has a generation/acceptance concept;
- start is a multi-step handshake, not a blind immediate launch;
- missing scenarios are detected before the game fully begins;
- the game receives a stable match ID/start time for later stats reporting.

Those semantics are the real behavior contract of `WOL_GSUP.CPP`.

### 6.8 Scenario distribution options

There are two plausible implementations:

1. **Closest to the old design**
   - host advertises scenario metadata;
   - guests request the file if missing;
   - host serves the file directly during the pre-game transition.
2. **Operationally safer modern design**
   - host uploads a custom scenario to the service when the room is created;
   - guests download from the service/CDN, while the UI still shows the same progress dialog;
   - official maps continue to use local data validation only.

For a first stable replacement, the second option is usually safer and simpler to debug while still preserving the legacy UI.

### 6.9 Suggested implementation phases

1. **Freeze the reference**
   - keep this document and `WOLSTRNG.*` as the behavioral/UI reference;
   - do not try to "clean up" the legacy flow before it is understood.
2. **Define a replacement service facade**
   - new client object replacing `pWolapi`;
   - explicit events for login, room list, user list, chat text, ready/start, and stats.
3. **Re-enable the UI path behind a new compile/runtime switch**
   - menu entry
   - login dialog
   - chat/lobby dialog
   - create-game dialog
4. **Implement room/game setup semantics**
   - authoritative host
   - param snapshot/accept cycle
   - ready/start/cancel/go
   - scenario metadata and mismatch detection
5. **Reconnect the gameplay handoff**
   - fill normal `Session` state exactly once the room reaches `GO`;
   - reuse the existing gameplay startup/network code.
6. **Restore live-match extras**
   - draw proposal/retract
   - page reply
   - connection-loss messages
   - tournament-specific disconnect handling if still desired
7. **Restore result reporting and ranking**
   - match ID/start time
   - end-game stats upload
   - ladder/rank display

### 6.10 UI-closeness checklist

If the goal is "modern backend, old UI feel", keep these visible details:

- same button names and rough button placement;
- same three-pane chat/channel/user layout;
- same create-game modal choices;
- same four-tab scenario grouping in setup;
- same host-vs-guest authority model in setup;
- same status/progress wording wherever practical, using `WOLSTRNG.*` as the source;
- same in-game draw/page affordances.

### 6.11 Things not to carry forward blindly

- DLL registration/re-registration code in `WOL_MAIN.CPP`
- proprietary Westwood service assumptions
- missing external headers/objects
- Windows-specific bridge assumptions
- any idea that the WOL service must also be the gameplay packet transport

Those were implementation details of the original integration, not requirements of the user-facing feature.

## 7. Key conclusions

- The WOL code in this repository is still rich enough to reconstruct the original user flow and service contract.
- The most important file is `CODE/WOL_GSUP.CPP`; it defines the actual behavioral contract of the pre-game room/setup process.
- WOL was primarily a **login/chat/lobby/setup/stats control plane** over the shared internet gameplay network path.
- A modern replacement should keep the UI and setup semantics close, replace `pWolapi` with a new service client, and reuse the existing gameplay transport handoff as long as possible.
