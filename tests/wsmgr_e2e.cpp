// End-to-end test: two WSManagerClass instances talk through the Zig server,
// simulating the in-game data plane.
//
// g++ -std=c++17 -I CODE tests/wsmgr_e2e.cpp \
//   CODE/WSMGR.CPP CODE/WSCLIENT.CPP CODE/SOCKETS_LINUX.CPP \
//   -o /tmp/wsmgr_e2e

#include "../CODE/WSMGR.H"
#include "../CODE/WOL_PROTO.H"
#include "../CODE/SOCKETS.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

using namespace WolProto;

// Manager-defined global is Wol; we need a second instance for the test.
// Include a local second manager.
static void Pump(WSManagerClass &a, WSManagerClass &b, int ms) {
	int elapsed = 0;
	while (elapsed < ms) {
		a.Service();
		b.Service();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
}

static bool Wait_For_Welcome(WSManagerClass &m, int ms) {
	int elapsed = 0;
	while (elapsed < ms) {
		m.Service();
		if (m.Is_Logged_In()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	return false;
}

static bool Wait_For_Ctrl_Op(WSManagerClass &m, uint16_t expect, uint8_t *out, int *out_len, int ms) {
	int elapsed = 0;
	while (elapsed < ms) {
		m.Service();
		int lim = *out_len;
		uint16_t op = m.Next_Control_Frame(out, &lim);
		if (op == expect) {
			*out_len = lim;
			return true;
		}
		if (op != 0) {
			std::fprintf(stderr, "  (ignored ctrl 0x%04x)\n", op);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	return false;
}

static bool Connect_And_Hello(WSManagerClass &m, char const *url, char const *nick) {
	m.Configure(url, nick);
	if (!m.Init()) {
		std::fprintf(stderr, "init/login failed for %s\n", nick);
		return false;
	}
	return true;
}

int main(int argc, char **argv) {
	char const *url = (argc > 1) ? argv[1] : "ws://127.0.0.1:19192/ws";
	Socket_Startup();

	WSManagerClass host;
	WSManagerClass guest;

	if (!Connect_And_Hello(host, url, "HOST")) { std::fprintf(stderr, "host hello fail\n"); return 1; }
	std::printf("HOST logged in as cid=%u\n", host.My_Client_Id());

	if (!Connect_And_Hello(guest, url, "GUEST")) { std::fprintf(stderr, "guest hello fail\n"); return 1; }
	std::printf("GUEST logged in as cid=%u\n", guest.My_Client_Id());

	// Host creates game.
	if (!host.Send_Game_Create("TESTGAME", 4)) { std::fprintf(stderr, "create fail\n"); return 1; }

	uint8_t buf[4096];
	int buf_len;

	// Host should receive GAME_LIST_REPLY + GAME_MEMBERS.
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "host no GAME_LIST_REPLY\n"); return 1;
	}
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_MEMBERS, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "host no GAME_MEMBERS\n"); return 1;
	}
	// Parse GAME_MEMBERS: u32 game_id, u16 count, {u32 cid, str nick} x count
	uint8_t const *p = buf;
	uint32_t game_id = Read_U32(p);
	std::printf("HOST: created game_id=%u\n", game_id);

	// Guest receives GAME_LIST_REPLY; parse to find game_id.
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no GAME_LIST_REPLY\n"); return 1;
	}

	// Guest joins game.
	if (!guest.Send_Game_Join(game_id)) { std::fprintf(stderr, "join fail\n"); return 1; }

	// Guest receives GAME_LIST_REPLY (updated) + GAME_MEMBERS.
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no GAME_LIST_REPLY after join\n"); return 1;
	}
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_MEMBERS, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no GAME_MEMBERS\n"); return 1;
	}
	// Parse GAME_MEMBERS: u32 game_id, u16 count, {u32 cid, str nick} x count
	p = buf;
	(void)Read_U32(p); // game_id
	uint32_t host_id_field = Read_U32(p);
	(void)host_id_field;
	uint16_t count = Read_U16(p);
	std::printf("GUEST: %u members in game\n", count);
	uint32_t host_cid = 0;
	for (int i = 0; i < count; ++i) {
		uint32_t cid = Read_U32(p);
		uint16_t nl = Read_U16(p);
		char nick[64]; int nc = nl < 63 ? nl : 63; std::memcpy(nick, p, nc); nick[nc] = '\0';
		p += nl;
		std::printf("  member cid=%u nick=%s\n", cid, nick);
		if (cid != guest.My_Client_Id()) host_cid = cid;
	}

	// Host also receives updated members.
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) { std::fprintf(stderr, "host no updated game list\n"); return 1; }
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_MEMBERS, buf, &buf_len, 2000)) { std::fprintf(stderr, "host no updated members\n"); return 1; }

	// Set up peer connections in both managers.
	host.Create_Connection_By_Cid(1, "GUEST", guest.My_Client_Id());
	guest.Create_Connection_By_Cid(1, "HOST", host_cid);
	std::printf("connections configured: host sees guest cid=%u, guest sees host cid=%u\n",
		guest.My_Client_Id(), host_cid);

	// Host starts the game.
	host.Send_Game_Start();

	// Both receive GAME_STARTED.
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_STARTED, buf, &buf_len, 2000)) { std::fprintf(stderr, "host no GAME_STARTED\n"); return 1; }
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_STARTED, buf, &buf_len, 2000)) { std::fprintf(stderr, "guest no GAME_STARTED\n"); return 1; }
	std::printf("game started on both sides\n");

	// Now simulate gameplay: host sends a private message to guest, guest echoes back.
	char const *hello_msg = "HELLO-FROM-HOST";
	if (!host.Send_Private_Message((void*)hello_msg, int(std::strlen(hello_msg)), 1, 1)) {
		std::fprintf(stderr, "host send private fail\n"); return 1;
	}

	// Guest services and receives.
	int elapsed = 0;
	while (elapsed < 2000 && guest.Private_Num_Receive() == 0) {
		guest.Service();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	if (guest.Private_Num_Receive() == 0) { std::fprintf(stderr, "guest never got private msg\n"); return 1; }

	char rbuf[256]; int rlen = sizeof(rbuf); int cid_out = -1;
	if (!guest.Get_Private_Message(rbuf, &rlen, &cid_out)) { std::fprintf(stderr, "guest Get_Private fail\n"); return 1; }
	rbuf[rlen < 255 ? rlen : 255] = '\0';
	std::printf("GUEST received '%s' (len=%d) from conn_id=%d\n", rbuf, rlen, cid_out);
	if (std::strcmp(rbuf, hello_msg) != 0) { std::fprintf(stderr, "msg mismatch\n"); return 1; }

	// Echo back.
	char const *echo = "HELLO-FROM-GUEST";
	if (!guest.Send_Private_Message((void*)echo, int(std::strlen(echo)), 1, 1)) return 1;

	elapsed = 0;
	while (elapsed < 2000 && host.Private_Num_Receive() == 0) {
		host.Service();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	if (host.Private_Num_Receive() == 0) { std::fprintf(stderr, "host never got echo\n"); return 1; }
	rlen = sizeof(rbuf); cid_out = -1;
	if (!host.Get_Private_Message(rbuf, &rlen, &cid_out)) return 1;
	rbuf[rlen < 255 ? rlen : 255] = '\0';
	std::printf("HOST received '%s' (len=%d) from conn_id=%d\n", rbuf, rlen, cid_out);
	if (std::strcmp(rbuf, echo) != 0) { std::fprintf(stderr, "echo mismatch\n"); return 1; }

	// Broadcast test.
	char const *bcast = "BROADCAST-FROM-HOST";
	host.Send_Private_Message((void*)bcast, int(std::strlen(bcast)), 1, ConnManClass::CONNECTION_NONE);
	elapsed = 0;
	while (elapsed < 2000 && guest.Private_Num_Receive() == 0) {
		guest.Service();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	if (guest.Private_Num_Receive() == 0) { std::fprintf(stderr, "guest never got broadcast\n"); return 1; }
	rlen = sizeof(rbuf);
	if (!guest.Get_Private_Message(rbuf, &rlen, &cid_out)) return 1;
	rbuf[rlen < 255 ? rlen : 255] = '\0';
	std::printf("GUEST received broadcast '%s'\n", rbuf);
	if (std::strcmp(rbuf, bcast) != 0) { std::fprintf(stderr, "bcast mismatch\n"); return 1; }

	// A peer that leaves immediately after sending must not leave a stale
	// private gameplay packet queued behind the disconnect notification.
	char const *stale = "STALE-BY-DISCONNECT";
	if (!guest.Send_Private_Message((void*)stale, int(std::strlen(stale)), 1, 1)) {
		std::fprintf(stderr, "guest stale send fail\n"); return 1;
	}
	guest.Shutdown();
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_PEER_LEFT, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "host no PEER_LEFT after guest shutdown\n"); return 1;
	}
	if (host.Private_Num_Receive() != 0) {
		std::fprintf(stderr, "stale private packet survived disconnect\n"); return 1;
	}

	// Return both peers to the lobby without logging out, then start a second game.
	host.Return_To_Lobby();
	guest.Return_To_Lobby();

	if (!Connect_And_Hello(host, url, "HOST")) { std::fprintf(stderr, "host re-init fail\n"); return 1; }
	if (!Connect_And_Hello(guest, url, "GUEST")) { std::fprintf(stderr, "guest re-init fail\n"); return 1; }

	if (!host.Send_Game_Create("TESTGAME2", 4)) { std::fprintf(stderr, "create second game fail\n"); return 1; }
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "host no second GAME_LIST_REPLY\n"); return 1;
	}
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(host, OP_GAME_MEMBERS, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "host no second GAME_MEMBERS\n"); return 1;
	}
	p = buf;
	game_id = Read_U32(p);

	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no second GAME_LIST_REPLY\n"); return 1;
	}
	if (!guest.Send_Game_Join(game_id)) { std::fprintf(stderr, "second join fail\n"); return 1; }
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_LIST_REPLY, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no updated second GAME_LIST_REPLY\n"); return 1;
	}
	buf_len = sizeof(buf);
	if (!Wait_For_Ctrl_Op(guest, OP_GAME_MEMBERS, buf, &buf_len, 2000)) {
		std::fprintf(stderr, "guest no second GAME_MEMBERS\n"); return 1;
	}

	(void)Pump;
	Socket_Cleanup();
	std::printf("ok\n");
	return 0;
}
