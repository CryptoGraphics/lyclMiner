#if 0
Borrowed from scrypt-1.2.0 which includes the following copyright notice:

The code and documentation in this directory ("libcperciva") is distributed
under the following terms:

Copyright 2005-2014 Colin Percival.  All rights reserved.
Copyright 2014 Sean Kelly.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
#endif

#ifndef _SYSENDIAN_H_
#define _SYSENDIAN_H_

#include <stdint.h>

// Avoid namespace collisions with BSD <sys/endian.h>.
#define be16dec libcperciva_be16dec
#define be16enc libcperciva_be16enc
#define be32dec libcperciva_be32dec
#define be32enc libcperciva_be32enc
#define be64dec libcperciva_be64dec
#define be64enc libcperciva_be64enc
#define le16dec libcperciva_le16dec
#define le16enc libcperciva_le16enc
#define le32dec libcperciva_le32dec
#define le32enc libcperciva_le32enc
#define le64dec libcperciva_le64dec
#define le64enc libcperciva_le64enc

inline uint16_t be16dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint16_t)(p[1]) + ((uint16_t)(p[0]) << 8));
}

inline void be16enc(void * pp, uint16_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[1] = x & 0xff;
    p[0] = (x >> 8) & 0xff;
}

inline uint32_t be32dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint32_t)(p[3]) + ((uint32_t)(p[2]) << 8) +
        ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24));
}

inline void be32enc(void * pp, uint32_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[3] = x & 0xff;
    p[2] = (x >> 8) & 0xff;
    p[1] = (x >> 16) & 0xff;
    p[0] = (x >> 24) & 0xff;
}

inline uint64_t be64dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint64_t)(p[7]) + ((uint64_t)(p[6]) << 8) +
        ((uint64_t)(p[5]) << 16) + ((uint64_t)(p[4]) << 24) +
        ((uint64_t)(p[3]) << 32) + ((uint64_t)(p[2]) << 40) +
        ((uint64_t)(p[1]) << 48) + ((uint64_t)(p[0]) << 56));
}

inline void be64enc(void * pp, uint64_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[7] = x & 0xff;
    p[6] = (x >> 8) & 0xff;
    p[5] = (x >> 16) & 0xff;
    p[4] = (x >> 24) & 0xff;
    p[3] = (x >> 32) & 0xff;
    p[2] = (x >> 40) & 0xff;
    p[1] = (x >> 48) & 0xff;
    p[0] = (x >> 56) & 0xff;
}

inline uint16_t le16dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint16_t)(p[0]) + ((uint16_t)(p[1]) << 8));
}

inline void le16enc(void * pp, uint16_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
}

inline uint32_t le32dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
        ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}

inline void le32enc(void * pp, uint32_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
    p[2] = (x >> 16) & 0xff;
    p[3] = (x >> 24) & 0xff;
}

inline uint64_t le64dec(const void * pp)
{
    const uint8_t * p = (uint8_t const *)pp;

    return ((uint64_t)(p[0]) + ((uint64_t)(p[1]) << 8) +
        ((uint64_t)(p[2]) << 16) + ((uint64_t)(p[3]) << 24) +
        ((uint64_t)(p[4]) << 32) + ((uint64_t)(p[5]) << 40) +
        ((uint64_t)(p[6]) << 48) + ((uint64_t)(p[7]) << 56));
}

inline void le64enc(void * pp, uint64_t x)
{
    uint8_t * p = (uint8_t *)pp;

    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
    p[2] = (x >> 16) & 0xff;
    p[3] = (x >> 24) & 0xff;
    p[4] = (x >> 32) & 0xff;
    p[5] = (x >> 40) & 0xff;
    p[6] = (x >> 48) & 0xff;
    p[7] = (x >> 56) & 0xff;
}

#endif /* !_SYSENDIAN_H_ */
