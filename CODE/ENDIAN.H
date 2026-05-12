#ifndef ENDIAN_H
#define ENDIAN_H

#include <SDL3/SDL_endian.h>
#include <cstdint>
#include <cstring>

#define RA_BIG_ENDIAN_HOST (SDL_BYTEORDER == SDL_BIG_ENDIAN)

static inline int16_t RA_ReadUnalignedLE16(void const *data)
{
	uint16_t value;
	std::memcpy(&value, data, sizeof(value));
	return static_cast<int16_t>(SDL_Swap16LE(value));
}

static inline uint16_t RA_ReadUnalignedULE16(void const *data)
{
	uint16_t value;
	std::memcpy(&value, data, sizeof(value));
	return SDL_Swap16LE(value);
}

static inline int32_t RA_ReadUnalignedLE32(void const *data)
{
	uint32_t value;
	std::memcpy(&value, data, sizeof(value));
	return static_cast<int32_t>(SDL_Swap32LE(value));
}

static inline uint32_t RA_ReadUnalignedULE32(void const *data)
{
	uint32_t value;
	std::memcpy(&value, data, sizeof(value));
	return SDL_Swap32LE(value);
}

static inline uint32_t RA_ReadUnalignedUBE32(void const *data)
{
	uint32_t value;
	std::memcpy(&value, data, sizeof(value));
	return SDL_Swap32BE(value);
}

static inline void RA_WriteUnalignedLE16(void *data, uint16_t value)
{
	value = SDL_Swap16LE(value);
	std::memcpy(data, &value, sizeof(value));
}

static inline void RA_WriteUnalignedLE32(void *data, uint32_t value)
{
	value = SDL_Swap32LE(value);
	std::memcpy(data, &value, sizeof(value));
}

static inline void RA_WriteUnalignedBE32(void *data, uint32_t value)
{
	value = SDL_Swap32BE(value);
	std::memcpy(data, &value, sizeof(value));
}

static inline void RA_ConvertHost16BufferToLE(void *data, size_t sample_count)
{
#if RA_BIG_ENDIAN_HOST
	uint8_t *bytes = static_cast<uint8_t *>(data);
	for (size_t index = 0; index < sample_count; ++index) {
		uint16_t value;
		std::memcpy(&value, bytes + (index * sizeof(value)), sizeof(value));
		value = SDL_Swap16LE(value);
		std::memcpy(bytes + (index * sizeof(value)), &value, sizeof(value));
	}
#else
	(void)data;
	(void)sample_count;
#endif
}

#endif
