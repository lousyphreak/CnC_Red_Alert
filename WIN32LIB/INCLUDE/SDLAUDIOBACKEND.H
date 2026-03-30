#ifndef SDLAUDIOBACKEND_H
#define SDLAUDIOBACKEND_H

#include "windows.h"

#include <cstdint>
#include <mutex>
#include <vector>

#define RAAUDIO_OK 0
#define RAAUDIOERR_BUFFERLOST ((HRESULT)-3000)
#define RAAUDIOERR_GENERIC ((HRESULT)-3001)

#define RAAUDIOPLAY_LOOPING 0x00000001U
#define RAAUDIOSTATUS_PLAYING 0x00000001U
#define RAAUDIOSTATUS_LOOPING 0x00000002U

#define RA_AUDIO_FORMAT_PCM 1

typedef struct AudioBufferFormat {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
} AudioBufferFormat;

class AudioBackendBuffer {
public:
	AudioBackendBuffer(class AudioBackendDevice *owner, DWORD buffer_bytes, const AudioBufferFormat *format, bool primary);
	~AudioBackendBuffer();
	HRESULT GetStatus(DWORD *status);
	HRESULT Stop(void);
	HRESULT Play(BOOL looping);
	HRESULT Lock(DWORD write_cursor, DWORD write_bytes, LPVOID *ptr1, DWORD *bytes1, LPVOID *ptr2, DWORD *bytes2);
	HRESULT Unlock(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2);
	HRESULT SetCurrentPosition(DWORD position);
	HRESULT GetCurrentPosition(DWORD *play_cursor, DWORD *write_cursor);
	HRESULT SetVolume(LONG volume);
	HRESULT Restore(void);
	HRESULT SetFormat(const AudioBufferFormat *format);
	HRESULT Release(void);

private:
	void MixInto(float *mix_buffer, int output_frames, int output_channels, int output_rate);

	friend class AudioBackendDevice;

	AudioBackendDevice *Owner;
	bool Primary;
	int RefCount;
	DWORD Status;
	DWORD Position;
	bool Locked;
	double PlaybackCursorFrames;
	LONG Volume;
	AudioBufferFormat FormatData;
	std::mutex Mutex;
	std::vector<uint8_t> Data;
};

class AudioBackendDevice {
public:
	AudioBackendDevice(void);
	~AudioBackendDevice();
	HRESULT SetWindow(HWND hwnd);
	HRESULT CreateBuffer(DWORD buffer_bytes, const AudioBufferFormat *format, BOOL primary, AudioBackendBuffer **buffer);
	HRESULT Release(void);

private:
	friend class AudioBackendBuffer;

	static void SDLCALL AudioCallback(void *userdata, struct SDL_AudioStream *stream, int additional_amount, int total_amount);

	void FeedAudio(struct SDL_AudioStream *stream, int additional_amount);
	void RemoveBuffer(AudioBackendBuffer *buffer);
	bool EnsureAudioStreamLocked(void);
	void UpdatePrimaryPlaybackLocked(bool playing);
	void RefreshPrimaryFormatLocked(void);

	int RefCount;
	HWND Window;
	std::mutex Mutex;
	struct SDL_AudioStream *Stream;
	struct SDL_AudioSpec MixSpec;
	bool PrimaryPlaying;
	AudioBackendBuffer *PrimaryBuffer;
	std::vector<AudioBackendBuffer *> SecondaryBuffers;
};

HRESULT CreateAudioBackendDevice(AudioBackendDevice **device);

#endif
