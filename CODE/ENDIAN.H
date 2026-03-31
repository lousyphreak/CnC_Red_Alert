#ifndef ENDIAN_H
#define ENDIAN_H

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#define RA_BIG_ENDIAN_HOST (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(BYTE_ORDER) && defined(BIG_ENDIAN)
#define RA_BIG_ENDIAN_HOST (BYTE_ORDER == BIG_ENDIAN)
#else
#define RA_BIG_ENDIAN_HOST 0
#endif

#endif
