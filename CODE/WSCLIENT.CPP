/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	Implementation of WSClientClass. Two back-ends share a single public
**	interface:
**
**	- EMSCRIPTEN: uses emscripten_websocket_* from emscripten/websocket.h.
**	  Link with -sUSE_WEBSOCKET=1.
**
**	- NATIVE: raw TCP via CODE/SOCKETS.H with an in-process RFC 6455
**	  implementation (SHA-1 + base64 for the handshake, XOR masking for
**	  client frames). No third-party dependency.
*/

#include "WSCLIENT.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <ctime>

#ifdef __EMSCRIPTEN__
#	include <emscripten/websocket.h>
#else
#	include "SOCKETS.H"
#endif

namespace {

struct QueuedMessage {
	uint8_t *data;
	size_t len;
	QueuedMessage *next;
};

void Push_Message(QueuedMessage **head, QueuedMessage **tail, uint8_t *data, size_t len) {
	QueuedMessage *m = static_cast<QueuedMessage *>(std::malloc(sizeof(QueuedMessage)));
	if (m == nullptr) {
		std::free(data);
		return;
	}
	m->data = data;
	m->len = len;
	m->next = nullptr;
	if (*tail) {
		(*tail)->next = m;
	} else {
		*head = m;
	}
	*tail = m;
}

bool Pop_Message(QueuedMessage **head, QueuedMessage **tail, void **out_data, size_t *out_len) {
	QueuedMessage *m = *head;
	if (m == nullptr) return false;
	*head = m->next;
	if (*head == nullptr) *tail = nullptr;
	*out_data = m->data;
	*out_len = m->len;
	std::free(m);
	return true;
}

void Free_All_Messages(QueuedMessage **head, QueuedMessage **tail) {
	QueuedMessage *m = *head;
	while (m) {
		QueuedMessage *n = m->next;
		std::free(m->data);
		std::free(m);
		m = n;
	}
	*head = nullptr;
	*tail = nullptr;
}

int Count_Messages(QueuedMessage *head) {
	int n = 0;
	while (head) { ++n; head = head->next; }
	return n;
}

} // anonymous namespace


// ============================================================================
// EMSCRIPTEN BACKEND
// ============================================================================
#ifdef __EMSCRIPTEN__

struct WSImpl {
	EMSCRIPTEN_WEBSOCKET_T socket;
	QueuedMessage *head;
	QueuedMessage *tail;
	WSClientClass *owner;
};

static EM_BOOL Em_On_Open(int /*eventType*/, EmscriptenWebSocketOpenEvent const *e, void *userData) {
	WSImpl *impl = static_cast<WSImpl *>(userData);
	(void)e;
	if (impl && impl->owner) {
		// State_ is private; owner pokes it via a friend or dedicated setter.
		// We stash state externally through Poll-like accessor — simplest: the
		// owner polls getReadyState, so we don't touch State_ here.
	}
	return EM_TRUE;
}

static EM_BOOL Em_On_Close(int /*eventType*/, EmscriptenWebSocketCloseEvent const *e, void *userData) {
	(void)e;
	(void)userData;
	return EM_TRUE;
}

static EM_BOOL Em_On_Error(int /*eventType*/, EmscriptenWebSocketErrorEvent const *e, void *userData) {
	(void)e;
	(void)userData;
	return EM_TRUE;
}

static EM_BOOL Em_On_Message(int /*eventType*/, EmscriptenWebSocketMessageEvent const *e, void *userData) {
	WSImpl *impl = static_cast<WSImpl *>(userData);
	if (!impl) return EM_TRUE;
	if (e->isText) {
		// We don't expect text frames in the app protocol; ignore.
		return EM_TRUE;
	}
	uint8_t *copy = static_cast<uint8_t *>(std::malloc(e->numBytes));
	if (!copy) return EM_TRUE;
	std::memcpy(copy, e->data, e->numBytes);
	Push_Message(&impl->head, &impl->tail, copy, e->numBytes);
	return EM_TRUE;
}

WSClientClass::WSClientClass() : State_(STATE_DISCONNECTED), Impl_(nullptr) {
	WSImpl *impl = static_cast<WSImpl *>(std::calloc(1, sizeof(WSImpl)));
	impl->owner = this;
	Impl_ = impl;
}

WSClientClass::~WSClientClass() {
	Close();
	std::free(Impl_);
	Impl_ = nullptr;
}

bool WSClientClass::Connect(char const *url) {
	if (!emscripten_websocket_is_supported()) {
		State_ = STATE_FAILED;
		return false;
	}
	Close();
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	EmscriptenWebSocketCreateAttributes attrs;
	emscripten_websocket_init_create_attributes(&attrs);
	attrs.url = url;
	attrs.createOnMainThread = EM_TRUE;
	EMSCRIPTEN_WEBSOCKET_T s = emscripten_websocket_new(&attrs);
	if (s <= 0) {
		State_ = STATE_FAILED;
		return false;
	}
	impl->socket = s;
	emscripten_websocket_set_onopen_callback(s, impl, Em_On_Open);
	emscripten_websocket_set_onclose_callback(s, impl, Em_On_Close);
	emscripten_websocket_set_onerror_callback(s, impl, Em_On_Error);
	emscripten_websocket_set_onmessage_callback(s, impl, Em_On_Message);
	State_ = STATE_CONNECTING;
	return true;
}

void WSClientClass::Close() {
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	if (!impl) return;
	if (impl->socket) {
		emscripten_websocket_close(impl->socket, 1000, "bye");
		emscripten_websocket_delete(impl->socket);
		impl->socket = 0;
	}
	Free_All_Messages(&impl->head, &impl->tail);
	State_ = STATE_DISCONNECTED;
}

int WSClientClass::Poll() {
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	if (!impl || !impl->socket) return 0;
	// Transition state based on browser-side ready state.
	unsigned short ready = 3; // CLOSED
	emscripten_websocket_get_ready_state(impl->socket, &ready);
	switch (ready) {
	case 0: State_ = STATE_CONNECTING; break;
	case 1: State_ = STATE_OPEN; break;
	case 2: State_ = STATE_CLOSING; break;
	default:
		State_ = (State_ == STATE_CONNECTING) ? STATE_FAILED : STATE_DISCONNECTED;
		break;
	}
	return Count_Messages(impl->head);
}

bool WSClientClass::Send_Binary(void const *data, size_t len) {
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	if (!impl || !impl->socket || State_ != STATE_OPEN) return false;
	EMSCRIPTEN_RESULT r = emscripten_websocket_send_binary(impl->socket,
		const_cast<void *>(data), static_cast<uint32_t>(len));
	return r == EMSCRIPTEN_RESULT_SUCCESS;
}

bool WSClientClass::Receive_Binary(void **out_data, size_t *out_len) {
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	if (!impl) return false;
	return Pop_Message(&impl->head, &impl->tail, out_data, out_len);
}

int WSClientClass::Receive_Queue_Size() const {
	WSImpl *impl = static_cast<WSImpl *>(Impl_);
	if (!impl) return 0;
	return Count_Messages(impl->head);
}

#else // !__EMSCRIPTEN__

// ============================================================================
// NATIVE BACKEND
// ============================================================================

namespace {

// ---- Minimal SHA-1 (RFC 3174) ---------------------------------------------

struct Sha1Ctx {
	uint32_t state[5];
	uint64_t count;
	uint8_t buffer[64];
};

inline uint32_t Sha1_Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void Sha1_Transform(uint32_t state[5], uint8_t const buffer[64]) {
	uint32_t w[80];
	for (int i = 0; i < 16; ++i) {
		w[i] = (uint32_t(buffer[i*4]) << 24)
		     | (uint32_t(buffer[i*4+1]) << 16)
		     | (uint32_t(buffer[i*4+2]) << 8)
		     | (uint32_t(buffer[i*4+3]));
	}
	for (int i = 16; i < 80; ++i) {
		w[i] = Sha1_Rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
	}
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
	for (int i = 0; i < 80; ++i) {
		uint32_t f, k;
		if (i < 20)      { f = (b & c) | ((~b) & d);       k = 0x5A827999; }
		else if (i < 40) { f = b ^ c ^ d;                  k = 0x6ED9EBA1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
		else             { f = b ^ c ^ d;                  k = 0xCA62C1D6; }
		uint32_t temp = Sha1_Rol(a, 5) + f + e + k + w[i];
		e = d; d = c; c = Sha1_Rol(b, 30); b = a; a = temp;
	}
	state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void Sha1_Init(Sha1Ctx *ctx) {
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	ctx->count = 0;
}

void Sha1_Update(Sha1Ctx *ctx, uint8_t const *data, size_t len) {
	size_t offset = size_t(ctx->count & 63);
	ctx->count += len;
	if (offset + len >= 64) {
		size_t fill = 64 - offset;
		std::memcpy(ctx->buffer + offset, data, fill);
		Sha1_Transform(ctx->state, ctx->buffer);
		data += fill; len -= fill; offset = 0;
		while (len >= 64) {
			Sha1_Transform(ctx->state, data);
			data += 64; len -= 64;
		}
	}
	if (len > 0) std::memcpy(ctx->buffer + offset, data, len);
}

void Sha1_Final(Sha1Ctx *ctx, uint8_t out[20]) {
	uint64_t bits = ctx->count * 8;
	uint8_t pad = 0x80;
	Sha1_Update(ctx, &pad, 1);
	uint8_t zero = 0;
	while ((ctx->count & 63) != 56) Sha1_Update(ctx, &zero, 1);
	uint8_t len_be[8];
	for (int i = 0; i < 8; ++i) len_be[i] = uint8_t(bits >> (56 - i*8));
	Sha1_Update(ctx, len_be, 8);
	for (int i = 0; i < 5; ++i) {
		out[i*4]   = uint8_t(ctx->state[i] >> 24);
		out[i*4+1] = uint8_t(ctx->state[i] >> 16);
		out[i*4+2] = uint8_t(ctx->state[i] >> 8);
		out[i*4+3] = uint8_t(ctx->state[i]);
	}
}

// ---- Base64 ---------------------------------------------------------------

static char const BASE64_ALPHABET[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void Base64_Encode(uint8_t const *data, size_t len, char *out) {
	size_t i = 0, o = 0;
	while (i + 3 <= len) {
		uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8) | uint32_t(data[i+2]);
		out[o++] = BASE64_ALPHABET[(v >> 18) & 0x3F];
		out[o++] = BASE64_ALPHABET[(v >> 12) & 0x3F];
		out[o++] = BASE64_ALPHABET[(v >> 6) & 0x3F];
		out[o++] = BASE64_ALPHABET[v & 0x3F];
		i += 3;
	}
	size_t rem = len - i;
	if (rem == 1) {
		uint32_t v = uint32_t(data[i]) << 16;
		out[o++] = BASE64_ALPHABET[(v >> 18) & 0x3F];
		out[o++] = BASE64_ALPHABET[(v >> 12) & 0x3F];
		out[o++] = '=';
		out[o++] = '=';
	} else if (rem == 2) {
		uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8);
		out[o++] = BASE64_ALPHABET[(v >> 18) & 0x3F];
		out[o++] = BASE64_ALPHABET[(v >> 12) & 0x3F];
		out[o++] = BASE64_ALPHABET[(v >> 6) & 0x3F];
		out[o++] = '=';
	}
	out[o] = '\0';
}

// ---- Helpers --------------------------------------------------------------

static char const WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

bool Parse_WS_URL(char const *url, bool *secure, char *host, size_t host_cap,
                  uint16_t *port, char *path, size_t path_cap) {
	char const *p = url;
	if (std::strncmp(p, "ws://", 5) == 0) { *secure = false; p += 5; }
	else if (std::strncmp(p, "wss://", 6) == 0) { *secure = true; p += 6; }
	else return false;

	char const *host_end = p;
	while (*host_end && *host_end != ':' && *host_end != '/') ++host_end;
	size_t host_len = size_t(host_end - p);
	if (host_len == 0 || host_len >= host_cap) return false;
	std::memcpy(host, p, host_len);
	host[host_len] = '\0';
	p = host_end;

	*port = *secure ? 443 : 80;
	if (*p == ':') {
		++p;
		int parsed = 0;
		int digits = 0;
		while (*p >= '0' && *p <= '9' && digits < 5) {
			parsed = parsed * 10 + (*p - '0');
			++p; ++digits;
		}
		if (digits == 0 || parsed > 65535) return false;
		*port = uint16_t(parsed);
	}

	if (*p == '\0') {
		std::snprintf(path, path_cap, "/");
	} else if (*p == '/') {
		std::snprintf(path, path_cap, "%s", p);
	} else {
		return false;
	}
	return true;
}

struct Native {
	SocketHandle sock;
	char host[256];
	char path[512];
	uint16_t port;
	bool secure;

	// Handshake state.
	bool handshake_sent;
	char handshake_key_b64[32]; // 16 random bytes -> base64(24)
	char expected_accept[32];
	uint8_t *http_buf;
	size_t http_buf_len;
	size_t http_buf_cap;

	// Outbound buffer for partial writes.
	uint8_t *out_buf;
	size_t out_buf_len;
	size_t out_buf_cap;

	// Inbound raw stream.
	uint8_t *in_buf;
	size_t in_buf_len;
	size_t in_buf_cap;

	// Reassembly of fragmented frames.
	uint8_t *frag_buf;
	size_t frag_buf_len;
	size_t frag_buf_cap;
	uint8_t frag_opcode;

	QueuedMessage *head;
	QueuedMessage *tail;
};

void Native_Free(Native *n) {
	if (!n) return;
	if (n->sock != INVALID_SOCKET_HANDLE) {
		Socket_Close(n->sock);
		n->sock = INVALID_SOCKET_HANDLE;
	}
	std::free(n->http_buf); n->http_buf = nullptr;
	std::free(n->out_buf);  n->out_buf  = nullptr;
	std::free(n->in_buf);   n->in_buf   = nullptr;
	std::free(n->frag_buf); n->frag_buf = nullptr;
	Free_All_Messages(&n->head, &n->tail);
}

bool Buffer_Append(uint8_t **buf, size_t *len, size_t *cap, void const *data, size_t n) {
	if (*len + n > *cap) {
		size_t newcap = (*cap == 0) ? 1024 : *cap;
		while (newcap < *len + n) newcap *= 2;
		uint8_t *nb = static_cast<uint8_t *>(std::realloc(*buf, newcap));
		if (!nb) return false;
		*buf = nb;
		*cap = newcap;
	}
	std::memcpy(*buf + *len, data, n);
	*len += n;
	return true;
}

void Buffer_Consume(uint8_t *buf, size_t *len, size_t n) {
	if (n >= *len) { *len = 0; return; }
	std::memmove(buf, buf + n, *len - n);
	*len -= n;
}

bool Compute_Expected_Accept(char const *key_b64, char *out32) {
	char combined[128];
	int w = std::snprintf(combined, sizeof(combined), "%s%s", key_b64, WS_GUID);
	if (w <= 0 || size_t(w) >= sizeof(combined)) return false;
	Sha1Ctx ctx;
	Sha1_Init(&ctx);
	Sha1_Update(&ctx, reinterpret_cast<uint8_t const *>(combined), size_t(w));
	uint8_t digest[20];
	Sha1_Final(&ctx, digest);
	Base64_Encode(digest, 20, out32);
	return true;
}

void Generate_Random_Key(char *out_b64_32) {
	uint8_t bytes[16];
	// Combine time + a counter + stack address for entropy.
	static uint64_t counter = 0;
	++counter;
	uint64_t t = uint64_t(std::time(nullptr));
	uintptr_t stk = reinterpret_cast<uintptr_t>(&bytes);
	for (int i = 0; i < 16; ++i) {
		uint64_t v = t ^ (counter * 0x9E3779B97F4A7C15ull) ^ (uint64_t(stk) << (i & 7));
		bytes[i] = uint8_t((v >> ((i * 13) & 56)) ^ (i * 17));
		t = t * 6364136223846793005ull + 1442695040888963407ull;
	}
	Base64_Encode(bytes, 16, out_b64_32);
}

bool Send_All_Nonblocking(Native *n, uint8_t const *data, size_t len) {
	// Try to write what we can; buffer the rest.
	size_t off = 0;
	while (off < len) {
		int wrote = Socket_Write(n->sock, data + off, int(len - off));
		if (wrote > 0) {
			off += size_t(wrote);
			continue;
		}
		int err = Socket_Get_Last_Error();
		if (wrote < 0 && Socket_Error_Is_Would_Block(err)) {
			break;
		}
		return false;
	}
	if (off < len) {
		if (!Buffer_Append(&n->out_buf, &n->out_buf_len, &n->out_buf_cap, data + off, len - off)) {
			return false;
		}
	}
	return true;
}

bool Flush_Out_Buffer(Native *n) {
	while (n->out_buf_len > 0) {
		int wrote = Socket_Write(n->sock, n->out_buf, int(n->out_buf_len));
		if (wrote > 0) {
			Buffer_Consume(n->out_buf, &n->out_buf_len, size_t(wrote));
			continue;
		}
		int err = Socket_Get_Last_Error();
		if (wrote < 0 && Socket_Error_Is_Would_Block(err)) return true;
		return false;
	}
	return true;
}

bool Write_Client_Frame(Native *n, uint8_t opcode, uint8_t const *payload, size_t len) {
	// Client frames MUST be masked (RFC 6455).
	uint8_t hdr[14];
	size_t hlen = 0;
	hdr[hlen++] = 0x80 | (opcode & 0x0F); // FIN set, opcode
	uint8_t mask_bit = 0x80;
	if (len < 126) {
		hdr[hlen++] = uint8_t(mask_bit | uint8_t(len));
	} else if (len <= 0xFFFF) {
		hdr[hlen++] = uint8_t(mask_bit | 126);
		hdr[hlen++] = uint8_t((len >> 8) & 0xFF);
		hdr[hlen++] = uint8_t(len & 0xFF);
	} else {
		hdr[hlen++] = uint8_t(mask_bit | 127);
		uint64_t l64 = uint64_t(len);
		for (int i = 7; i >= 0; --i) hdr[hlen++] = uint8_t((l64 >> (i * 8)) & 0xFF);
	}
	// Mask key.
	uint8_t mask[4];
	for (int i = 0; i < 4; ++i) mask[i] = uint8_t((std::rand() & 0xFF) ^ (i * 37));
	std::memcpy(hdr + hlen, mask, 4);
	hlen += 4;

	if (!Send_All_Nonblocking(n, hdr, hlen)) return false;

	if (len > 0) {
		uint8_t *masked = static_cast<uint8_t *>(std::malloc(len));
		if (!masked) return false;
		for (size_t i = 0; i < len; ++i) masked[i] = payload[i] ^ mask[i & 3];
		bool ok = Send_All_Nonblocking(n, masked, len);
		std::free(masked);
		if (!ok) return false;
	}
	return true;
}

bool Send_Handshake(Native *n) {
	Generate_Random_Key(n->handshake_key_b64);
	Compute_Expected_Accept(n->handshake_key_b64, n->expected_accept);
	char req[1024];
	int w = std::snprintf(req, sizeof(req),
		"GET %s HTTP/1.1\r\n"
		"Host: %s:%u\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: %s\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n",
		n->path, n->host, unsigned(n->port), n->handshake_key_b64);
	if (w <= 0 || size_t(w) >= sizeof(req)) return false;
	n->handshake_sent = true;
	return Send_All_Nonblocking(n, reinterpret_cast<uint8_t *>(req), size_t(w));
}

/// Returns 1 on success, 0 on not-yet-complete, -1 on error.
int Try_Parse_Handshake_Response(Native *n) {
	// Look for "\r\n\r\n" in http_buf.
	for (size_t i = 3; i < n->http_buf_len; ++i) {
		if (n->http_buf[i-3] == '\r' && n->http_buf[i-2] == '\n'
		 && n->http_buf[i-1] == '\r' && n->http_buf[i] == '\n') {
			size_t header_len = i + 1;
			// Parse status line.
			char line[256];
			size_t line_end = 0;
			while (line_end < n->http_buf_len && n->http_buf[line_end] != '\r' && line_end + 1 < sizeof(line)) {
				line[line_end] = char(n->http_buf[line_end]);
				++line_end;
			}
			line[line_end] = '\0';
			if (std::strstr(line, " 101 ") == nullptr) {
				return -1;
			}
			// Find Sec-WebSocket-Accept header.
			char const *acc_header = "\r\nsec-websocket-accept:";
			// Build a lowercased copy of headers to find it.
			char *lc = static_cast<char *>(std::malloc(header_len + 1));
			if (!lc) return -1;
			for (size_t k = 0; k < header_len; ++k) {
				char c = char(n->http_buf[k]);
				if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
				lc[k] = c;
			}
			lc[header_len] = '\0';
			char const *found = std::strstr(lc, acc_header);
			if (!found) { std::free(lc); return -1; }
			size_t val_off = size_t(found - lc) + std::strlen(acc_header);
			while (val_off < header_len && (lc[val_off] == ' ' || lc[val_off] == '\t')) ++val_off;
			size_t val_end = val_off;
			while (val_end < header_len && lc[val_end] != '\r') ++val_end;
			// Compare against expected (case sensitive for the base64 value).
			size_t exp_len = std::strlen(n->expected_accept);
			if (val_end - val_off != exp_len) { std::free(lc); return -1; }
			// Use the original (non-lowercased) bytes for comparison.
			if (std::memcmp(n->http_buf + val_off, n->expected_accept, exp_len) != 0) {
				std::free(lc); return -1;
			}
			std::free(lc);

			// Any bytes after header_len are the start of WS frames.
			size_t trailing = n->http_buf_len - header_len;
			if (trailing > 0) {
				if (!Buffer_Append(&n->in_buf, &n->in_buf_len, &n->in_buf_cap,
					n->http_buf + header_len, trailing)) return -1;
			}
			std::free(n->http_buf); n->http_buf = nullptr;
			n->http_buf_len = n->http_buf_cap = 0;
			return 1;
		}
	}
	return 0;
}

/// Try to parse one WebSocket frame from n->in_buf. Returns:
///   >0 = consumed that many bytes (a frame was handled)
///    0 = need more data
///   -1 = protocol error
int Try_Parse_One_Frame(Native *n) {
	if (n->in_buf_len < 2) return 0;
	uint8_t b0 = n->in_buf[0];
	uint8_t b1 = n->in_buf[1];
	bool fin = (b0 & 0x80) != 0;
	uint8_t opcode = b0 & 0x0F;
	bool masked = (b1 & 0x80) != 0;
	if (masked) return -1; // Server MUST NOT mask.
	uint64_t plen = b1 & 0x7F;
	size_t off = 2;
	if (plen == 126) {
		if (n->in_buf_len < off + 2) return 0;
		plen = (uint64_t(n->in_buf[off]) << 8) | uint64_t(n->in_buf[off+1]);
		off += 2;
	} else if (plen == 127) {
		if (n->in_buf_len < off + 8) return 0;
		plen = 0;
		for (int i = 0; i < 8; ++i) plen = (plen << 8) | uint64_t(n->in_buf[off+i]);
		off += 8;
	}
	if (plen > 16ull * 1024ull * 1024ull) return -1;
	if (n->in_buf_len < off + plen) return 0;
	uint8_t const *payload = n->in_buf + off;

	switch (opcode) {
	case 0x0: // continuation
		if (n->frag_buf_len == 0 && !n->frag_opcode) return -1;
		if (!Buffer_Append(&n->frag_buf, &n->frag_buf_len, &n->frag_buf_cap, payload, size_t(plen))) return -1;
		if (fin) {
			if (n->frag_opcode == 0x2) {
				uint8_t *copy = static_cast<uint8_t *>(std::malloc(n->frag_buf_len));
				if (!copy) return -1;
				std::memcpy(copy, n->frag_buf, n->frag_buf_len);
				Push_Message(&n->head, &n->tail, copy, n->frag_buf_len);
			}
			n->frag_buf_len = 0;
			n->frag_opcode = 0;
		}
		break;
	case 0x1: // text — ignore
		break;
	case 0x2: // binary
		if (fin) {
			uint8_t *copy = static_cast<uint8_t *>(std::malloc(size_t(plen)));
			if (!copy && plen > 0) return -1;
			if (plen > 0) std::memcpy(copy, payload, size_t(plen));
			Push_Message(&n->head, &n->tail, copy, size_t(plen));
		} else {
			n->frag_opcode = 0x2;
			if (!Buffer_Append(&n->frag_buf, &n->frag_buf_len, &n->frag_buf_cap, payload, size_t(plen))) return -1;
		}
		break;
	case 0x8: // close
		Write_Client_Frame(n, 0x8, nullptr, 0);
		return -1;
	case 0x9: // ping
		Write_Client_Frame(n, 0xA, payload, size_t(plen));
		break;
	case 0xA: // pong — ignore
		break;
	default:
		return -1;
	}

	return int(off + plen);
}

} // anonymous namespace


WSClientClass::WSClientClass() : State_(STATE_DISCONNECTED), Impl_(nullptr) {
	Native *n = static_cast<Native *>(std::calloc(1, sizeof(Native)));
	n->sock = INVALID_SOCKET_HANDLE;
	Impl_ = n;
}

WSClientClass::~WSClientClass() {
	Close();
	if (Impl_) {
		Native_Free(static_cast<Native *>(Impl_));
		std::free(Impl_);
		Impl_ = nullptr;
	}
}

bool WSClientClass::Connect(char const *url) {
	Native *n = static_cast<Native *>(Impl_);
	Close();
	n = static_cast<Native *>(Impl_);
	bool secure = false;
	if (!Parse_WS_URL(url, &secure, n->host, sizeof(n->host), &n->port, n->path, sizeof(n->path))) {
		State_ = STATE_FAILED;
		return false;
	}
	if (secure) {
		// TLS not supported in v1.
		State_ = STATE_FAILED;
		return false;
	}
	n->secure = false;

	SocketHostEntry host_entry;
	uint32_t addr = 0;
	if (Socket_Address_From_String(n->host, &addr)) {
		// ok, numeric
	} else if (Socket_Lookup_Host_By_Name(n->host, &host_entry) && host_entry.AddressCount > 0) {
		addr = host_entry.Addresses[0];
	} else {
		State_ = STATE_FAILED;
		return false;
	}

	n->sock = Socket_Open(SOCKET_TYPE_STREAM, SOCKET_PROTOCOL_TCP);
	if (n->sock == INVALID_SOCKET_HANDLE) {
		State_ = STATE_FAILED;
		return false;
	}
	Socket_Set_NonBlocking(n->sock, true);

	SocketAddress sa;
	sa.Address = addr;
	sa.Port = Socket_Host_To_Network16(n->port);
	int rc = Socket_Connect(n->sock, &sa);
	if (rc < 0) {
		int err = Socket_Get_Last_Error();
		if (!Socket_Error_Is_Would_Block(err)) {
			Socket_Close(n->sock);
			n->sock = INVALID_SOCKET_HANDLE;
			State_ = STATE_FAILED;
			return false;
		}
	}
	State_ = STATE_CONNECTING;
	return true;
}

void WSClientClass::Close() {
	Native *n = static_cast<Native *>(Impl_);
	if (!n) return;
	if (State_ == STATE_OPEN && n->sock != INVALID_SOCKET_HANDLE) {
		Write_Client_Frame(n, 0x8, nullptr, 0);
		Flush_Out_Buffer(n);
	}
	Native_Free(n);
	std::memset(n, 0, sizeof(*n));
	n->sock = INVALID_SOCKET_HANDLE;
	State_ = STATE_DISCONNECTED;
}

int WSClientClass::Poll() {
	Native *n = static_cast<Native *>(Impl_);
	if (!n || n->sock == INVALID_SOCKET_HANDLE) return 0;

	if (State_ == STATE_CONNECTING) {
		if (Socket_Is_Write_Ready(n->sock)) {
			if (!n->handshake_sent) {
				if (!Send_Handshake(n)) {
					State_ = STATE_FAILED;
					return 0;
				}
			}
		}
	}

	// Flush pending outbound.
	if (!Flush_Out_Buffer(n)) {
		State_ = STATE_FAILED;
		return 0;
	}

	// Read available.
	uint8_t tmp[4096];
	for (;;) {
		int r = Socket_Read(n->sock, tmp, int(sizeof(tmp)));
		if (r > 0) {
			if (State_ == STATE_CONNECTING) {
				if (!Buffer_Append(&n->http_buf, &n->http_buf_len, &n->http_buf_cap, tmp, size_t(r))) {
					State_ = STATE_FAILED;
					return 0;
				}
				int pr = Try_Parse_Handshake_Response(n);
				if (pr < 0) { State_ = STATE_FAILED; return 0; }
				if (pr > 0) { State_ = STATE_OPEN; }
			} else {
				if (!Buffer_Append(&n->in_buf, &n->in_buf_len, &n->in_buf_cap, tmp, size_t(r))) {
					State_ = STATE_FAILED;
					return 0;
				}
			}
			continue;
		}
		if (r == 0) {
			State_ = STATE_DISCONNECTED;
			return Count_Messages(n->head);
		}
		int err = Socket_Get_Last_Error();
		if (Socket_Error_Is_Would_Block(err)) break;
		State_ = STATE_FAILED;
		return 0;
	}

	// Drain frames.
	if (State_ == STATE_OPEN) {
		for (;;) {
			int consumed = Try_Parse_One_Frame(n);
			if (consumed < 0) { State_ = STATE_FAILED; return 0; }
			if (consumed == 0) break;
			Buffer_Consume(n->in_buf, &n->in_buf_len, size_t(consumed));
		}
	}

	return Count_Messages(n->head);
}

bool WSClientClass::Send_Binary(void const *data, size_t len) {
	Native *n = static_cast<Native *>(Impl_);
	if (!n || State_ != STATE_OPEN) return false;
	return Write_Client_Frame(n, 0x2, static_cast<uint8_t const *>(data), len);
}

bool WSClientClass::Receive_Binary(void **out_data, size_t *out_len) {
	Native *n = static_cast<Native *>(Impl_);
	if (!n) return false;
	return Pop_Message(&n->head, &n->tail, out_data, out_len);
}

int WSClientClass::Receive_Queue_Size() const {
	Native *n = static_cast<Native *>(Impl_);
	if (!n) return 0;
	return Count_Messages(n->head);
}

#endif // __EMSCRIPTEN__
