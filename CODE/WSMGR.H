/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	WSManagerClass: ConnManClass implementation backed by the Zig WOL
**	replacement server. Gameplay packets flow as RELAY_PRIVATE frames
**	between peers. The server is strictly a relay; this client holds the
**	roster and routes. Since the underlying transport (WebSocket over TCP)
**	is reliable and ordered, this class does no ack/retry of its own.
*/
#ifndef WSMGR_H
#define WSMGR_H

#include "CONNMGR.H"
#include "WSCLIENT.H"
#include "WOL_PROTO.H"
#include "UDPADDR.H"

#include <cstdint>
#include <cstddef>

class WSManagerClass : public ConnManClass
{
public:
	enum {
		MAX_CONNECTIONS = 8,
		CONN_NAME_MAX = 40,
		MAX_QUEUED_MSGS = 128,
		MAX_MSG_LEN = 64 * 1024,
	};

	/*
	** Channel tags prepended to relay payloads to multiplex the global
	** (lobby/discovery) and private (gameplay) streams over the single
	** relay channel provided by the server.
	*/
	enum Channel {
		CH_PRIVATE = 0,
		CH_GLOBAL = 1,
	};

	WSManagerClass();
	virtual ~WSManagerClass();

	/*.....................................................................
	Initiates the connection to the server. Returns true if the async
	connection attempt was started. Call Service() repeatedly to progress.
	.....................................................................*/
	bool Connect(char const *ws_url);

	/// Stage the URL + nickname to use on the next Init() call.
	void Configure(char const *ws_url, char const *nick);

	/// True once the server has sent WELCOME.
	bool Is_Logged_In() const { return LoggedIn; }

	/// Our client id assigned by the server (valid after WELCOME).
	uint32_t My_Client_Id() const { return MyClientId; }

	/*.....................................................................
	Send a login frame. Must be called after Connect() and before any
	other control-plane activity.
	.....................................................................*/
	bool Send_Hello(char const *nick);

	/*.....................................................................
	Control-plane helpers used by the command-line launch flow.
	.....................................................................*/
	bool Send_Game_Create(char const *name, uint8_t max_players);
	bool Send_Game_Join(uint32_t game_id);
	bool Send_Game_Leave();
	bool Send_Game_Start();
	bool Send_Channel_Join(char const *channel_name);
	void Return_To_Lobby();

	/*.....................................................................
	Cid-based connection helper, used by the control-plane flow which
	works in terms of remote client ids directly.
	.....................................................................*/
	int Create_Connection_By_Cid(int id, char const *name, uint32_t remote_cid);
	uint32_t Connection_Remote_Cid(int id);

	/*.....................................................................
	Pop one pending control-plane frame (non-relay opcodes). Returns the
	opcode and fills out_buf / *out_len. Returns 0 if none available.
	The payload length excludes the 8-byte header.
	.....................................................................*/
	uint16_t Next_Control_Frame(void *out_buf, int *out_len);

	/*.....................................................................
	Encode/decode a WOL cid into a UDPAddressClass. Cid is written little
	endian into NodeAddress[0..3]; NetworkNumber is zero, NodeAddress[4..5]
	are zero. This gives stable byte-wise equality and ordering.
	.....................................................................*/
	static void Cid_To_Address(uint32_t cid, UDPAddressClass *out);
	static uint32_t Address_To_Cid(UDPAddressClass *addr);

	/*.....................................................................
	ConnManClass overrides.
	.....................................................................*/
	virtual int Init();
	virtual int Service();

	virtual int Send_Private_Message(void *buf, int buflen,
		int ack_req = 1, int conn_id = CONNECTION_NONE);
	virtual int Get_Private_Message(void *buf, int *buflen, int *conn_id);

	virtual int Send_Global_Message(void *buf, int buflen, int ack_req = 0,
		UDPAddressClass *address = 0);
	virtual int Get_Global_Message(void *buf, int *buflen,
		UDPAddressClass *address, uint16_t *product_id);

	virtual int Create_Connection(int id, char *name, UDPAddressClass *address);
	virtual int Delete_Connection(int id);
	virtual int Num_Connections();
	virtual int Connection_ID(int index);
	virtual int Connection_Index(int id);
	virtual char *Connection_Name(int id);
	virtual UDPAddressClass *Connection_Address(int id);

	virtual int Global_Num_Send();
	virtual int Global_Num_Receive();
	virtual int Private_Num_Send(int id = CONNECTION_NONE);
	virtual int Private_Num_Receive(int id = CONNECTION_NONE);
	virtual void Shutdown();

	virtual void Reset_Response_Time();
	virtual uint32_t Response_Time();
	virtual uint32_t Global_Response_Time() { return 0; }
	virtual void Set_Timing(uint32_t retrydelta, uint32_t maxretries, uint32_t timeout);

	virtual void Configure_Debug(int index, int type_offset, int type_size,
		char const **names, int namestart, int namecount);
#ifdef CHEAT_KEYS
	virtual void Mono_Debug_Print(int index, int refresh);
#endif

private:
	struct Entry {
		int Id;
		uint32_t RemoteCid;
		char Name[CONN_NAME_MAX];
		UDPAddressClass Address;
		bool Used;
	};

	struct Msg {
		uint8_t *Data;
		int Len;
		int ConnId;          // CONNECTION_NONE for global/control frames
		uint32_t SourceCid;  // for global messages, the sender's cid
		uint16_t Opcode;     // original opcode (for control frames)
		Msg *Next;
	};

	void Push_Priv(int conn_id, uint8_t const *data, int len);
	void Push_Global(uint32_t source_cid, uint8_t const *data, int len);
	void Push_Ctrl(uint16_t opcode, uint8_t const *data, int len);
	Msg *Pop(Msg **head, Msg **tail);
	void Clear_Queue(Msg **head, Msg **tail);
	void Purge_Private_For_Conn(int conn_id);
	void Clear_Peer_State(void);
	void Clear_Message_State(void);
	void Reset_State(void);
	int Cid_To_Conn_Id(uint32_t cid) const;
	bool Send_Raw_Frame(uint16_t opcode, void const *payload, size_t len);

	WSClientClass Client;
	bool Connected;
	bool LoggedIn;
	uint32_t MyClientId;

	char PendingUrl[256];
	char PendingNick[64];
	char ActiveUrl[256];
	char ActiveNick[64];

	Entry Conns[MAX_CONNECTIONS];
	int ConnCount;

	Msg *PrivHead;
	Msg *PrivTail;
	Msg *GlobalHead;
	Msg *GlobalTail;
	Msg *CtrlHead;
	Msg *CtrlTail;
	int PrivCount;
	int GlobalCount;
	int CtrlCount;
};

extern WSManagerClass Wol;

#endif // WSMGR_H
