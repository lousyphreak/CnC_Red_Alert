// Multi-client end-to-end test: N WSManagerClass instances share one game
// and all exchange private and broadcast traffic through the relay.
//
// build: see tests/wsmgr_e2e.cpp; same deps.

#include "../CODE/WSMGR.H"
#include "../CODE/WOL_PROTO.H"
#include "../CODE/SOCKETS.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <memory>

using namespace WolProto;

static bool Wait_For_Welcome(WSManagerClass &m, int ms) {
	int e = 0;
	while (e < ms) { m.Service(); if (m.Is_Logged_In()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(10)); e += 10; }
	return false;
}

static bool Wait_Ctrl(WSManagerClass &m, uint16_t expect, uint8_t *buf, int *blen, int ms) {
	int e = 0;
	while (e < ms) {
		m.Service();
		int lim = *blen;
		uint16_t op = m.Next_Control_Frame(buf, &lim);
		if (op == expect) { *blen = lim; return true; }
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		e += 10;
	}
	return false;
}

static bool Connect_And_Login(WSManagerClass &m, char const *url, char const *nick) {
	if (!m.Connect(url)) return false;
	for (int i = 0; i < 40; ++i) { m.Service(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
	if (!m.Send_Hello(nick)) return false;
	return Wait_For_Welcome(m, 3000);
}

int main(int argc, char **argv) {
	char const *url = (argc > 1) ? argv[1] : "ws://127.0.0.1:19192/ws";
	int const N = (argc > 2) ? std::atoi(argv[2]) : 4;
	Socket_Startup();

	std::vector<std::unique_ptr<WSManagerClass>> mgrs;
	std::vector<std::string> nicks;
	for (int i = 0; i < N; ++i) {
		mgrs.push_back(std::make_unique<WSManagerClass>());
		char nick[32]; std::snprintf(nick, sizeof(nick), "P%d", i);
		nicks.push_back(nick);
	}

	for (int i = 0; i < N; ++i) {
		if (!Connect_And_Login(*mgrs[i], url, nicks[i].c_str())) {
			std::fprintf(stderr, "login failed for %s\n", nicks[i].c_str()); return 1;
		}
		std::printf("%s logged in as cid=%u\n", nicks[i].c_str(), mgrs[i]->My_Client_Id());
	}

	uint8_t buf[4096];
	int blen;

	// Player 0 creates the game.
	if (!mgrs[0]->Send_Game_Create("MULTI", uint8_t(N))) return 1;

	// Drain P0's confirms.
	blen = sizeof(buf); if (!Wait_Ctrl(*mgrs[0], OP_GAME_LIST_REPLY, buf, &blen, 2000)) { std::fprintf(stderr, "p0 no list\n"); return 1; }
	blen = sizeof(buf); if (!Wait_Ctrl(*mgrs[0], OP_GAME_MEMBERS, buf, &blen, 2000)) { std::fprintf(stderr, "p0 no members\n"); return 1; }
	uint8_t const *p = buf;
	uint32_t game_id = Read_U32(p);
	std::printf("game_id=%u\n", game_id);

	// Others drain initial list and join.
	for (int i = 1; i < N; ++i) {
		blen = sizeof(buf); if (!Wait_Ctrl(*mgrs[i], OP_GAME_LIST_REPLY, buf, &blen, 2000)) return 1;
		if (!mgrs[i]->Send_Game_Join(game_id)) return 1;
	}

	// Wait for everyone's view of 5 broadcasts of list-reply to settle, then
	// extract the final member roster from the last GAME_MEMBERS seen by P0.
	// Simpler: pump & drain for 400ms, then each peer asks for latest members
	// by reading the most recent GAME_MEMBERS in their queue.
	for (int t = 0; t < 80; ++t) {
		for (int i = 0; i < N; ++i) mgrs[i]->Service();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// Each peer drains its ctrl queue, remembering the last GAME_MEMBERS.
	std::vector<std::vector<uint32_t>> rosters(N);
	for (int i = 0; i < N; ++i) {
		uint8_t last_members[4096]; int last_len = 0;
		for (;;) {
			blen = sizeof(buf);
			uint16_t op = mgrs[i]->Next_Control_Frame(buf, &blen);
			if (op == 0) break;
			if (op == OP_GAME_MEMBERS) { std::memcpy(last_members, buf, blen); last_len = blen; }
		}
		if (last_len == 0) { std::fprintf(stderr, "peer %d never saw GAME_MEMBERS\n", i); return 1; }
		p = last_members;
		Read_U32(p); // game_id
		Read_U32(p); // host_id
		uint16_t cnt = Read_U16(p);
		for (int k = 0; k < cnt; ++k) {
			uint32_t cid = Read_U32(p);
			uint16_t nl = Read_U16(p);
			p += nl;
			rosters[i].push_back(cid);
		}
		std::printf("P%d sees %d members\n", i, (int)rosters[i].size());
		if ((int)rosters[i].size() != N) { std::fprintf(stderr, "P%d: expected %d members, got %d\n", i, N, (int)rosters[i].size()); return 1; }
	}

	// Build connection rosters: each peer creates connections to all others.
	for (int i = 0; i < N; ++i) {
		int conn_id = 1;
		for (uint32_t cid : rosters[i]) {
			if (cid == mgrs[i]->My_Client_Id()) continue;
			char name[16]; std::snprintf(name, sizeof(name), "peer%u", cid);
			mgrs[i]->Create_Connection_By_Cid(conn_id++, name, cid);
		}
	}

	// P0 starts the game.
	mgrs[0]->Send_Game_Start();
	for (int i = 0; i < N; ++i) {
		blen = sizeof(buf);
		if (!Wait_Ctrl(*mgrs[i], OP_GAME_STARTED, buf, &blen, 3000)) { std::fprintf(stderr, "P%d no GAME_STARTED\n", i); return 1; }
	}
	std::printf("all %d peers started\n", N);

	// Every peer broadcasts one unique message; expect each peer to receive
	// N-1 messages (from all other peers).
	for (int i = 0; i < N; ++i) {
		char msg[32]; std::snprintf(msg, sizeof(msg), "HELLO-FROM-%d", i);
		if (!mgrs[i]->Send_Private_Message(msg, int(std::strlen(msg)), 1, ConnManClass::CONNECTION_NONE)) return 1;
	}

	// Pump until everyone's private queues have N-1 messages (or timeout).
	int elapsed = 0;
	while (elapsed < 3000) {
		bool done = true;
		for (int i = 0; i < N; ++i) {
			mgrs[i]->Service();
			if (mgrs[i]->Private_Num_Receive() < N - 1) done = false;
		}
		if (done) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		elapsed += 10;
	}
	for (int i = 0; i < N; ++i) {
		int got = mgrs[i]->Private_Num_Receive();
		if (got != N - 1) { std::fprintf(stderr, "P%d got %d/%d broadcasts\n", i, got, N - 1); return 1; }
		std::printf("P%d received %d broadcasts:\n", i, got);
		for (int k = 0; k < N - 1; ++k) {
			char rbuf[256]; int rlen = sizeof(rbuf); int cid_out;
			if (!mgrs[i]->Get_Private_Message(rbuf, &rlen, &cid_out)) return 1;
			rbuf[rlen < 255 ? rlen : 255] = 0;
			std::printf("  [%s] (conn=%d)\n", rbuf, cid_out);
		}
	}

	Socket_Cleanup();
	std::printf("ok: %d-client multi-peer test passed\n", N);
	return 0;
}
