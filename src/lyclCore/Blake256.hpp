/*
 * Copyright 2018-2019 CryptoGraphics <CrGr@protonmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef Blake256_INCLUDE_ONCE
#define Blake256_INCLUDE_ONCE

#include <stdint.h>

const uint8_t c_sigma[16][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 }
};

const uint32_t c_u256[16] =
{
    0x243F6A88, 0x85A308D3,
    0x13198A2E, 0x03707344,
    0xA4093822, 0x299F31D0,
    0x082EFA98, 0xEC4E6C89,
    0x452821E6, 0x38D01377,
    0xBE5466CF, 0x34E90C6C,
    0xC0AC29B7, 0xC97C50DD,
    0x3F84D5B5, 0xB5470917
};

uint32_t rotr32(uint32_t w, uint32_t c)
{
    return (( w >> c ) | ( w << ( 32 - c ) ) );
}


#define GS(a,b,c,d,x) { \
    const uint8_t idx1 = c_sigma[r][x]; \
    const uint8_t idx2 = c_sigma[r][x + 1]; \
    v[a] += (m[idx1] ^ c_u256[idx2]) + v[b]; \
    v[d] = rotr32(v[d] ^ v[a], 16); \
    v[c] += v[d]; \
    v[b] = rotr32(v[b] ^ v[c], 12); \
    \
    v[a] += (m[idx2] ^ c_u256[idx1]) + v[b]; \
    v[d] = rotr32(v[d] ^ v[a], 8); \
    v[c] += v[d]; \
    v[b] = rotr32(v[b] ^ v[c], 7); \
}

inline void blake256_compress(uint32_t* h, const uint32_t* block)
{
    uint32_t m[16];
    uint32_t v[16] =
    {
        0x6A09E667, 0xBB67AE85,
        0x3C6EF372, 0xA54FF53A,
        0x510E527F, 0x9B05688C,
        0x1F83D9AB, 0x5BE0CD19,
        0x243F6A88, 0x85A308D3,
        0x13198A2E, 0x03707344,
        0xA4093A22, 0x299F33D0,
        0x082EFA98, 0xEC4E6C89
    };

    for (int i = 0; i < 16; ++i)
    {
        m[i] = block[i];
    }

    for (int r = 0; r < 14; ++r)
    {
        // column step
        GS(0, 4, 0x8, 0xC, 0x0);
        GS(1, 5, 0x9, 0xD, 0x2);
        GS(2, 6, 0xA, 0xE, 0x4);
        GS(3, 7, 0xB, 0xF, 0x6);
        // diagonal step
        GS(0, 5, 0xA, 0xF, 0x8);
        GS(1, 6, 0xB, 0xC, 0xA);
        GS(2, 7, 0x8, 0xD, 0xC);
        GS(3, 4, 0x9, 0xE, 0xE);
    }

    h[0] ^= v[0] ^ v[8];
    h[1] ^= v[1] ^ v[9];
    h[2] ^= v[2] ^ v[10];
    h[3] ^= v[3] ^ v[11];
    h[4] ^= v[4] ^ v[12];
    h[5] ^= v[5] ^ v[13];
    h[6] ^= v[6] ^ v[14];
    h[7] ^= v[7] ^ v[15];
}


#endif // !Blake256_INCLUDE_ONCE
