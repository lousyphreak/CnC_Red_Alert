# WOL replacement protocol

_Last updated: 2026-04-20_

Canonical reference for the wire protocol spoken between the game
(`CODE/WSCLIENT.*`) and the Zig server (`server/`). Both sides MUST stay
in sync with this document; the source-of-truth constants live in
`server/src/proto.zig` and `CODE/WOL_PROTO.H`.

## Transport

- Plain WebSocket (`ws://`). TLS is out of scope for v1; terminate TLS at
  a reverse proxy if needed.
- Exactly one HTTP server process also serves:
  - `/` → emscripten build output (`*.html`, `*.js`, `*.wasm`, `*.data`,
    `ra-assets-manifest.txt`);
  - `/GameData/...` → files under the configured `GameData/` directory,
    with case-insensitive path resolution (`/gamedata/...` remains accepted
    for compatibility);
  - `/healthz` → plain-text health probe endpoint;
  - `/ws` → the WebSocket upgrade endpoint described here.

## Application framing

Inside every WebSocket *binary* message there is exactly one
application frame:

```
struct WsFrame {
    uint16_t opcode;       // little-endian
    uint16_t flags;        // reserved; always 0 in v1
    uint32_t payload_len;  // bytes that follow, <= 1 MiB
    uint8_t  payload[payload_len];
};
```

All multi-byte integers are **little-endian** (x86 and wasm native).
Strings are length-prefixed UTF-8: `uint16_t len; uint8_t bytes[len];`
and are NOT null-terminated on the wire.

`HEADER_SIZE = 8`, `PROTOCOL_VERSION = 1`, `MAX_PAYLOAD = 1 MiB`.

## Opcodes

| Opcode   | Dir | Name                | Payload                                                   |
|----------|-----|---------------------|-----------------------------------------------------------|
| `0x0001` | C→S | `HELLO`             | `u32 proto_version, str nick` (nick ≤ 32 bytes)           |
| `0x0002` | S→C | `WELCOME`           | `u32 client_id, u32 server_version`                       |
| `0x0003` | S→C | `ERROR`             | `u16 code, str message`                                   |
| `0x0010` | C→S | `CHANNEL_LIST`      | _(empty)_                                                 |
| `0x0011` | S→C | `CHANNEL_LIST_REPLY`| `u16 count, { str name, u16 users } × count`              |
| `0x0012` | C→S | `CHANNEL_JOIN`      | `str name`                                                |
| `0x0013` | S→C | `CHANNEL_JOINED`    | `str name, u16 count, { u32 cid, str nick } × count`      |
| `0x0014` | S→C | `CHANNEL_USER_JOIN` | `u32 cid, str nick`                                       |
| `0x0015` | S→C | `CHANNEL_USER_LEAVE`| `u32 cid`                                                 |
| `0x0020` | C↔S | `CHAT`              | C→S: `u32 target_cid, str text` (0 = broadcast to channel); S→C: `u32 source_cid, u32 target_cid, str text` |
| `0x0030` | C→S | `GAME_CREATE`       | `str name, u8 max_players`                                |
| `0x0031` | C→S | `GAME_JOIN`         | `u32 game_id`                                             |
| `0x0032` | C→S | `GAME_LEAVE`        | _(empty)_                                                 |
| `0x0033` | C→S | `GAME_START`        | _(empty; host only)_                                      |
| `0x0034` | S→C | `GAME_LIST_REPLY`   | `u16 count, { u32 game_id, str name, u32 host_cid, u8 members, u8 max } × count` |
| `0x0035` | S→C | `GAME_MEMBERS`      | `u32 game_id, u16 count, { u32 cid, str nick } × count`   |
| `0x0036` | S→C | `GAME_STARTED`      | `u32 game_id, u16 count, u32 cid × count` (seating order) |
| `0x0100` | C→S | `RELAY_BROADCAST`   | `u8 bytes[]` — opaque gameplay payload                    |
| `0x0101` | C→S | `RELAY_PRIVATE`     | `u32 target_cid, u8 bytes[]`                              |
| `0x0102` | S→C | `RELAY_FROM`        | `u32 source_cid, u8 bytes[]`                              |
| `0x0103` | S→C | `PEER_LEFT`         | `u32 cid`                                                 |

## Message-ordering notes

- On `GAME_CREATE` and `GAME_JOIN`, the server sends `GAME_LIST_REPLY`
  **before** `GAME_MEMBERS` (so every connected client's game-list stays
  fresh before the player sees their own lobby).
- The relay scope is "current game room" for RELAY_* opcodes. Outside a
  game, CHAT is scoped to the currently-joined channel; outside both a
  channel and a game, `ERROR(NOT_IN_CHANNEL)` is returned.

## Error codes (`u16` in `ERROR` frames)

| Code | Meaning                 |
|------|-------------------------|
| 0    | OK (unused on the wire) |
| 1    | Bad protocol version    |
| 2    | Bad nickname            |
| 3    | Not logged in           |
| 4    | Not in a channel        |
| 5    | Not in a game           |
| 6    | Already in a game       |
| 7    | Unknown target cid      |
| 8    | Game full               |
| 9    | Game already started    |
| 100  | Internal error          |

## Versioning

- `PROTOCOL_VERSION` is bumped for any breaking change to the opcode
  table or payload layouts.
- The server rejects a HELLO whose `proto_version` is not exactly its
  supported version with `ERROR(BAD_PROTOCOL)`.
