/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	Shared wire protocol header for the Westwood Online replacement
**	WebSocket transport. Mirrors server/src/proto.zig. Both sides must
**	agree on this layout.
**
**	Framing (little-endian, one frame per WebSocket binary message):
**		uint16_t opcode;
**		uint16_t flags;
**		uint32_t payload_len;
**		uint8_t  payload[payload_len];
*/
#ifndef WOL_PROTO_H
#define WOL_PROTO_H

#include <stdint.h>
#include <stddef.h>

namespace WolProto {

static constexpr size_t HEADER_SIZE = 8;
static constexpr uint32_t PROTOCOL_VERSION = 1;
static constexpr uint32_t MAX_PAYLOAD = 1u * 1024u * 1024u;

enum Opcode : uint16_t {
	OP_HELLO              = 0x0001,
	OP_WELCOME            = 0x0002,
	OP_ERROR              = 0x0003,

	OP_CHANNEL_LIST       = 0x0010,
	OP_CHANNEL_LIST_REPLY = 0x0011,
	OP_CHANNEL_JOIN       = 0x0012,
	OP_CHANNEL_JOINED     = 0x0013,
	OP_CHANNEL_USER_JOIN  = 0x0014,
	OP_CHANNEL_USER_LEAVE = 0x0015,

	OP_CHAT               = 0x0020,

	OP_GAME_CREATE        = 0x0030,
	OP_GAME_JOIN          = 0x0031,
	OP_GAME_LEAVE         = 0x0032,
	OP_GAME_START         = 0x0033,
	OP_GAME_LIST_REPLY    = 0x0034,
	OP_GAME_MEMBERS       = 0x0035,
	OP_GAME_STARTED       = 0x0036,

	OP_RELAY_BROADCAST    = 0x0100,
	OP_RELAY_PRIVATE      = 0x0101,
	OP_RELAY_FROM         = 0x0102,
	OP_PEER_LEFT          = 0x0103,
};

enum ErrorCode : uint16_t {
	ERR_OK              = 0,
	ERR_BAD_PROTOCOL    = 1,
	ERR_BAD_NICKNAME    = 2,
	ERR_NOT_LOGGED_IN   = 3,
	ERR_NOT_IN_CHANNEL  = 4,
	ERR_NOT_IN_GAME     = 5,
	ERR_ALREADY_IN_GAME = 6,
	ERR_UNKNOWN_TARGET  = 7,
	ERR_GAME_FULL       = 8,
	ERR_GAME_STARTED    = 9,
	ERR_INTERNAL        = 100,
};

inline void Write_U8(uint8_t *&p, uint8_t v) { *p++ = v; }
inline void Write_U16(uint8_t *&p, uint16_t v) {
	p[0] = uint8_t(v & 0xFFu);
	p[1] = uint8_t((v >> 8) & 0xFFu);
	p += 2;
}
inline void Write_U32(uint8_t *&p, uint32_t v) {
	p[0] = uint8_t(v & 0xFFu);
	p[1] = uint8_t((v >> 8) & 0xFFu);
	p[2] = uint8_t((v >> 16) & 0xFFu);
	p[3] = uint8_t((v >> 24) & 0xFFu);
	p += 4;
}

inline uint8_t  Read_U8 (uint8_t const *&p) { return *p++; }
inline uint16_t Read_U16(uint8_t const *&p) {
	uint16_t v = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
	p += 2;
	return v;
}
inline uint32_t Read_U32(uint8_t const *&p) {
	uint32_t v = uint32_t(p[0])
		| (uint32_t(p[1]) << 8)
		| (uint32_t(p[2]) << 16)
		| (uint32_t(p[3]) << 24);
	p += 4;
	return v;
}

/// Encodes the 8-byte frame header into `dest`.
inline void Encode_Header(uint8_t *dest, uint16_t opcode, uint16_t flags, uint32_t payload_len) {
	dest[0] = uint8_t(opcode & 0xFFu);
	dest[1] = uint8_t((opcode >> 8) & 0xFFu);
	dest[2] = uint8_t(flags & 0xFFu);
	dest[3] = uint8_t((flags >> 8) & 0xFFu);
	dest[4] = uint8_t(payload_len & 0xFFu);
	dest[5] = uint8_t((payload_len >> 8) & 0xFFu);
	dest[6] = uint8_t((payload_len >> 16) & 0xFFu);
	dest[7] = uint8_t((payload_len >> 24) & 0xFFu);
}

struct ParsedFrame {
	uint16_t opcode;
	uint16_t flags;
	uint8_t const *payload;
	uint32_t payload_len;
};

/// Parses the header of an incoming frame. Returns true on success.
/// Requires buf.len >= HEADER_SIZE + payload_len.
inline bool Parse_Frame(uint8_t const *buf, size_t buf_len, ParsedFrame *out) {
	if (buf_len < HEADER_SIZE) return false;
	uint16_t op = uint16_t(buf[0]) | (uint16_t(buf[1]) << 8);
	uint16_t fl = uint16_t(buf[2]) | (uint16_t(buf[3]) << 8);
	uint32_t ln = uint32_t(buf[4])
		| (uint32_t(buf[5]) << 8)
		| (uint32_t(buf[6]) << 16)
		| (uint32_t(buf[7]) << 24);
	if (ln > MAX_PAYLOAD) return false;
	if (buf_len < HEADER_SIZE + ln) return false;
	out->opcode = op;
	out->flags = fl;
	out->payload = buf + HEADER_SIZE;
	out->payload_len = ln;
	return true;
}

} // namespace WolProto

#endif // WOL_PROTO_H
