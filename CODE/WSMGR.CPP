/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	Implementation of WSManagerClass. Drives one WSClientClass and
**	routes RELAY_FROM frames to per-peer queues keyed by connection id.
*/
#include "WSMGR.H"
#include "UDPADDR.H"
#include "UDP.H"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace WolProto;

WSManagerClass Wol;

WSManagerClass::WSManagerClass()
	: Connected(false)
	, LoggedIn(false)
	, MyClientId(0)
	, ConnCount(0)
	, PrivHead(nullptr)
	, PrivTail(nullptr)
	, PendingPrivHead(nullptr)
	, PendingPrivTail(nullptr)
	, GlobalHead(nullptr)
	, GlobalTail(nullptr)
	, CtrlHead(nullptr)
	, CtrlTail(nullptr)
	, PrivCount(0)
	, PendingPrivCount(0)
	, GlobalCount(0)
	, CtrlCount(0)
{
	PendingUrl[0] = '\0';
	PendingNick[0] = '\0';
	ActiveUrl[0] = '\0';
	ActiveNick[0] = '\0';
	Clear_Peer_State();
}

WSManagerClass::~WSManagerClass()
{
	Shutdown();
	Clear_Queue(&PrivHead, &PrivTail);
	Clear_Queue(&PendingPrivHead, &PendingPrivTail);
	Clear_Queue(&GlobalHead, &GlobalTail);
	Clear_Queue(&CtrlHead, &CtrlTail);
}

void WSManagerClass::Cid_To_Address(uint32_t cid, UDPAddressClass *out)
{
	NetNumType net = {0, 0, 0, 0};
	NetNodeType node = {0, 0, 0, 0, 0, 0};
	node[0] = uint8_t(cid & 0xFF);
	node[1] = uint8_t((cid >> 8) & 0xFF);
	node[2] = uint8_t((cid >> 16) & 0xFF);
	node[3] = uint8_t((cid >> 24) & 0xFF);
	out->Set_Address(net, node);
}

uint32_t WSManagerClass::Address_To_Cid(UDPAddressClass *addr)
{
	NetNumType net;
	NetNodeType node;
	addr->Get_Address(net, node);
	return uint32_t(node[0])
		| (uint32_t(node[1]) << 8)
		| (uint32_t(node[2]) << 16)
		| (uint32_t(node[3]) << 24);
}

bool WSManagerClass::Connect(char const *ws_url)
{
	if (!Client.Connect(ws_url)) {
		Connected = false;
		return false;
	}
	Connected = true;
	return true;
}

bool WSManagerClass::Send_Raw_Frame(uint16_t opcode, void const *payload, size_t len)
{
	if (HEADER_SIZE + len > MAX_PAYLOAD) return false;
	uint8_t *buf = static_cast<uint8_t *>(std::malloc(HEADER_SIZE + len));
	if (!buf) return false;
	Encode_Header(buf, opcode, 0, uint32_t(len));
	if (len > 0) std::memcpy(buf + HEADER_SIZE, payload, len);
	bool ok = Client.Send_Binary(buf, HEADER_SIZE + len);
	std::free(buf);
	return ok;
}

bool WSManagerClass::Send_Hello(char const *nick)
{
	uint16_t nick_len = uint16_t(std::strlen(nick));
	size_t payload_len = 4 + 2 + nick_len;
	uint8_t *p = static_cast<uint8_t *>(std::malloc(payload_len));
	if (!p) return false;
	uint8_t *w = p;
	Write_U32(w, PROTOCOL_VERSION);
	Write_U16(w, nick_len);
	std::memcpy(w, nick, nick_len);
	bool ok = Send_Raw_Frame(OP_HELLO, p, payload_len);
	std::free(p);
	return ok;
}

bool WSManagerClass::Send_Game_Create(char const *name, uint8_t max_players)
{
	uint16_t nl = uint16_t(std::strlen(name));
	size_t payload_len = 2 + nl + 1;
	uint8_t *p = static_cast<uint8_t *>(std::malloc(payload_len));
	if (!p) return false;
	uint8_t *w = p;
	Write_U16(w, nl);
	std::memcpy(w, name, nl); w += nl;
	Write_U8(w, max_players);
	bool ok = Send_Raw_Frame(OP_GAME_CREATE, p, payload_len);
	std::free(p);
	return ok;
}

bool WSManagerClass::Send_Game_Join(uint32_t game_id)
{
	uint8_t buf[4];
	uint8_t *w = buf;
	Write_U32(w, game_id);
	return Send_Raw_Frame(OP_GAME_JOIN, buf, 4);
}

bool WSManagerClass::Send_Game_Leave()
{
	return Send_Raw_Frame(OP_GAME_LEAVE, nullptr, 0);
}

bool WSManagerClass::Send_Game_Start()
{
	return Send_Raw_Frame(OP_GAME_START, nullptr, 0);
}

bool WSManagerClass::Send_Channel_Join(char const *channel_name)
{
	uint16_t nl = uint16_t(std::strlen(channel_name));
	size_t payload_len = 2 + nl;
	uint8_t *p = static_cast<uint8_t *>(std::malloc(payload_len));
	if (!p) return false;
	uint8_t *w = p;
	Write_U16(w, nl);
	std::memcpy(w, channel_name, nl);
	bool ok = Send_Raw_Frame(OP_CHANNEL_JOIN, p, payload_len);
	std::free(p);
	return ok;
}

int WSManagerClass::Create_Connection_By_Cid(int id, char const *name, uint32_t remote_cid)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!Conns[i].Used) {
			Conns[i].Used = true;
			Conns[i].Id = id;
			Conns[i].RemoteCid = remote_cid;
			Cid_To_Address(remote_cid, &Conns[i].Address);
			std::strncpy(Conns[i].Name, name ? name : "", CONN_NAME_MAX - 1);
			Conns[i].Name[CONN_NAME_MAX - 1] = '\0';
			if (i >= ConnCount) ConnCount = i + 1;
			Move_Pending_Private_For_Cid(remote_cid, id);
			return 1;
		}
	}
	return 0;
}

int WSManagerClass::Create_Connection(int id, char *name, UDPAddressClass *address)
{
	uint32_t cid = address ? Address_To_Cid(address) : 0;
	return Create_Connection_By_Cid(id, name, cid);
}

int WSManagerClass::Delete_Connection(int id)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].Id == id) {
			Purge_Private_For_Conn(id);
			Purge_Pending_Private_For_Cid(Conns[i].RemoteCid);
			Conns[i].Used = false;
			Conns[i].Id = -1;
			Conns[i].RemoteCid = 0;
			Conns[i].Name[0] = '\0';
			return 1;
		}
	}
	return 0;
}

int WSManagerClass::Num_Connections()
{
	int n = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) if (Conns[i].Used) ++n;
	return n;
}

int WSManagerClass::Connection_ID(int index)
{
	int n = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!Conns[i].Used) continue;
		if (n == index) return Conns[i].Id;
		++n;
	}
	return CONNECTION_NONE;
}

int WSManagerClass::Connection_Index(int id)
{
	int n = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (!Conns[i].Used) continue;
		if (Conns[i].Id == id) return n;
		++n;
	}
	return -1;
}

char *WSManagerClass::Connection_Name(int id)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].Id == id) return Conns[i].Name;
	}
	return (char *)"";
}

UDPAddressClass *WSManagerClass::Connection_Address(int id)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].Id == id) return &Conns[i].Address;
	}
	return nullptr;
}

uint32_t WSManagerClass::Connection_Remote_Cid(int id)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].Id == id) return Conns[i].RemoteCid;
	}
	return 0;
}

int WSManagerClass::Cid_To_Conn_Id(uint32_t cid) const
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].RemoteCid == cid) return Conns[i].Id;
	}
	return CONNECTION_NONE;
}

void WSManagerClass::Push_Priv(int conn_id, uint8_t const *data, int len)
{
	if (len < 0 || len > MAX_MSG_LEN) return;
	if (PrivCount >= MAX_QUEUED_MSGS) return;
	Msg *m = static_cast<Msg *>(std::malloc(sizeof(Msg)));
	if (!m) return;
	m->Data = static_cast<uint8_t *>(std::malloc(len > 0 ? len : 1));
	if (!m->Data) { std::free(m); return; }
	if (len > 0) std::memcpy(m->Data, data, len);
	m->Len = len;
	m->ConnId = conn_id;
	m->SourceCid = 0;
	m->Opcode = OP_RELAY_FROM;
	m->Next = nullptr;
	if (PrivTail) PrivTail->Next = m; else PrivHead = m;
	PrivTail = m;
	++PrivCount;
}

void WSManagerClass::Push_Pending_Priv(uint32_t source_cid, uint8_t const *data, int len)
{
	if (len < 0 || len > MAX_MSG_LEN) return;
	if (PendingPrivCount >= MAX_QUEUED_MSGS) return;
	Msg *m = static_cast<Msg *>(std::malloc(sizeof(Msg)));
	if (!m) return;
	m->Data = static_cast<uint8_t *>(std::malloc(len > 0 ? len : 1));
	if (!m->Data) { std::free(m); return; }
	if (len > 0) std::memcpy(m->Data, data, len);
	m->Len = len;
	m->ConnId = CONNECTION_NONE;
	m->SourceCid = source_cid;
	m->Opcode = OP_RELAY_FROM;
	m->Next = nullptr;
	if (PendingPrivTail) PendingPrivTail->Next = m; else PendingPrivHead = m;
	PendingPrivTail = m;
	++PendingPrivCount;
}

void WSManagerClass::Push_Global(uint32_t source_cid, uint8_t const *data, int len)
{
	if (len < 0 || len > MAX_MSG_LEN) return;
	if (GlobalCount >= MAX_QUEUED_MSGS) return;
	Msg *m = static_cast<Msg *>(std::malloc(sizeof(Msg)));
	if (!m) return;
	m->Data = static_cast<uint8_t *>(std::malloc(len > 0 ? len : 1));
	if (!m->Data) { std::free(m); return; }
	if (len > 0) std::memcpy(m->Data, data, len);
	m->Len = len;
	m->ConnId = CONNECTION_NONE;
	m->SourceCid = source_cid;
	m->Opcode = OP_RELAY_FROM;
	m->Next = nullptr;
	if (GlobalTail) GlobalTail->Next = m; else GlobalHead = m;
	GlobalTail = m;
	++GlobalCount;
}

void WSManagerClass::Push_Ctrl(uint16_t opcode, uint8_t const *data, int len)
{
	if (len < 0 || len > MAX_MSG_LEN) return;
	if (CtrlCount >= MAX_QUEUED_MSGS) return;
	Msg *m = static_cast<Msg *>(std::malloc(sizeof(Msg)));
	if (!m) return;
	m->Data = static_cast<uint8_t *>(std::malloc(len > 0 ? len : 1));
	if (!m->Data) { std::free(m); return; }
	if (len > 0) std::memcpy(m->Data, data, len);
	m->Len = len;
	m->ConnId = CONNECTION_NONE;
	m->SourceCid = 0;
	m->Opcode = opcode;
	m->Next = nullptr;
	if (CtrlTail) CtrlTail->Next = m; else CtrlHead = m;
	CtrlTail = m;
	++CtrlCount;
}

WSManagerClass::Msg *WSManagerClass::Pop(Msg **head, Msg **tail)
{
	Msg *m = *head;
	if (!m) return nullptr;
	*head = m->Next;
	if (*head == nullptr) *tail = nullptr;
	return m;
}

void WSManagerClass::Clear_Queue(Msg **head, Msg **tail)
{
	while (*head) {
		Msg *n = (*head)->Next;
		std::free((*head)->Data);
		std::free(*head);
		*head = n;
	}
	*tail = nullptr;
}

void WSManagerClass::Purge_Private_For_Conn(int conn_id)
{
	Msg *prev = nullptr;
	Msg *cur = PrivHead;

	while (cur) {
		Msg *next = cur->Next;
		if (cur->ConnId == conn_id) {
			if (prev) {
				prev->Next = next;
			} else {
				PrivHead = next;
			}
			if (PrivTail == cur) {
				PrivTail = prev;
			}
			std::free(cur->Data);
			std::free(cur);
			--PrivCount;
		} else {
			prev = cur;
		}
		cur = next;
	}
}

void WSManagerClass::Purge_Pending_Private_For_Cid(uint32_t source_cid)
{
	Msg *prev = nullptr;
	Msg *cur = PendingPrivHead;

	while (cur) {
		Msg *next = cur->Next;
		if (cur->SourceCid == source_cid) {
			if (prev) {
				prev->Next = next;
			} else {
				PendingPrivHead = next;
			}
			if (PendingPrivTail == cur) {
				PendingPrivTail = prev;
			}
			std::free(cur->Data);
			std::free(cur);
			--PendingPrivCount;
		} else {
			prev = cur;
		}
		cur = next;
	}
}

void WSManagerClass::Move_Pending_Private_For_Cid(uint32_t source_cid, int conn_id)
{
	Msg *prev = nullptr;
	Msg *cur = PendingPrivHead;

	while (cur) {
		Msg *next = cur->Next;
		if (cur->SourceCid == source_cid) {
			if (prev) {
				prev->Next = next;
			} else {
				PendingPrivHead = next;
			}
			if (PendingPrivTail == cur) {
				PendingPrivTail = prev;
			}
			--PendingPrivCount;

			cur->ConnId = conn_id;
			cur->Next = nullptr;
			if (PrivCount < MAX_QUEUED_MSGS) {
				if (PrivTail) PrivTail->Next = cur; else PrivHead = cur;
				PrivTail = cur;
				++PrivCount;
			} else {
				std::free(cur->Data);
				std::free(cur);
			}
		} else {
			prev = cur;
		}
		cur = next;
	}
}

void WSManagerClass::Clear_Peer_State(void)
{
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		Conns[i].Used = false;
		Conns[i].Id = -1;
		Conns[i].RemoteCid = 0;
		Conns[i].Name[0] = '\0';
	}
	ConnCount = 0;
}

void WSManagerClass::Clear_Message_State(void)
{
	Clear_Queue(&PrivHead, &PrivTail);
	Clear_Queue(&PendingPrivHead, &PendingPrivTail);
	Clear_Queue(&GlobalHead, &GlobalTail);
	Clear_Queue(&CtrlHead, &CtrlTail);
	PrivCount = 0;
	PendingPrivCount = 0;
	GlobalCount = 0;
	CtrlCount = 0;
}

void WSManagerClass::Reset_State(void)
{
	Clear_Peer_State();
	Connected = false;
	LoggedIn = false;
	MyClientId = 0;
	Clear_Message_State();
	ActiveUrl[0] = '\0';
	ActiveNick[0] = '\0';
}

uint16_t WSManagerClass::Next_Control_Frame(void *out_buf, int *out_len)
{
	Msg *m = Pop(&CtrlHead, &CtrlTail);
	if (!m) return 0;
	--CtrlCount;
	uint16_t op = m->Opcode;
	int copy = m->Len;
	if (*out_len < copy) copy = *out_len;
	std::memcpy(out_buf, m->Data, copy);
	*out_len = copy;
	std::free(m->Data);
	std::free(m);
	return op;
}

int WSManagerClass::Init()
{
	if (Connected && LoggedIn
		&& std::strcmp(ActiveUrl, PendingUrl) == 0
		&& std::strcmp(ActiveNick, PendingNick) == 0) {
		if (PrivHead || GlobalHead || CtrlHead || Num_Connections() > 0) {
			Return_To_Lobby();
		}
		return 1;
	}

	if (Connected || LoggedIn || PrivHead || GlobalHead || CtrlHead || Num_Connections() > 0) {
		Shutdown();
	}

#ifdef __EMSCRIPTEN__
	if (!emscripten_has_asyncify()) {
		std::fprintf(stderr, "[WOL] emscripten asyncify is required for blocking Init()\n");
		return 0;
	}
#endif

	// If not yet connected, start the connection and pump until WELCOME.
	if (!Connected) {
		if (PendingUrl[0] == '\0') {
			std::fprintf(stderr, "[WOL] Init: no server URL configured\n");
			return 0;
		}
		std::fprintf(stderr, "[WOL] connecting to %s as \"%s\"\n",
			PendingUrl, PendingNick[0] ? PendingNick : "(anon)");
		if (!Connect(PendingUrl)) {
			std::fprintf(stderr, "[WOL] Connect(%s) failed\n", PendingUrl);
			return 0;
		}
	}

	// Wait for server welcome; send HELLO as soon as the socket is open.
	bool hello_sent = false;
	for (int spin = 0; spin < 600 && !LoggedIn; ++spin) {
		Client.Poll();
		if (Client.Get_State() == WSClientClass::STATE_FAILED || Client.Get_State() == WSClientClass::STATE_DISCONNECTED) {
			std::fprintf(stderr, "[WOL] socket closed before WELCOME (handshake failed?)\n");
			Connected = false;
			return 0;
		}
		if (!hello_sent && Client.Is_Open()) {
			std::fprintf(stderr, "[WOL] socket open, sending HELLO\n");
			Send_Hello(PendingNick);
			hello_sent = true;
		}
		Service();
#ifdef __EMSCRIPTEN__
		emscripten_sleep(10);
#else
		struct timespec ts = { 0, 10 * 1000 * 1000 };
		nanosleep(&ts, nullptr);
#endif
	}
	if (LoggedIn) {
		std::strncpy(ActiveUrl, PendingUrl, sizeof(ActiveUrl) - 1);
		ActiveUrl[sizeof(ActiveUrl) - 1] = '\0';
		std::strncpy(ActiveNick, PendingNick, sizeof(ActiveNick) - 1);
		ActiveNick[sizeof(ActiveNick) - 1] = '\0';
		std::fprintf(stderr, "[WOL] logged in, cid=%u; joining lobby channel\n", MyClientId);
		Send_Channel_Join("ra-lobby");
		// Drain briefly so CHANNEL_JOINED arrives before we return. We don't
		// block on it — the channel-joined is primarily needed so the next
		// RELAY_BROADCAST from the client has scope on the server.
		for (int spin = 0; spin < 50; ++spin) {
			Client.Poll();
			Service();
#ifdef __EMSCRIPTEN__
			emscripten_sleep(10);
#else
			struct timespec ts = { 0, 10 * 1000 * 1000 };
			nanosleep(&ts, nullptr);
#endif
		}
	} else {
		std::fprintf(stderr, "[WOL] timeout waiting for WELCOME (hello_sent=%d, open=%d)\n",
			hello_sent, Client.Is_Open() ? 1 : 0);
	}
	return LoggedIn ? 1 : 0;
}

void WSManagerClass::Configure(char const *ws_url, char const *nick)
{
	if (ws_url && ws_url[0]) {
		// Normalize: if caller didn't include a path, default to /ws so users
		// can just pass ws://host:port.
		char const *scheme_end = std::strstr(ws_url, "://");
		char const *host_start = scheme_end ? scheme_end + 3 : ws_url;
		char const *path = std::strchr(host_start, '/');
		if (path == nullptr) {
			std::snprintf(PendingUrl, sizeof(PendingUrl), "%s/ws", ws_url);
		} else {
			std::strncpy(PendingUrl, ws_url, sizeof(PendingUrl) - 1);
			PendingUrl[sizeof(PendingUrl) - 1] = '\0';
		}
	} else {
		PendingUrl[0] = '\0';
	}
	if (nick) {
		std::strncpy(PendingNick, nick, sizeof(PendingNick) - 1);
		PendingNick[sizeof(PendingNick) - 1] = '\0';
	} else {
		PendingNick[0] = '\0';
	}
}

int WSManagerClass::Service()
{
	if (!Connected) return 0;
	Client.Poll();

	void *msg;
	size_t mlen;
	while (Client.Receive_Binary(&msg, &mlen)) {
		ParsedFrame pf;
		if (!Parse_Frame(static_cast<uint8_t *>(msg), mlen, &pf)) {
			std::free(msg);
			continue;
		}
		switch (pf.opcode) {
		case OP_WELCOME: {
			if (pf.payload_len < 8) {
				std::fprintf(stderr, "[WOL] short WELCOME payload (%u bytes)\n",
					unsigned(pf.payload_len));
				std::free(msg);
				Client.Close();
				Reset_State();
				return 0;
			}
			uint8_t const *p = pf.payload;
			MyClientId = Read_U32(p);
			Read_U32(p); // server_version
			LoggedIn = true;
			Push_Ctrl(pf.opcode, pf.payload, int(pf.payload_len));
			break;
		}
		case OP_RELAY_FROM: {
			if (pf.payload_len < 5) break;
			uint8_t const *p = pf.payload;
			uint32_t source_cid = Read_U32(p);
			uint8_t channel = *p++;
			int plen = int(pf.payload_len) - 5;
			if (channel == CH_GLOBAL) {
				Push_Global(source_cid, p, plen);
			} else {
				int conn_id = Cid_To_Conn_Id(source_cid);
				if (conn_id != CONNECTION_NONE) {
					Push_Priv(conn_id, p, plen);
				} else {
					Push_Pending_Priv(source_cid, p, plen);
				}
			}
			break;
		}
		case OP_PEER_LEFT: {
			if (pf.payload_len >= 4) {
				uint8_t const *p = pf.payload;
				uint32_t cid = Read_U32(p);
				Purge_Pending_Private_For_Cid(cid);
				int conn_id = Cid_To_Conn_Id(cid);
				if (conn_id != CONNECTION_NONE) Delete_Connection(conn_id);
			}
			Push_Ctrl(pf.opcode, pf.payload, int(pf.payload_len));
			break;
		}
		default:
			Push_Ctrl(pf.opcode, pf.payload, int(pf.payload_len));
			break;
		}
		std::free(msg);
	}
	return 0;
}

int WSManagerClass::Send_Private_Message(void *buf, int buflen, int /*ack_req*/, int conn_id)
{
	if (buflen < 0 || buflen > MAX_MSG_LEN) return 0;
	if (!LoggedIn) return 0;
	// Prepend private-channel tag byte.
	size_t payload_len = 1 + size_t(buflen);

	if (conn_id == CONNECTION_NONE) {
		uint8_t *p = static_cast<uint8_t *>(std::malloc(payload_len));
		if (!p) return 0;
		p[0] = CH_PRIVATE;
		if (buflen > 0) std::memcpy(p + 1, buf, buflen);
		bool ok = Send_Raw_Frame(OP_RELAY_BROADCAST, p, payload_len);
		std::free(p);
		return ok ? 1 : 0;
	}

	uint32_t cid = 0;
	for (int i = 0; i < MAX_CONNECTIONS; ++i) {
		if (Conns[i].Used && Conns[i].Id == conn_id) { cid = Conns[i].RemoteCid; break; }
	}
	if (cid == 0) return 0;
	size_t full_len = 4 + payload_len;
	uint8_t *p = static_cast<uint8_t *>(std::malloc(full_len));
	if (!p) return 0;
	uint8_t *w = p;
	Write_U32(w, cid);
	*w++ = CH_PRIVATE;
	if (buflen > 0) std::memcpy(w, buf, buflen);
	bool ok = Send_Raw_Frame(OP_RELAY_PRIVATE, p, full_len);
	std::free(p);
	return ok ? 1 : 0;
}

int WSManagerClass::Get_Private_Message(void *buf, int *buflen, int *conn_id)
{
	Msg *m = Pop(&PrivHead, &PrivTail);
	if (!m) return 0;
	--PrivCount;
	int capacity = (buflen != nullptr) ? *buflen : 0;
	if (buf == nullptr || buflen == nullptr || conn_id == nullptr || m->Len < 0 || capacity < m->Len) {
		std::fprintf(stderr, "[WOL] dropping private packet len=%d (buf cap=%d)\n", m->Len, capacity);
		if (buflen) *buflen = m->Len;
		std::free(m->Data);
		std::free(m);
		return 0;
	}
	std::memcpy(buf, m->Data, size_t(m->Len));
	*buflen = m->Len;
	*conn_id = m->ConnId;
	std::free(m->Data);
	std::free(m);
	return 1;
}

int WSManagerClass::Send_Global_Message(void *buf, int buflen, int /*ack_req*/, UDPAddressClass *address)
{
	if (buflen < 0 || buflen > MAX_MSG_LEN) return 0;
	if (!LoggedIn) return 0;
	size_t payload_len = 1 + size_t(buflen);

	// Address == NULL means broadcast to all peers.
	if (address == nullptr) {
		uint8_t *p = static_cast<uint8_t *>(std::malloc(payload_len));
		if (!p) return 0;
		p[0] = CH_GLOBAL;
		if (buflen > 0) std::memcpy(p + 1, buf, buflen);
		bool ok = Send_Raw_Frame(OP_RELAY_BROADCAST, p, payload_len);
		std::free(p);
		return ok ? 1 : 0;
	}

	uint32_t cid = Address_To_Cid(address);
	if (cid == 0) return 0;
	size_t full_len = 4 + payload_len;
	uint8_t *p = static_cast<uint8_t *>(std::malloc(full_len));
	if (!p) return 0;
	uint8_t *w = p;
	Write_U32(w, cid);
	*w++ = CH_GLOBAL;
	if (buflen > 0) std::memcpy(w, buf, buflen);
	bool ok = Send_Raw_Frame(OP_RELAY_PRIVATE, p, full_len);
	std::free(p);
	return ok ? 1 : 0;
}

int WSManagerClass::Get_Global_Message(void *buf, int *buflen, UDPAddressClass *address, uint16_t *product_id)
{
	Msg *m = Pop(&GlobalHead, &GlobalTail);
	if (!m) return 0;
	--GlobalCount;
	int capacity = (buflen != nullptr) ? *buflen : 0;
	if (buf == nullptr || buflen == nullptr || m->Len < 0 || capacity < m->Len) {
		std::fprintf(stderr, "[WOL] dropping global packet len=%d (buf cap=%d)\n", m->Len, capacity);
		if (buflen) *buflen = m->Len;
		std::free(m->Data);
		std::free(m);
		return 0;
	}
	std::memcpy(buf, m->Data, size_t(m->Len));
	*buflen = m->Len;
	if (address) Cid_To_Address(m->SourceCid, address);
	// Match the UDP global channel's product ID so NETDLG accepts the packet.
	if (product_id) *product_id = 0xaa00;
	std::free(m->Data);
	std::free(m);
	return 1;
}

int WSManagerClass::Global_Num_Send() { return 0; }
int WSManagerClass::Global_Num_Receive() { return GlobalCount; }
int WSManagerClass::Private_Num_Send(int /*id*/) { return 0; }
int WSManagerClass::Private_Num_Receive(int id)
{
	if (id == CONNECTION_NONE) return PrivCount;
	int n = 0;
	for (Msg *m = PrivHead; m; m = m->Next) if (m->ConnId == id) ++n;
	return n;
}

void WSManagerClass::Return_To_Lobby()
{
	if (!Connected || !LoggedIn) {
		Clear_Peer_State();
		Clear_Message_State();
		return;
	}

	Send_Game_Leave();
	for (int spin = 0; spin < 20; ++spin) {
		Service();
#ifdef __EMSCRIPTEN__
		emscripten_sleep(10);
#else
		struct timespec ts = { 0, 10 * 1000 * 1000 };
		nanosleep(&ts, nullptr);
#endif
	}

	Clear_Peer_State();
	Clear_Message_State();
}

void WSManagerClass::Shutdown()
{
	if (Connected) {
		if (LoggedIn) {
			Send_Game_Leave();
			for (int spin = 0; spin < 10; ++spin) {
				Client.Poll();
				void *msg;
				size_t mlen;
				while (Client.Receive_Binary(&msg, &mlen)) {
					std::free(msg);
				}
#ifdef __EMSCRIPTEN__
				emscripten_sleep(10);
#else
				struct timespec ts = { 0, 10 * 1000 * 1000 };
				nanosleep(&ts, nullptr);
#endif
			}
		}
		Client.Close();
	}
	Reset_State();
}

void WSManagerClass::Reset_Response_Time() {}
uint32_t WSManagerClass::Response_Time() { return 60; }
void WSManagerClass::Set_Timing(uint32_t, uint32_t, uint32_t) {}

void WSManagerClass::Configure_Debug(int, int, int, char const **, int, int) {}
#ifdef CHEAT_KEYS
void WSManagerClass::Mono_Debug_Print(int, int) {}
#endif
