/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	Thin WebSocket client abstraction. One interface, two back-ends:
**	- Native: raw TCP via CODE/SOCKETS.H + local RFC 6455 handshake + framing.
**	- Emscripten: emscripten/websocket.h (browser's native WebSocket).
**
**	Only binary application frames are delivered to the caller. Control
**	frames (ping/pong/close) are handled internally. TLS is not supported
**	on the native path in v1; use ws:// or run behind a reverse proxy.
*/
#ifndef WSCLIENT_H
#define WSCLIENT_H

#include <stddef.h>
#include <stdint.h>

class WSClientClass
{
public:
	enum State {
		STATE_DISCONNECTED = 0,
		STATE_CONNECTING,
		STATE_OPEN,
		STATE_CLOSING,
		STATE_FAILED,
	};

	WSClientClass();
	~WSClientClass();

	WSClientClass(WSClientClass const &) = delete;
	WSClientClass &operator=(WSClientClass const &) = delete;

	/*.....................................................................
	Initiates a connection. `url` must look like "ws://host:port/path".
	Returns true if the connection attempt was started; the actual TCP
	+ handshake completes asynchronously during Poll().
	.....................................................................*/
	bool Connect(char const *url);

	/*.....................................................................
	Closes the connection (sends close frame on native, calls close on
	emscripten) and releases resources.
	.....................................................................*/
	void Close();

	/*.....................................................................
	Drives the state machine. Must be called regularly once per game
	tick. Returns the number of inbound binary messages now queued.
	.....................................................................*/
	int Poll();

	State Get_State() const { return State_; }
	bool Is_Open() const { return State_ == STATE_OPEN; }

	/*.....................................................................
	Send a single binary application message. Returns true on success.
	On native, data is buffered and flushed during Poll() if the send
	would block.
	.....................................................................*/
	bool Send_Binary(void const *data, size_t len);

	/*.....................................................................
	Pops the oldest received binary message. Returns true and fills
	(*out_data, *out_len) on success; caller must free(*out_data).
	Returns false if no message available.
	.....................................................................*/
	bool Receive_Binary(void **out_data, size_t *out_len);

	/// Returns the number of queued incoming binary messages.
	int Receive_Queue_Size() const;

private:
	State State_;

	// Pimpl-style opaque state differs by backend.
	void *Impl_;
};

#endif // WSCLIENT_H
