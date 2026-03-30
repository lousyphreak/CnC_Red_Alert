#ifndef RA_DSOUND_WRAPPER_H
#define RA_DSOUND_WRAPPER_H

#include "mmsystem.h"

#include <mutex>
#include <vector>

#define DS_OK 0
#define DSERR_BUFFERLOST ((HRESULT)-3000)
#define DSERR_GENERIC ((HRESULT)-3001)

#define DSSCL_PRIORITY 0x00000002U
#define DSSCL_EXCLUSIVE 0x00000001U

#define DSBCAPS_PRIMARYBUFFER 0x00000001U
#define DSBCAPS_CTRLVOLUME 0x00000002U

#define DSBPLAY_LOOPING 0x00000001U
#define DSBSTATUS_PLAYING 0x00000001U
#define DSBSTATUS_LOOPING 0x00000002U

#pragma pack(push, 4)
struct DSBUFFERDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
};
#pragma pack(pop)

class IDirectSoundBuffer {
public:
    IDirectSoundBuffer(class IDirectSound* owner, DWORD buffer_bytes, const WAVEFORMATEX* format, bool primary);
    ~IDirectSoundBuffer();
    HRESULT GetStatus(DWORD* status);
    HRESULT Stop();
    HRESULT Play(DWORD reserved1, DWORD reserved2, DWORD flags);
    HRESULT Lock(DWORD write_cursor, DWORD write_bytes, LPVOID* ptr1, DWORD* bytes1, LPVOID* ptr2, DWORD* bytes2, DWORD flags);
    HRESULT Unlock(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2);
    HRESULT SetCurrentPosition(DWORD position);
    HRESULT GetCurrentPosition(DWORD* play_cursor, DWORD* write_cursor);
    HRESULT SetVolume(LONG volume);
    HRESULT Restore();
    HRESULT SetFormat(const WAVEFORMATEX* format);
    HRESULT Release();

    const WAVEFORMATEX& Format() const;
    uint8_t* Data();
    DWORD Size() const;
private:
    void MixInto(float* mix_buffer, int output_frames, int output_channels, int output_rate);

    friend class IDirectSound;

    IDirectSound* owner_;
    bool primary_;
    int ref_count_;
    DWORD status_;
    DWORD position_;
    bool locked_;
    double playback_cursor_frames_;
    LONG volume_;
    WAVEFORMATEX format_;
    std::mutex mutex_;
    std::vector<uint8_t> data_;
};

class IDirectSound {
public:
    IDirectSound();
    ~IDirectSound();
    HRESULT SetCooperativeLevel(HWND hwnd, DWORD level);
    HRESULT CreateSoundBuffer(const DSBUFFERDESC* desc, IDirectSoundBuffer** buffer, LPVOID unknown_outer);
    HRESULT Release();
private:
    friend class IDirectSoundBuffer;

    static void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

    void FeedAudio(SDL_AudioStream* stream, int additional_amount);
    void RemoveBuffer(IDirectSoundBuffer* buffer);
    bool EnsureAudioStreamLocked();
    void UpdatePrimaryPlaybackLocked(bool playing);
    void RefreshPrimaryFormatLocked();

    int ref_count_;
    HWND window_;
    std::mutex mutex_;
    SDL_AudioStream* stream_;
    SDL_AudioSpec mix_spec_;
    bool primary_playing_;
    IDirectSoundBuffer* primary_buffer_;
    std::vector<IDirectSoundBuffer*> secondary_buffers_;
};

using LPDIRECTSOUND = IDirectSound*;
using LPDIRECTSOUNDBUFFER = IDirectSoundBuffer*;

extern "C" HRESULT DirectSoundCreate(LPVOID guid, LPDIRECTSOUND* direct_sound, LPVOID unknown_outer);

#endif
