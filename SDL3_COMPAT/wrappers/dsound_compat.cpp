#include "dsound.h"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cstring>

namespace {

DWORD bytes_per_second(const WAVEFORMATEX& format)
{
    if (format.nAvgBytesPerSec != 0) {
        return format.nAvgBytesPerSec;
    }
    const DWORD block_align = format.nBlockAlign ? format.nBlockAlign : static_cast<DWORD>((format.wBitsPerSample / 8) * std::max<WORD>(1, format.nChannels));
    return format.nSamplesPerSec * block_align;
}

void update_position(IDirectSoundBuffer* buffer)
{
    DWORD dummy_play = 0;
    DWORD dummy_write = 0;
    buffer->GetCurrentPosition(&dummy_play, &dummy_write);
}

} // namespace

IDirectSoundBuffer::IDirectSoundBuffer(DWORD buffer_bytes, const WAVEFORMATEX* format, bool primary)
    : primary_(primary), ref_count_(1), status_(0), position_(0), volume_(0), data_(buffer_bytes ? buffer_bytes : 1, 0), last_tick_ms_(SDL_GetTicks())
{
    std::memset(&format_, 0, sizeof(format_));
    if (format) {
        format_ = *format;
    } else {
        format_.wFormatTag = WAVE_FORMAT_PCM;
        format_.nChannels = 2;
        format_.nSamplesPerSec = 22050;
        format_.wBitsPerSample = 8;
        format_.nBlockAlign = 2;
        format_.nAvgBytesPerSec = 44100;
        format_.cbSize = 0;
    }
}

HRESULT IDirectSoundBuffer::GetStatus(DWORD* status)
{
    update_position(this);
    if (status) {
        *status = status_;
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
    status_ = 0;
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Play(DWORD, DWORD, DWORD flags)
{
    last_tick_ms_ = SDL_GetTicks();
    status_ = DSBSTATUS_PLAYING;
    if (flags & DSBPLAY_LOOPING) {
        status_ |= DSBSTATUS_LOOPING;
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Lock(DWORD write_cursor, DWORD write_bytes, LPVOID* ptr1, DWORD* bytes1, LPVOID* ptr2, DWORD* bytes2, DWORD)
{
    if (!ptr1 || !bytes1 || !ptr2 || !bytes2 || data_.empty()) {
        return DSERR_GENERIC;
    }

    const DWORD start = write_cursor % static_cast<DWORD>(data_.size());
    const DWORD available_to_end = static_cast<DWORD>(data_.size()) - start;
    const DWORD first = std::min(write_bytes, available_to_end);
    const DWORD second = write_bytes - first;

    *ptr1 = data_.data() + start;
    *bytes1 = first;
    *ptr2 = second ? data_.data() : nullptr;
    *bytes2 = second;
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Unlock(LPVOID, DWORD, LPVOID, DWORD)
{
    return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD position)
{
    position_ = data_.empty() ? 0 : (position % static_cast<DWORD>(data_.size()));
    last_tick_ms_ = SDL_GetTicks();
    return DS_OK;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition(DWORD* play_cursor, DWORD* write_cursor)
{
    if ((status_ & DSBSTATUS_PLAYING) && !data_.empty()) {
        const uint64_t now = SDL_GetTicks();
        const uint64_t elapsed = now - last_tick_ms_;
        const uint64_t advance = (elapsed * bytes_per_second(format_)) / 1000u;
        if (advance > 0) {
            position_ = static_cast<DWORD>((position_ + advance) % data_.size());
            last_tick_ms_ = now;
        }
    }

    if (play_cursor) {
        *play_cursor = position_;
    }
    if (write_cursor) {
        *write_cursor = data_.empty() ? 0 : static_cast<DWORD>((position_ + (data_.size() / 2)) % data_.size());
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::SetVolume(LONG volume)
{
    volume_ = volume;
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Restore()
{
    return DS_OK;
}

HRESULT IDirectSoundBuffer::SetFormat(const WAVEFORMATEX* format)
{
    if (!format) {
        return DSERR_GENERIC;
    }
    format_ = *format;
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Release()
{
    if (--ref_count_ == 0) {
        delete this;
    }
    return DS_OK;
}

const WAVEFORMATEX& IDirectSoundBuffer::Format() const { return format_; }
uint8_t* IDirectSoundBuffer::Data() { return data_.data(); }
DWORD IDirectSoundBuffer::Size() const { return static_cast<DWORD>(data_.size()); }

IDirectSound::IDirectSound() : ref_count_(1), window_(nullptr)
{
}

HRESULT IDirectSound::SetCooperativeLevel(HWND hwnd, DWORD)
{
    window_ = hwnd;
    return DS_OK;
}

HRESULT IDirectSound::CreateSoundBuffer(const DSBUFFERDESC* desc, IDirectSoundBuffer** buffer, LPVOID)
{
    if (!desc || !buffer) {
        return DSERR_GENERIC;
    }
    const bool primary = (desc->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
    const WAVEFORMATEX* format = primary ? nullptr : desc->lpwfxFormat;
    *buffer = new IDirectSoundBuffer(primary ? 4096 : desc->dwBufferBytes, format, primary);
    return DS_OK;
}

HRESULT IDirectSound::Release()
{
    if (--ref_count_ == 0) {
        delete this;
    }
    return DS_OK;
}

extern "C" HRESULT DirectSoundCreate(LPVOID, LPDIRECTSOUND* direct_sound, LPVOID)
{
    if (!direct_sound) {
        return DSERR_GENERIC;
    }
    *direct_sound = new IDirectSound();
    return DS_OK;
}
