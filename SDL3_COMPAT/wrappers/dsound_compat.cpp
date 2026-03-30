#include "dsound.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace {

int normalized_channels(const WAVEFORMATEX& format)
{
    return std::max<int>(1, format.nChannels ? format.nChannels : 2);
}

int normalized_bits_per_sample(const WAVEFORMATEX& format)
{
    return std::max<int>(8, format.wBitsPerSample ? format.wBitsPerSample : 8);
}

int bytes_per_frame(const WAVEFORMATEX& format)
{
    if (format.nBlockAlign != 0) {
        return static_cast<int>(format.nBlockAlign);
    }
    return normalized_channels(format) * std::max<int>(1, normalized_bits_per_sample(format) / 8);
}

int normalized_frequency(const WAVEFORMATEX& format)
{
    return static_cast<int>(format.nSamplesPerSec ? format.nSamplesPerSec : 22050);
}

float directsound_volume_to_gain(LONG volume)
{
    if (volume <= -10000) {
        return 0.0f;
    }
    if (volume >= 0) {
        return 1.0f;
    }
    return std::pow(10.0f, static_cast<float>(volume) / 2000.0f);
}

float read_pcm_sample(const uint8_t* frame, int channel, const WAVEFORMATEX& format)
{
    const int channel_count = normalized_channels(format);
    const int sample_bits = normalized_bits_per_sample(format);
    const int clamped_channel = std::clamp(channel, 0, channel_count - 1);

    if (sample_bits == 16) {
        const uint8_t* sample = frame + (clamped_channel * 2);
        const int16_t value = static_cast<int16_t>(static_cast<uint16_t>(sample[0]) | (static_cast<uint16_t>(sample[1]) << 8));
        return static_cast<float>(value) / 32768.0f;
    }

    const uint8_t value = frame[clamped_channel];
    return (static_cast<float>(value) - 128.0f) / 128.0f;
}

} // namespace

IDirectSoundBuffer::IDirectSoundBuffer(IDirectSound* owner, DWORD buffer_bytes, const WAVEFORMATEX* format, bool primary)
    : owner_(owner), primary_(primary), ref_count_(1), status_(0), position_(0), locked_(false), playback_cursor_frames_(0.0), volume_(0), data_(buffer_bytes ? buffer_bytes : 1, 0)
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

IDirectSoundBuffer::~IDirectSoundBuffer()
{
    if (owner_) {
        owner_->RemoveBuffer(this);
    }
}

HRESULT IDirectSoundBuffer::GetStatus(DWORD* status)
{
    std::scoped_lock lock(mutex_);
    if (status) {
        *status = status_;
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
    {
        std::scoped_lock lock(mutex_);
        status_ = 0;
    }
    if (primary_ && owner_) {
        std::scoped_lock owner_lock(owner_->mutex_);
        owner_->UpdatePrimaryPlaybackLocked(false);
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Play(DWORD, DWORD, DWORD flags)
{
    {
        std::scoped_lock lock(mutex_);
        status_ = DSBSTATUS_PLAYING;
        if (flags & DSBPLAY_LOOPING) {
            status_ |= DSBSTATUS_LOOPING;
        }
        if (primary_) {
            position_ = 0;
        }
    }
    if (primary_ && owner_) {
        std::scoped_lock owner_lock(owner_->mutex_);
        owner_->UpdatePrimaryPlaybackLocked(true);
    }
    return DS_OK;
}

HRESULT IDirectSoundBuffer::Lock(DWORD write_cursor, DWORD write_bytes, LPVOID* ptr1, DWORD* bytes1, LPVOID* ptr2, DWORD* bytes2, DWORD)
{
    std::scoped_lock lock(mutex_);
    if (!ptr1 || !bytes1 || !ptr2 || !bytes2 || data_.empty() || locked_) {
        return DSERR_GENERIC;
    }

    locked_ = true;

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
    std::scoped_lock lock(mutex_);
    locked_ = false;
    return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD position)
{
    std::scoped_lock lock(mutex_);
    if (data_.empty()) {
        position_ = 0;
        playback_cursor_frames_ = 0.0;
        return DS_OK;
    }

    position_ = position % static_cast<DWORD>(data_.size());
    const int frame_bytes = bytes_per_frame(format_);
    playback_cursor_frames_ = static_cast<double>(position_) / static_cast<double>(std::max(1, frame_bytes));
    return DS_OK;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition(DWORD* play_cursor, DWORD* write_cursor)
{
    std::scoped_lock lock(mutex_);
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
    std::scoped_lock lock(mutex_);
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

    {
        std::scoped_lock lock(mutex_);
        format_ = *format;
        const int frame_bytes = bytes_per_frame(format_);
        playback_cursor_frames_ = static_cast<double>(position_) / static_cast<double>(std::max(1, frame_bytes));
    }

    if (primary_ && owner_) {
        std::scoped_lock owner_lock(owner_->mutex_);
        owner_->RefreshPrimaryFormatLocked();
    }
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

void IDirectSoundBuffer::MixInto(float* mix_buffer, int output_frames, int output_channels, int output_rate)
{
    std::scoped_lock lock(mutex_);

    if (primary_ || locked_ || !(status_ & DSBSTATUS_PLAYING) || data_.empty() || output_frames <= 0) {
        return;
    }

    const int source_frame_bytes = bytes_per_frame(format_);
    const size_t source_frames = data_.size() / static_cast<size_t>(std::max(1, source_frame_bytes));
    if (source_frames == 0) {
        return;
    }

    const int source_channels = normalized_channels(format_);
    const double step = static_cast<double>(normalized_frequency(format_)) / static_cast<double>(std::max(1, output_rate));
    const float gain = directsound_volume_to_gain(volume_);
    double cursor = playback_cursor_frames_;

    for (int frame = 0; frame < output_frames; ++frame) {
        size_t source_index = static_cast<size_t>(cursor);
        if (source_index >= source_frames) {
            if (status_ & DSBSTATUS_LOOPING) {
                cursor = std::fmod(cursor, static_cast<double>(source_frames));
                source_index = static_cast<size_t>(cursor);
            } else {
                status_ = 0;
                position_ = 0;
                playback_cursor_frames_ = 0.0;
                return;
            }
        }

        const uint8_t* source_frame = data_.data() + (source_index * static_cast<size_t>(source_frame_bytes));
        const float left = read_pcm_sample(source_frame, 0, format_);
        const float right = (source_channels > 1) ? read_pcm_sample(source_frame, 1, format_) : left;

        if (output_channels == 1) {
            mix_buffer[frame] += gain * ((source_channels > 1) ? ((left + right) * 0.5f) : left);
        } else {
            mix_buffer[frame * output_channels] += gain * left;
            mix_buffer[frame * output_channels + 1] += gain * right;
            for (int channel = 2; channel < output_channels; ++channel) {
                mix_buffer[frame * output_channels + channel] += gain * ((left + right) * 0.5f);
            }
        }

        cursor += step;
    }

    if (status_ & DSBSTATUS_LOOPING) {
        cursor = std::fmod(cursor, static_cast<double>(source_frames));
    }

    playback_cursor_frames_ = cursor;
    position_ = data_.empty() ? 0 : static_cast<DWORD>(static_cast<uint64_t>(cursor * static_cast<double>(source_frame_bytes)) % data_.size());
}

IDirectSound::IDirectSound() : ref_count_(1), window_(nullptr), stream_(nullptr), primary_playing_(false), primary_buffer_(nullptr)
{
    std::memset(&mix_spec_, 0, sizeof(mix_spec_));
    mix_spec_.channels = 2;
    mix_spec_.format = SDL_AUDIO_F32;
    mix_spec_.freq = 22050;
}

IDirectSound::~IDirectSound()
{
    std::scoped_lock lock(mutex_);
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
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
    *buffer = new IDirectSoundBuffer(this, primary ? 4096 : desc->dwBufferBytes, format, primary);
    std::scoped_lock lock(mutex_);
    if (primary) {
        primary_buffer_ = *buffer;
        RefreshPrimaryFormatLocked();
    } else {
        secondary_buffers_.push_back(*buffer);
    }
    return DS_OK;
}

HRESULT IDirectSound::Release()
{
    if (--ref_count_ == 0) {
        delete this;
    }
    return DS_OK;
}

void SDLCALL IDirectSound::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int)
{
    if (!userdata || !stream || additional_amount <= 0) {
        return;
    }
    static_cast<IDirectSound*>(userdata)->FeedAudio(stream, additional_amount);
}

void IDirectSound::FeedAudio(SDL_AudioStream* stream, int additional_amount)
{
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        std::vector<float> silence(static_cast<size_t>((additional_amount + static_cast<int>(sizeof(float)) - 1) / static_cast<int>(sizeof(float))), 0.0f);
        SDL_PutAudioStreamData(stream, silence.data(), static_cast<int>(silence.size() * sizeof(float)));
        return;
    }

    const int output_channels = std::max<int>(1, mix_spec_.channels);
    const int frame_bytes = output_channels * static_cast<int>(sizeof(float));
    int output_frames = additional_amount / frame_bytes;
    if (additional_amount % frame_bytes != 0) {
        ++output_frames;
    }
    if (output_frames <= 0) {
        return;
    }

    std::vector<float> mix(static_cast<size_t>(output_frames) * static_cast<size_t>(output_channels), 0.0f);
    if (primary_playing_) {
        for (IDirectSoundBuffer* buffer : secondary_buffers_) {
            if (buffer) {
                buffer->MixInto(mix.data(), output_frames, output_channels, mix_spec_.freq);
            }
        }
    }

    for (float& sample : mix) {
        sample = std::clamp(sample, -1.0f, 1.0f);
    }
    SDL_PutAudioStreamData(stream, mix.data(), static_cast<int>(mix.size() * sizeof(float)));
}

void IDirectSound::RemoveBuffer(IDirectSoundBuffer* buffer)
{
    std::scoped_lock lock(mutex_);
    if (primary_buffer_ == buffer) {
        primary_buffer_ = nullptr;
        primary_playing_ = false;
        if (stream_) {
            SDL_ClearAudioStream(stream_);
            SDL_PauseAudioStreamDevice(stream_);
            SDL_DestroyAudioStream(stream_);
            stream_ = nullptr;
        }
        return;
    }

    secondary_buffers_.erase(std::remove(secondary_buffers_.begin(), secondary_buffers_.end(), buffer), secondary_buffers_.end());
}

bool IDirectSound::EnsureAudioStreamLocked()
{
    if (stream_) {
        return true;
    }

    if (primary_buffer_) {
        std::scoped_lock primary_lock(primary_buffer_->mutex_);
        const WAVEFORMATEX& format = primary_buffer_->format_;
        mix_spec_.channels = static_cast<Uint8>(normalized_channels(format));
        mix_spec_.freq = normalized_frequency(format);
    } else {
        mix_spec_.channels = 2;
        mix_spec_.freq = 22050;
    }
    mix_spec_.format = SDL_AUDIO_F32;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mix_spec_, AudioCallback, this);
    if (!stream_) {
        return false;
    }

    if (primary_playing_) {
        SDL_ResumeAudioStreamDevice(stream_);
    }
    return true;
}

void IDirectSound::UpdatePrimaryPlaybackLocked(bool playing)
{
    primary_playing_ = playing;
    if (!playing) {
        if (stream_) {
            SDL_ClearAudioStream(stream_);
            SDL_PauseAudioStreamDevice(stream_);
        }
        return;
    }

    if (EnsureAudioStreamLocked()) {
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

void IDirectSound::RefreshPrimaryFormatLocked()
{
    if (!primary_buffer_) {
        return;
    }

    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }

    if (primary_playing_) {
        EnsureAudioStreamLocked();
    }
}

extern "C" HRESULT DirectSoundCreate(LPVOID, LPDIRECTSOUND* direct_sound, LPVOID)
{
    if (!direct_sound) {
        return DSERR_GENERIC;
    }
    *direct_sound = new IDirectSound();
    return DS_OK;
}
