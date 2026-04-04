/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header: /counterstrike/SESSION.H 4     3/10/97 6:23p Steve_tall $ */
/***************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 *                                                                         *
 *                 Project Name : Command & Conquer                        *
 *                                                                         *
 *                    File Name : SESSION.H                                *
 *                                                                         *
 *                   Programmer : Bill R. Randolph                         *
 *                                                                         *
 *                   Start Date : 11/30/95                                 *
 *                                                                         *
 *                  Last Update : November 30, 1995 [BRR]                  *
 *                                                                         *
 * The purpose of this class is to contain those variables & routines		*
 * specifically related to a multiplayer game.										*
 *                                                                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifndef SESSION_H
#define SESSION_H
#include <stdint.h>

#include "UDPADDR.H"
#include "MSGLIST.H"
#include "CONNECT.H"
//---------------------------------------------------------------------------
// Forward declarations
//---------------------------------------------------------------------------
class AircraftClass;
class AnimClass;
class BuildingClass;
class BulletClass;
class InfantryClass;
class UnitClass;
class CellClass;

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
//...........................................................................
// Various limiting values
//...........................................................................
#define	MPLAYER_BUILD_LEVEL_MAX		10		// max build level in multiplay
#define	MAX_MPLAYER_COLORS			8		// max # of colors

//...........................................................................
// Max sizes of packets we want to send
// The UDP packet's size is UDP's max size (546), rounded down to accommodate
// the max number of events possible.
//...........................................................................
#define	MAX_UDP_PACKET_SIZE			(((546 - sizeof(CommHeaderType)) / \
												sizeof(EventClass) ) * sizeof(EventClass))
#define	MAX_FILE_PACKET_SIZE			256

//...........................................................................
// Max length of player names fields; attempt to use the constant for the
// HouseClass, if it's been defined; otherwise, define it myself.
//...........................................................................
#ifdef HOUSE_NAME_MAX
#define	MPLAYER_NAME_MAX				HOUSE_NAME_MAX
#else
#define	MPLAYER_NAME_MAX				12		// max length of a player's name
#endif

//...........................................................................
// Values to control the multiplayer score screen
//...........................................................................
#define	MAX_MULTI_NAMES	8		// max # names (rows) on the score screen
#define	MAX_MULTI_GAMES	4		// max # games (columns) on the score screen

//...........................................................................
// Min value for MaxAhead on higher-latency multiplayer transports; only
// applies for COMM_PROTOCOL_MULTI_E_COMP.
//...........................................................................
#define HIGH_LATENCY_MIN_MAX_AHEAD	5
#define NETWORK_MIN_MAX_AHEAD			2

//...........................................................................
// Send period (in frames) for COMM_PROTOCOL_MULTI_E_COMP and above
//...........................................................................
#define DEFAULT_FRAME_SEND_RATE		3

#define	CREDITSBUF_MAX					5
#define	PACKET_TIMING_TIMEOUT		40		// ticks b/w sending a timing packet

//---------------------------------------------------------------------------
// Enums
//---------------------------------------------------------------------------
//...........................................................................
// Types of games; used to tell which protocol we're using
//...........................................................................
typedef enum GameEnum {
	GAME_NORMAL,									// not multiplayer
	GAME_UDP = 3,									// UDP Network game
	GAME_INTERNET = 4,							// Internet H2H
	GAME_SKIRMISH = 5,							// 1 plr vs. AI's
	GAME_TEN = 6,									// TEN Network game
	GAME_MPATH = 7								// MPath Network game
} GameType;

//...........................................................................
// Commands sent over the network Global Channel
//...........................................................................
typedef enum NetCommandType {
	NET_QUERY_GAME,			// Hey, what games are out there?
	NET_ANSWER_GAME,			// Yo, Here's my game's name!
	NET_QUERY_PLAYER,			// Hey, what players are in this game?
	NET_ANSWER_PLAYER,		// Yo, I'm in that game!
	NET_CHAT_ANNOUNCE,		// I'm at the chat screen
	NET_CHAT_REQUEST,			// Respond with a CHAT_ANNOUNCE, please.
	NET_QUERY_JOIN,			// Hey guys, can I play too?
	NET_CONFIRM_JOIN,			// Well, OK, if you really want to.
	NET_REJECT_JOIN,			// No, you can't join; sorry, dude.
	NET_GAME_OPTIONS,			// Hey, dudes, here's some new game options
	NET_SIGN_OFF,				// Bogus, dudes, my boss is coming; I'm outta here!
	NET_GO,						// OK, jump into the game loop!
	NET_MESSAGE,				// Here's a message
	NET_PING,					// I'm pinging you to take a time measurement
	NET_LOADGAME,				// start a game by loading a saved game
	NET_REQ_SCENARIO  =1000,// Reqest that host sends the scenario file to the other players.
	NET_FILE_INFO		=1001,// Info about the file that is going to be transferred
	NET_FILE_CHUNK		=1002,// A chunk of scenario
	NET_READY_TO_GO	=1003,// Sent in response to a 'GO' command
	NET_NO_SCENARIO	=1004	// Scenario isnt available on remote machine so we cant play
} NetCommandType;

//---------------------------------------------------------------------------
// Structures
//---------------------------------------------------------------------------
//...........................................................................
// An entry on the score screen is defined by this structure
//...........................................................................
typedef struct {
	char Name[MPLAYER_NAME_MAX];
	int Wins;
	int Kills[MAX_MULTI_GAMES];
	PlayerColorType Color;
} MPlayerScoreType;

//...........................................................................
//	This is a "node", used for the lists of available games & players.  The
//	'Game' structure is used for games; the 'Player' structure for players.
//...........................................................................
typedef struct NodeNameTag {
	char Name[MPLAYER_NAME_MAX];		// player or game name
		UDPAddressClass Address;
#if(TEN)
		int TenAddress;
#endif
#if(MPATH)
		int MPathAddress;
#endif
	union {
		struct {
			uint8_t IsOpen;		// is the game open?
			uint32_t LastTime;		// last time we heard from this guy
		} Game;
		struct {
			HousesType House;				// "ActLike" House of this player
			PlayerColorType Color;		// Color of this player
			HousesType ID;					// Actual House of this player
			int ProcessTime;				// Length of time to process players main loop
		} Player;
		struct {
			uint32_t LastTime;		// last time we heard from this guy
			uint8_t LastChance;	// we're about to remove him from the list
			PlayerColorType Color;		// chat player's color
		} Chat;
	};
} NodeNameType;


//...........................................................................
// Packet sent over the network global channel (for file transfers)
//...........................................................................
#define MAX_SEND_FILE_PACKET_SIZE MAX_FILE_PACKET_SIZE - 64
typedef struct {
	NetCommandType	Command;					// Enum defined above. Should be a file transfer enum.
	uint16_t 	BlockNumber;			// Index position of this file chunk in the file
	uint16_t		BlockLength;			// Length of data in the RawData buffer
	uint8_t 		RawData	[MAX_SEND_FILE_PACKET_SIZE];
} RemoteFileTransferType;


//...........................................................................
// Packet sent over the network Global Channel
//...........................................................................
typedef struct GlobalPacketType {
	NetCommandType Command;						// One of the enum's defined above
	char Name[MPLAYER_NAME_MAX];				// Player or Game Name
	union {
		struct {
			uint32_t IsOpen		: 1;		// 1 = game is open for joining
		} GameInfo;
		struct {
			HousesType House;						// player's House
			PlayerColorType Color;				// player's color
			uint32_t NameCRC;				// CRC of player's game's name
			uint32_t MinVersion;			// game's min supported version
			uint32_t MaxVersion;			// game's max supported version
			int CheatCheck;						// Unique ID of "rules.ini" file.
		} PlayerInfo;
		struct {
			char Scenario[DESCRIP_MAX];		// Scenario Name
			uint32_t Credits;				// player's credits
			uint32_t IsBases		: 1;		// 1 = bases are allowed
			uint32_t IsTiberium	: 1;		// 1 = tiberium is allowed
			uint32_t IsGoodies	: 1;		// 1 = goodies are allowed
			uint32_t IsGhosties	: 1;		// 1 = ghosts are allowed
			uint32_t OfficialScenario :1;// Is this scenario an official Westwood one?
			uint8_t BuildLevel;			// buildable level
			uint8_t UnitCount;			// max # units
			uint8_t AIPlayers;			// # of AI players allowed
			int Seed;								// random number seed
			SpecialClass Special;				// command-line options
			uint32_t GameSpeed;				// Game Speed
			uint32_t Version;				// version # common to all players
			uint32_t FileLength;			// Length of scenario file to expect from host.
#ifdef WOLAPI_INTEGRATION
			char ShortFileName[13];				// Name of scenario file to expect from host
#else
			char ShortFileName[12];				// Name of scenario file to expect from host
#endif
			uint8_t FileDigest[32];		// Digest of scenario file to expect from host
												//	ajw - This is not necessarily null-terminated.
		} ScenarioInfo;
		struct {
			char Buf[MAX_MESSAGE_LENGTH];		// inter-user message
			PlayerColorType Color;					// color of sender of message
			uint32_t NameCRC;				// CRC of sender's Game Name
		} Message;
		struct {
			int OneWay;								// one-way response time
		} ResponseTime;
		struct {
			int Why;									// why were we rejected from the game?
		} Reject;
		struct {
			uint32_t ID;						// unique ID for this chat node
			PlayerColorType Color;				// my color
		} Chat;
	};
} GlobalPacketType;


//...........................................................................
// For finding sync bugs; filled in by the engine when certain conditions
// are met; the pointers allow examination of objects in the debugger.
//...........................................................................
typedef struct {
	union {
		AircraftClass *Aircraft;
		AnimClass *Anim;
		BuildingClass *Building;
		BulletClass *Bullet;
		InfantryClass *Infantry;
		UnitClass *Unit;
		void *All;
	} Ptr;
} TrapObjectType;

/*
**	This is the identifier for a multiplayer mission. This can be used to
**	identify the filename of the mission as well as display the mission in a
**	mission selection list.
*/
class MultiMission
{
	public:
		MultiMission(char const * filename = NULL, char const * description = NULL, char const *digest = NULL, bool official = true, bool expansion = false);

		void Set_Description(char const * description);
		void Set_Filename(char const * filename);
		void Set_Digest(char const * digest);
		void Set_Official(bool official);
		void Set_Expansion(bool expansion);
		void Draw_It(int index, int x, int y, int width, int height, bool selected, TextPrintType flags) const;
		char const * Description(void) const {return(ScenarioDescription);}
		char const * Get_Filename(void) const {return(Filename);}
		char const * Get_Digest(void) const {return(Digest);}
		bool Get_Official(void) { return (IsOfficial); }
		bool Get_Expansion(void) { return (IsExpansion); }	//	Implied "IsCounterstrike". No longer used. -ajw

	private:
		char ScenarioDescription[DESCRIP_MAX];
		char Filename[_MAX_FNAME+_MAX_EXT];
		char Digest[32];
		bool IsOfficial;
		bool IsExpansion;
};


typedef struct {
	int 	ScenarioIndex;		//Used on host machine only as index into scenario list
	int 	Bases;
	int 	Credits;
	int 	Tiberium;
	int 	Goodies;
	int 	Ghosts;
	int 	UnitCount;
	int 	AIPlayers;			// # of AI players allowed to be built
	char	ScenarioDescription [DESCRIP_MAX];	//Used on client machines only
} GameOptionsType;

//---------------------------------------------------------------------------
// Class Definition
//---------------------------------------------------------------------------
class SessionClass
{
	//------------------------------------------------------------------------
	// Public interface
	//------------------------------------------------------------------------
	public:
		//.....................................................................
		// Constructor/Destructor
		//.....................................................................
		SessionClass(void);
		~SessionClass(void);

		//.....................................................................
		// Initialization
		//.....................................................................
		void One_Time(void);
		void Init(void);

		//.....................................................................
		// Reads/writes to the INI file
		//.....................................................................
		void Read_MultiPlayer_Settings (void);
		void Write_MultiPlayer_Settings (void);
		void Read_Scenario_Descriptions (void);
		void Free_Scenario_Descriptions(void);

		//.....................................................................
		// Utility functions
		//.....................................................................
		int Create_Connections(void);
		bool Am_I_Master(void);
		uint32_t Compute_Unique_ID(void);

#if(TEN)
		int Create_TEN_Connections(void);
#endif	// TEN

#if(MPATH)
		int Create_MPATH_Connections(void);
#endif	// MPATH

		//.....................................................................
		// File I/O
		//.....................................................................
		int Save(Pipe & file) const;
		int Load(Straw & file);
		int Save(CCFileClass & file);
		int Load(CCFileClass & file);

		//.....................................................................
		// Debugging / Sync Bugs
		//.....................................................................
		void Trap_Object(void);

		//---------------------------------------------------------------------
		// Public Data
		//---------------------------------------------------------------------
		//.....................................................................
		// The type of session being played
		//.....................................................................
		GameType Type;

		//.....................................................................
		// The current communications protocol
		//.....................................................................
		CommProtocolType CommProtocol;

		//.....................................................................
		// Game options
		//.....................................................................
		GameOptionsType Options;

		//.....................................................................
		// Unique workstation ID, for detecting my own packets
		//.....................................................................
		uint32_t UniqueID;

		//.....................................................................
		// Player's local options
		//.....................................................................
		char Handle[MPLAYER_NAME_MAX];		// player name
		PlayerColorType PrefColor;				// preferred color index
		PlayerColorType ColorIdx;				// actual color index
		HousesType House;							// GDI / NOD
		int ObiWan;									// 1 = player can see all
		int Solo;									// 1 = player can play alone

		//.....................................................................
		// Max allowable # of players & actual # of (human) players
		//.....................................................................
		int MaxPlayers;
		int NumPlayers;

		//.....................................................................
		// Frame-sync'ing timing variables
		// 'MaxAhead' is the number of frames ahead of this one to execute
		// a given packet.  It's set by the RESPONSE_TIME event.
		// 'FrameSendRate' is the # frames between data packets
		//.....................................................................
		uint32_t MaxAhead;
		uint32_t FrameSendRate;

		int			DesiredFrameRate;

		int			ProcessTimer;
		int			ProcessTicks;
		int			ProcessFrames;

		//.....................................................................
		// This flag is set when we've loaded a multiplayer game.
		//.....................................................................
		int LoadGame;

		//.....................................................................
		// This flag is set when a multiplayer game saves due to a lost
		// connection.
		//.....................................................................
		int EmergencySave;

		//.....................................................................
		// List of scenarios & their file numbers
		//.....................................................................
		DynamicVectorClass<MultiMission *> Scenarios;
//		DynamicVectorClass <char *> Scenarios;
//		DynamicVectorClass <int> Filenum;

		char ScenarioFileName[_MAX_FNAME+_MAX_EXT+1];	//File name of scenario to load

		char ScenarioDigest [32];								//Digest of scenario to load
		uint32_t ScenarioFileLength;
		bool ScenarioIsOfficial;

		char ScenarioRequests[20];		//Which players requested scenario files
		int  RequestCount;
		UDPAddressClass	HostAddress;

		//.....................................................................
		// This is the multiplayer messaging system
		//.....................................................................
		MessageListClass Messages;
		UDPAddressClass MessageAddress;
		char LastMessage[MAX_MESSAGE_LENGTH];

		//.....................................................................
		// This is the multiplayer scorekeeping system
		//.....................................................................
		MPlayerScoreType Score[MAX_MULTI_NAMES];
		int GamesPlayed;		// # games played this run
		int NumScores;			// # active entries in MPlayerScore
		int Winner;				// index of winner of last game
		int CurGame;			// index of current game being played

		//.....................................................................
		// Static arrays
		//.....................................................................
		static char Descriptions[100][40];
		static int CountMin[2];
		static int CountMax[2];
		static char const * GlobalPacketNames[];

		//.....................................................................
		// For Recording & Playing back a file
		//.....................................................................
		CCFileClass RecordFile;
		unsigned Record				: 1;
		unsigned Play				 	: 1;
		unsigned Attract			 	: 1;

		//.....................................................................
		// UDP-specific variables
		//.....................................................................
		int IsBridge;								// 1 = we're crossing a bridge
		UDPAddressClass BridgeNet;				// address of bridge
		bool NetStealth;							// makes us invisible
		bool NetProtect;							// keeps others from messaging us
		bool NetOpen;								// 1 = game is open for joining
		char GameName[MPLAYER_NAME_MAX];		// game's name
		GlobalPacketType GPacket;				// global packet
		int GPacketlen;							// global packet length
		UDPAddressClass GAddress;				// address of sender
		uint16_t GProductID;				// product ID of sender
		char MetaPacket[MAX_UDP_PACKET_SIZE]; // packet building buffer
		int MetaSize;								// size of MetaPacket
		DynamicVectorClass <NodeNameType *> Games;	// list of games
		DynamicVectorClass <NodeNameType *> Players;	// list of players
		DynamicVectorClass <NodeNameType *> Chat;		// list of chat nodes

		//.....................................................................
		// For finding Sync Bugs
		//.....................................................................
		int32_t TrapFrame;				// frame # to start trapping 'TrapObject'
		RTTIType TrapObjType;		// type of object to trap
		TrapObjectType TrapObject;	// ptr to object to trap (watch)
		COORDINATE TrapCoord;		// coord of object, 0 = ignore
		TARGET TrapTarget;			// Target # of object, 0 = ignore
		CellClass * TrapCell;		// Ptr to cell to trap (watch)
		int TrapCheckHeap;			// true = check the heap as of TrapFrame
		int32_t TrapPrintCRC;			// Frame # to print CRC state file

#if(TEN)
		//
		// TEN-specific variables
		//
		char *TenPacket;
		int TenSize;
		int TenMessageAddress;
		int TenAddress;
		int TenPlayerID;
		char OptionsFile[256];
		int AllowSolo;
		int NetResponseTime;
#endif	// TEN

#if(MPATH)
		//
		// MPATH-specific variables
		//
		char *MPathPacket;
		int MPathSize;
		int MPathMessageAddress;
		int MPathAddress;
		char OptionsFile[256];
		int AllowSolo;
		int NetResponseTime;
#endif	// MPATH
};

#endif		// SESSION_H

/*************************** end of session.h ******************************/
