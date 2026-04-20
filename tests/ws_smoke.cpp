// Standalone smoke test: drive WSClientClass against the Zig WOL server.
// Builds against CODE/WSCLIENT.CPP, CODE/SOCKETS_LINUX.CPP, and CODE/WOL_PROTO.H.
//
// g++ -std=c++17 -I CODE tests/ws_smoke.cpp CODE/WSCLIENT.CPP CODE/SOCKETS_LINUX.CPP -o /tmp/ws_smoke

#include "../CODE/WSCLIENT.H"
#include "../CODE/WOL_PROTO.H"
#include "../CODE/SOCKETS.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>

using namespace WolProto;

static void Build_Hello(uint8_t *buf, size_t buf_cap, size_t *out_len, char const *nick) {
	uint16_t nick_len = uint16_t(std::strlen(nick));
	size_t payload_len = 4 + 2 + nick_len;
	if (HEADER_SIZE + payload_len > buf_cap) std::exit(2);
	Encode_Header(buf, OP_HELLO, 0, uint32_t(payload_len));
	uint8_t *p = buf + HEADER_SIZE;
	Write_U32(p, PROTOCOL_VERSION);
	Write_U16(p, nick_len);
	std::memcpy(p, nick, nick_len);
	*out_len = HEADER_SIZE + payload_len;
}

static bool Wait_For_Message(WSClientClass &c, void **data, size_t *len, int timeout_ms) {
	int elapsed = 0;
	while (elapsed < timeout_ms) {
		c.Poll();
		if (c.Receive_Binary(data, len)) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		elapsed += 20;
	}
	return false;
}

int main(int argc, char **argv) {
	if (!Socket_Startup()) { std::fprintf(stderr, "socket startup failed\n"); return 1; }

	char const *url = (argc > 1) ? argv[1] : "ws://127.0.0.1:19192/ws";
	WSClientClass c;
	if (!c.Connect(url)) {
		std::fprintf(stderr, "connect failed\n");
		return 1;
	}

	// Wait for handshake.
	int tries = 0;
	while (c.Get_State() == WSClientClass::STATE_CONNECTING && tries < 200) {
		c.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		++tries;
	}
	if (c.Get_State() != WSClientClass::STATE_OPEN) {
		std::fprintf(stderr, "handshake failed, state=%d\n", int(c.Get_State()));
		return 1;
	}
	std::printf("handshake ok\n");

	uint8_t buf[256];
	size_t buf_len;
	Build_Hello(buf, sizeof(buf), &buf_len, "TESTER");
	if (!c.Send_Binary(buf, buf_len)) { std::fprintf(stderr, "send HELLO failed\n"); return 1; }

	void *msg = nullptr;
	size_t mlen = 0;
	if (!Wait_For_Message(c, &msg, &mlen, 2000)) {
		std::fprintf(stderr, "no reply to HELLO\n");
		return 1;
	}
	ParsedFrame pf;
	if (!Parse_Frame(static_cast<uint8_t *>(msg), mlen, &pf)) {
		std::fprintf(stderr, "bad reply frame\n"); return 1;
	}
	if (pf.opcode != OP_WELCOME) {
		std::fprintf(stderr, "expected WELCOME got 0x%04x\n", pf.opcode); return 1;
	}
	uint8_t const *rp = pf.payload;
	uint32_t cid = Read_U32(rp);
	uint32_t sv  = Read_U32(rp);
	std::printf("WELCOME cid=%u server_version=%u\n", cid, sv);
	std::free(msg);

	c.Close();
	Socket_Cleanup();
	std::printf("ok\n");
	return 0;
}
