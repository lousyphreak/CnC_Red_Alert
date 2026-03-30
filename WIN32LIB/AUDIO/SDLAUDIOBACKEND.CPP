#ifdef WIN32
#define RA_AUDIO_RESTORE_WIN32 1
#undef WIN32
#endif

#ifdef _WIN32
#define RA_AUDIO_RESTORE__WIN32 1
#undef _WIN32
#endif

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#ifdef RA_AUDIO_RESTORE_WIN32
#define WIN32 1
#undef RA_AUDIO_RESTORE_WIN32
#endif

#ifdef RA_AUDIO_RESTORE__WIN32
#define _WIN32 1
#undef RA_AUDIO_RESTORE__WIN32
#endif

#include "SDLAUDIOBACKEND.H"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

int normalized_channels(const AudioBufferFormat &format)
{
	return (std::max<int>(1, format.nChannels ? format.nChannels : 2));
}

int normalized_bits_per_sample(const AudioBufferFormat &format)
{
	return (std::max<int>(8, format.wBitsPerSample ? format.wBitsPerSample : 8));
}

int bytes_per_frame(const AudioBufferFormat &format)
{
	if (format.nBlockAlign != 0) {
		return ((int)format.nBlockAlign);
	}
	return (normalized_channels(format) * std::max<int>(1, normalized_bits_per_sample(format) / 8));
}

int normalized_frequency(const AudioBufferFormat &format)
{
	return ((int)(format.nSamplesPerSec ? format.nSamplesPerSec : 22050));
}

float backend_volume_to_gain(LONG volume)
{
	if (volume <= -10000) {
		return (0.0f);
	}
	if (volume >= 0) {
		return (1.0f);
	}
	return (std::pow(10.0f, (float)volume / 2000.0f));
}

float read_pcm_sample(const uint8_t *frame, int channel, const AudioBufferFormat &format)
{
	const int channel_count = normalized_channels(format);
	const int sample_bits = normalized_bits_per_sample(format);
	const int clamped_channel = std::clamp(channel, 0, channel_count - 1);

	if (sample_bits == 16) {
		const uint8_t *sample = frame + (clamped_channel * 2);
		const int16_t value = (int16_t)((uint16_t)sample[0] | ((uint16_t)sample[1] << 8));
		return ((float)value / 32768.0f);
	}

	return (((float)frame[clamped_channel] - 128.0f) / 128.0f);
}

AudioBufferFormat default_format(void)
{
	AudioBufferFormat format;

	std::memset(&format, 0, sizeof(format));
	format.wFormatTag = RA_AUDIO_FORMAT_PCM;
	format.nChannels = 2;
	format.nSamplesPerSec = 22050;
	format.wBitsPerSample = 8;
	format.nBlockAlign = 2;
	format.nAvgBytesPerSec = 44100;
	return (format);
}

}	// namespace


AudioBackendBuffer::AudioBackendBuffer(AudioBackendDevice *owner, DWORD buffer_bytes, const AudioBufferFormat *format, bool primary)
	: Owner(owner), Primary(primary), RefCount(1), Status(0), Position(0), Locked(false), PlaybackCursorFrames(0.0), Volume(0),
	  Data(buffer_bytes ? buffer_bytes : 1, 0)
{
	FormatData = format ? *format : default_format();
}


AudioBackendBuffer::~AudioBackendBuffer()
{
	if (Owner != NULL) {
		Owner->RemoveBuffer(this);
	}
}


HRESULT AudioBackendBuffer::GetStatus(DWORD *status)
{
	std::scoped_lock lock(Mutex);
	if (status != NULL) {
		*status = Status;
	}
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Stop(void)
{
	{
		std::scoped_lock lock(Mutex);
		Status = 0;
	}

	if (Primary && Owner != NULL) {
		std::scoped_lock owner_lock(Owner->Mutex);
		Owner->UpdatePrimaryPlaybackLocked(false);
	}

	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Play(BOOL looping)
{
	{
		std::scoped_lock lock(Mutex);
		Status = RAAUDIOSTATUS_PLAYING;
		if (looping) {
			Status |= RAAUDIOSTATUS_LOOPING;
		}
		if (Primary) {
			Position = 0;
		}
	}

	if (Primary && Owner != NULL) {
		std::scoped_lock owner_lock(Owner->Mutex);
		Owner->UpdatePrimaryPlaybackLocked(true);
	}

	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Lock(DWORD write_cursor, DWORD write_bytes, LPVOID *ptr1, DWORD *bytes1, LPVOID *ptr2, DWORD *bytes2)
{
	std::scoped_lock lock(Mutex);
	if (ptr1 == NULL || bytes1 == NULL || ptr2 == NULL || bytes2 == NULL || Data.empty() || Locked) {
		return (RAAUDIOERR_GENERIC);
	}

	Locked = true;

	const DWORD start = write_cursor % (DWORD)Data.size();
	const DWORD available_to_end = (DWORD)Data.size() - start;
	const DWORD first = std::min(write_bytes, available_to_end);
	const DWORD second = write_bytes - first;

	*ptr1 = Data.data() + start;
	*bytes1 = first;
	*ptr2 = second ? Data.data() : NULL;
	*bytes2 = second;

	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Unlock(LPVOID, DWORD, LPVOID, DWORD)
{
	std::scoped_lock lock(Mutex);
	Locked = false;
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::SetCurrentPosition(DWORD position)
{
	std::scoped_lock lock(Mutex);
	if (Data.empty()) {
		Position = 0;
		PlaybackCursorFrames = 0.0;
		return (RAAUDIO_OK);
	}

	Position = position % (DWORD)Data.size();
	PlaybackCursorFrames = (double)Position / (double)std::max(1, bytes_per_frame(FormatData));
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::GetCurrentPosition(DWORD *play_cursor, DWORD *write_cursor)
{
	std::scoped_lock lock(Mutex);
	if (play_cursor != NULL) {
		*play_cursor = Position;
	}
	if (write_cursor != NULL) {
		*write_cursor = Data.empty() ? 0 : (DWORD)((Position + (Data.size() / 2)) % Data.size());
	}
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::SetVolume(LONG volume)
{
	std::scoped_lock lock(Mutex);
	Volume = volume;
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Restore(void)
{
	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::SetFormat(const AudioBufferFormat *format)
{
	if (format == NULL) {
		return (RAAUDIOERR_GENERIC);
	}

	{
		std::scoped_lock lock(Mutex);
		FormatData = *format;
		PlaybackCursorFrames = (double)Position / (double)std::max(1, bytes_per_frame(FormatData));
	}

	if (Primary && Owner != NULL) {
		std::scoped_lock owner_lock(Owner->Mutex);
		Owner->RefreshPrimaryFormatLocked();
	}

	return (RAAUDIO_OK);
}


HRESULT AudioBackendBuffer::Release(void)
{
	if (--RefCount == 0) {
		delete this;
	}
	return (RAAUDIO_OK);
}


void AudioBackendBuffer::MixInto(float *mix_buffer, int output_frames, int output_channels, int output_rate)
{
	std::scoped_lock lock(Mutex);

	if (Primary || Locked || !(Status & RAAUDIOSTATUS_PLAYING) || Data.empty() || output_frames <= 0) {
		return;
	}

	const int source_frame_bytes = bytes_per_frame(FormatData);
	const size_t source_frames = Data.size() / (size_t)std::max(1, source_frame_bytes);
	if (source_frames == 0) {
		return;
	}

	const int source_channels = normalized_channels(FormatData);
	const double step = (double)normalized_frequency(FormatData) / (double)std::max(1, output_rate);
	const float gain = backend_volume_to_gain(Volume);
	double cursor = PlaybackCursorFrames;

	for (int frame = 0; frame < output_frames; ++frame) {
		size_t source_index = (size_t)cursor;
		if (source_index >= source_frames) {
			if (Status & RAAUDIOSTATUS_LOOPING) {
				cursor = std::fmod(cursor, (double)source_frames);
				source_index = (size_t)cursor;
			} else {
				Status = 0;
				Position = 0;
				PlaybackCursorFrames = 0.0;
				return;
			}
		}

		const uint8_t *source_frame = Data.data() + (source_index * (size_t)source_frame_bytes);
		const float left = read_pcm_sample(source_frame, 0, FormatData);
		const float right = (source_channels > 1) ? read_pcm_sample(source_frame, 1, FormatData) : left;

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

	if (Status & RAAUDIOSTATUS_LOOPING) {
		cursor = std::fmod(cursor, (double)source_frames);
	}

	PlaybackCursorFrames = cursor;
	Position = Data.empty() ? 0 : (DWORD)(((uint64_t)(cursor * (double)source_frame_bytes)) % Data.size());
}


AudioBackendDevice::AudioBackendDevice(void)
	: RefCount(1), Window(NULL), Stream(NULL), PrimaryPlaying(false), PrimaryBuffer(NULL)
{
	std::memset(&MixSpec, 0, sizeof(MixSpec));
	MixSpec.channels = 2;
	MixSpec.format = SDL_AUDIO_F32;
	MixSpec.freq = 22050;
}


AudioBackendDevice::~AudioBackendDevice()
{
	std::scoped_lock lock(Mutex);
	if (Stream != NULL) {
		SDL_DestroyAudioStream(Stream);
		Stream = NULL;
	}
}


HRESULT AudioBackendDevice::SetWindow(HWND hwnd)
{
	Window = hwnd;
	return (RAAUDIO_OK);
}


HRESULT AudioBackendDevice::CreateBuffer(DWORD buffer_bytes, const AudioBufferFormat *format, BOOL primary, AudioBackendBuffer **buffer)
{
	if (buffer == NULL) {
		return (RAAUDIOERR_GENERIC);
	}

	*buffer = new AudioBackendBuffer(this, primary ? 4096 : buffer_bytes, format, primary != FALSE);
	if (*buffer == NULL) {
		return (RAAUDIOERR_GENERIC);
	}

	std::scoped_lock lock(Mutex);
	if (primary) {
		PrimaryBuffer = *buffer;
		RefreshPrimaryFormatLocked();
	} else {
		SecondaryBuffers.push_back(*buffer);
	}

	return (RAAUDIO_OK);
}


HRESULT AudioBackendDevice::Release(void)
{
	if (--RefCount == 0) {
		delete this;
	}
	return (RAAUDIO_OK);
}


void SDLCALL AudioBackendDevice::AudioCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int)
{
	if (userdata == NULL || stream == NULL || additional_amount <= 0) {
		return;
	}

	((AudioBackendDevice *)userdata)->FeedAudio(stream, additional_amount);
}


void AudioBackendDevice::FeedAudio(SDL_AudioStream *stream, int additional_amount)
{
	std::unique_lock<std::mutex> lock(Mutex, std::try_to_lock);
	if (!lock.owns_lock()) {
		std::vector<float> silence((size_t)((additional_amount + (int)sizeof(float) - 1) / (int)sizeof(float)), 0.0f);
		SDL_PutAudioStreamData(stream, silence.data(), (int)(silence.size() * sizeof(float)));
		return;
	}

	const int output_channels = std::max<int>(1, MixSpec.channels);
	const int frame_bytes = output_channels * (int)sizeof(float);
	int output_frames = additional_amount / frame_bytes;
	if (additional_amount % frame_bytes != 0) {
		++output_frames;
	}
	if (output_frames <= 0) {
		return;
	}

	std::vector<float> mix((size_t)output_frames * (size_t)output_channels, 0.0f);
	if (PrimaryPlaying) {
		for (AudioBackendBuffer *buffer : SecondaryBuffers) {
			if (buffer != NULL) {
				buffer->MixInto(mix.data(), output_frames, output_channels, MixSpec.freq);
			}
		}
	}

	for (float &sample : mix) {
		sample = std::clamp(sample, -1.0f, 1.0f);
	}

	SDL_PutAudioStreamData(stream, mix.data(), (int)(mix.size() * sizeof(float)));
}


void AudioBackendDevice::RemoveBuffer(AudioBackendBuffer *buffer)
{
	std::scoped_lock lock(Mutex);
	if (PrimaryBuffer == buffer) {
		PrimaryBuffer = NULL;
		PrimaryPlaying = false;
		if (Stream != NULL) {
			SDL_ClearAudioStream(Stream);
			SDL_PauseAudioStreamDevice(Stream);
			SDL_DestroyAudioStream(Stream);
			Stream = NULL;
		}
		return;
	}

	SecondaryBuffers.erase(std::remove(SecondaryBuffers.begin(), SecondaryBuffers.end(), buffer), SecondaryBuffers.end());
}


bool AudioBackendDevice::EnsureAudioStreamLocked(void)
{
	if (Stream != NULL) {
		return (true);
	}

	if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			return (false);
		}
	}

	if (PrimaryBuffer != NULL) {
		std::scoped_lock primary_lock(PrimaryBuffer->Mutex);
		const AudioBufferFormat &format = PrimaryBuffer->FormatData;
		MixSpec.channels = (Uint8)normalized_channels(format);
		MixSpec.freq = normalized_frequency(format);
	} else {
		MixSpec.channels = 2;
		MixSpec.freq = 22050;
	}
	MixSpec.format = SDL_AUDIO_F32;

	Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &MixSpec, AudioCallback, this);
	if (Stream == NULL) {
		return (false);
	}

	if (PrimaryPlaying) {
		SDL_ResumeAudioStreamDevice(Stream);
	}

	return (true);
}


void AudioBackendDevice::UpdatePrimaryPlaybackLocked(bool playing)
{
	PrimaryPlaying = playing;
	if (!playing) {
		if (Stream != NULL) {
			SDL_ClearAudioStream(Stream);
			SDL_PauseAudioStreamDevice(Stream);
		}
		return;
	}

	if (EnsureAudioStreamLocked()) {
		SDL_ResumeAudioStreamDevice(Stream);
	}
}


void AudioBackendDevice::RefreshPrimaryFormatLocked(void)
{
	if (PrimaryBuffer == NULL) {
		return;
	}

	if (Stream != NULL) {
		SDL_DestroyAudioStream(Stream);
		Stream = NULL;
	}

	if (PrimaryPlaying) {
		EnsureAudioStreamLocked();
	}
}


HRESULT CreateAudioBackendDevice(AudioBackendDevice **device)
{
	if (device == NULL) {
		return (RAAUDIOERR_GENERIC);
	}

	*device = new AudioBackendDevice();
	return (*device == NULL) ? RAAUDIOERR_GENERIC : RAAUDIO_OK;
}
