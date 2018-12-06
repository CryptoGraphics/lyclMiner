/*
 * Copyright 2018 CryptoGraphics ( CrGraphics@protonmail.com )
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef BMW_INCLUDE_ONCE
#define BMW_INCLUDE_ONCE

#include <lyclApplets/AppCommon.hpp>

namespace lycl
{
    #define shl(x, n) ((x) << (n))
    #define shr(x, n) ((x) >> (n))

    //#define SPH_ROTL32(x,n) rotate(x,(uint)n)
    #define SPH_ROTL32(x, n) (( (x)<<(n) ) | ((x) >> (32 - (n))))
    #define ss0(x) (shr((x), 1) ^ shl((x), 3) ^ SPH_ROTL32((x),  4) ^ SPH_ROTL32((x), 19))
    #define ss1(x) (shr((x), 1) ^ shl((x), 2) ^ SPH_ROTL32((x),  8) ^ SPH_ROTL32((x), 23))
    #define ss2(x) (shr((x), 2) ^ shl((x), 1) ^ SPH_ROTL32((x), 12) ^ SPH_ROTL32((x), 25))
    #define ss3(x) (shr((x), 2) ^ shl((x), 2) ^ SPH_ROTL32((x), 15) ^ SPH_ROTL32((x), 29))
    #define ss4(x) (shr((x), 1) ^ (x))
    #define ss5(x) (shr((x), 2) ^ (x))
    #define rs1(x) SPH_ROTL32((x),  3)
    #define rs2(x) SPH_ROTL32((x),  7)
    #define rs3(x) SPH_ROTL32((x), 13)
    #define rs4(x) SPH_ROTL32((x), 16)
    #define rs5(x) SPH_ROTL32((x), 19)
    #define rs6(x) SPH_ROTL32((x), 23)
    #define rs7(x) SPH_ROTL32((x), 27)
    //-----------------------------------------------------------------------------
    // Message expansion function 1
    uint32_t expand32_1(size_t i, uint32_t* M32, uint32_t* H, uint32_t* Q)
    {
        return (ss1(Q[i - 16]) + ss2(Q[i - 15]) + ss3(Q[i - 14]) + ss0(Q[i - 13])
                + ss1(Q[i - 12]) + ss2(Q[i - 11]) + ss3(Q[i - 10]) + ss0(Q[i - 9])
                + ss1(Q[i - 8]) + ss2(Q[i - 7]) + ss3(Q[i - 6]) + ss0(Q[i - 5])
                + ss1(Q[i - 4]) + ss2(Q[i - 3]) + ss3(Q[i - 2]) + ss0(Q[i - 1])
                + ((i*(0x05555555ul) + SPH_ROTL32(M32[(i - 16) & 15], ((i - 16) & 15) + 1) + SPH_ROTL32(M32[(i - 13) % 16], ((i - 13) % 16) + 1) - SPH_ROTL32(M32[(i - 6) % 16], ((i - 6) % 16) + 1)) ^ H[(i - 16 + 7) % 16]));
    }
    //-----------------------------------------------------------------------------
    // Message expansion function 2
    uint32_t expand32_2(size_t i, uint32_t* M32, uint32_t* H, uint32_t* Q)
    {
        return (Q[i - 16] + rs1(Q[i - 15]) + Q[i - 14] + rs2(Q[i - 13])
                + Q[i - 12] + rs3(Q[i - 11]) + Q[i - 10] + rs4(Q[i - 9])
                + Q[i - 8] + rs5(Q[i - 7]) + Q[i - 6] + rs6(Q[i - 5])
                + Q[i - 4] + rs7(Q[i - 3]) + ss4(Q[i - 2]) + ss5(Q[i - 1])
                + ((i*(0x05555555ul) + SPH_ROTL32(M32[(i - 16) % 16], ((i - 16) % 16) + 1) + SPH_ROTL32(M32[(i - 13) % 16], ((i - 13) % 16) + 1) - SPH_ROTL32(M32[(i - 6) % 16], ((i - 6) % 16) + 1)) ^ H[(i - 16 + 7) % 16]));
    }
    //-----------------------------------------------------------------------------
    void compression256(uint32_t* M32, uint32_t* H)
    {
        uint32_t XL32, XH32, Q[32];

        Q[ 0] = (M32[ 5] ^ H[ 5]) - (M32[7] ^ H[7]) + (M32[10] ^ H[10]) + (M32[13] ^ H[13]) + (M32[14] ^ H[14]);
        Q[ 1] = (M32[ 6] ^ H[ 6]) - (M32[8] ^ H[8]) + (M32[11] ^ H[11]) + (M32[14] ^ H[14]) - (M32[15] ^ H[15]);
        Q[ 2] = (M32[ 0] ^ H[ 0]) + (M32[7] ^ H[7]) + (M32[ 9] ^ H[ 9]) - (M32[12] ^ H[12]) + (M32[15] ^ H[15]);
        Q[ 3] = (M32[ 0] ^ H[ 0]) - (M32[1] ^ H[1]) + (M32[ 8] ^ H[ 8]) - (M32[10] ^ H[10]) + (M32[13] ^ H[13]);
        Q[ 4] = (M32[ 1] ^ H[ 1]) + (M32[2] ^ H[2]) + (M32[ 9] ^ H[ 9]) - (M32[11] ^ H[11]) - (M32[14] ^ H[14]);
        Q[ 5] = (M32[ 3] ^ H[ 3]) - (M32[2] ^ H[2]) + (M32[10] ^ H[10]) - (M32[12] ^ H[12]) + (M32[15] ^ H[15]);
        Q[ 6] = (M32[ 4] ^ H[ 4]) - (M32[0] ^ H[0]) - (M32[ 3] ^ H[ 3]) - (M32[11] ^ H[11]) + (M32[13] ^ H[13]);
        Q[ 7] = (M32[ 1] ^ H[ 1]) - (M32[4] ^ H[4]) - (M32[ 5] ^ H[ 5]) - (M32[12] ^ H[12]) - (M32[14] ^ H[14]);
        Q[ 8] = (M32[ 2] ^ H[ 2]) - (M32[5] ^ H[5]) - (M32[ 6] ^ H[ 6]) + (M32[13] ^ H[13]) - (M32[15] ^ H[15]);
        Q[ 9] = (M32[ 0] ^ H[ 0]) - (M32[3] ^ H[3]) + (M32[ 6] ^ H[ 6]) - (M32[ 7] ^ H[ 7]) + (M32[14] ^ H[14]);
        Q[10] = (M32[ 8] ^ H[ 8]) - (M32[1] ^ H[1]) - (M32[ 4] ^ H[ 4]) - (M32[ 7] ^ H[ 7]) + (M32[15] ^ H[15]);
        Q[11] = (M32[ 8] ^ H[ 8]) - (M32[0] ^ H[0]) - (M32[ 2] ^ H[ 2]) - (M32[ 5] ^ H[ 5]) + (M32[ 9] ^ H[ 9]);
        Q[12] = (M32[ 1] ^ H[ 1]) + (M32[3] ^ H[3]) - (M32[ 6] ^ H[ 6]) - (M32[ 9] ^ H[ 9]) + (M32[10] ^ H[10]);
        Q[13] = (M32[ 2] ^ H[ 2]) + (M32[4] ^ H[4]) + (M32[ 7] ^ H[ 7]) + (M32[10] ^ H[10]) + (M32[11] ^ H[11]);
        Q[14] = (M32[ 3] ^ H[ 3]) - (M32[5] ^ H[5]) + (M32[ 8] ^ H[ 8]) - (M32[11] ^ H[11]) - (M32[12] ^ H[12]);
        Q[15] = (M32[12] ^ H[12]) - (M32[4] ^ H[4]) - (M32[ 6] ^ H[ 6]) - (M32[ 9] ^ H[ 9]) + (M32[13] ^ H[13]);

        /*  Diffuse the differences in every word in a bijective manner with ssi, and then add the values of the previous double pipe.*/
        Q[ 0] = ss0(Q[ 0]) + H[ 1];
        Q[ 1] = ss1(Q[ 1]) + H[ 2];
        Q[ 2] = ss2(Q[ 2]) + H[ 3];
        Q[ 3] = ss3(Q[ 3]) + H[ 4];
        Q[ 4] = ss4(Q[ 4]) + H[ 5];
        Q[ 5] = ss0(Q[ 5]) + H[ 6];
        Q[ 6] = ss1(Q[ 6]) + H[ 7];
        Q[ 7] = ss2(Q[ 7]) + H[ 8];
        Q[ 8] = ss3(Q[ 8]) + H[ 9];
        Q[ 9] = ss4(Q[ 9]) + H[10];
        Q[10] = ss0(Q[10]) + H[11];
        Q[11] = ss1(Q[11]) + H[12];
        Q[12] = ss2(Q[12]) + H[13];
        Q[13] = ss3(Q[13]) + H[14];
        Q[14] = ss4(Q[14]) + H[15];
        Q[15] = ss0(Q[15]) + H[ 0];

        // This is the Message expansion or f_1 in the documentation.
        // It has 16 rounds.
        // Blue Midnight Wish has two tunable security parameters.
        // The parameters are named EXPAND_1_ROUNDS and EXPAND_2_ROUNDS.
        // The following relation for these parameters should is satisfied:
        // EXPAND_1_ROUNDS + EXPAND_2_ROUNDS = 16
        Q[16] = expand32_1( 16, M32, H, Q);
        Q[17] = expand32_1( 17, M32, H, Q);

        for (size_t i = 2; i<16; i++)
        {
            Q[i + 16] = expand32_2(i + 16, M32, H, Q);
        }

        // Blue Midnight Wish has two temporary cummulative variables that accumulate via XORing
        // 16 new variables that are prooduced in the Message Expansion part.
        XL32 = Q[16] ^ Q[17] ^ Q[18] ^ Q[19] ^ Q[20] ^ Q[21] ^ Q[22] ^ Q[23];
        XH32 = XL32^Q[24] ^ Q[25] ^ Q[26] ^ Q[27] ^ Q[28] ^ Q[29] ^ Q[30] ^ Q[31];

        // This part is the function f_2 - in the documentation

        // Compute the double chaining pipe for the next message block.
        H[ 0] = (shl(XH32,  5) ^ shr(Q[16], 5) ^ M32[0]) + (XL32    ^ Q[24] ^ Q[0]);
        H[ 1] = (shr(XH32,  7) ^ shl(Q[17], 8) ^ M32[1]) + (XL32    ^ Q[25] ^ Q[1]);
        H[ 2] = (shr(XH32,  5) ^ shl(Q[18], 5) ^ M32[2]) + (XL32    ^ Q[26] ^ Q[2]);
        H[ 3] = (shr(XH32,  1) ^ shl(Q[19], 5) ^ M32[3]) + (XL32    ^ Q[27] ^ Q[3]);
        H[ 4] = (shr(XH32,  3) ^ Q[20] ^ M32[4]) + (XL32    ^ Q[28] ^ Q[4]);
        H[ 5] = (shl(XH32,  6) ^ shr(Q[21], 6) ^ M32[5]) + (XL32    ^ Q[29] ^ Q[5]);
        H[ 6] = (shr(XH32,  4) ^ shl(Q[22], 6) ^ M32[6]) + (XL32    ^ Q[30] ^ Q[6]);
        H[ 7] = (shr(XH32, 11) ^ shl(Q[23], 2) ^ M32[7]) + (XL32    ^ Q[31] ^ Q[7]);

        H[ 8] = SPH_ROTL32(H[4],  9) + (XH32 ^ Q[24] ^ M32[ 8]) + (shl(XL32, 8) ^ Q[23] ^ Q[ 8]);
        H[ 9] = SPH_ROTL32(H[5], 10) + (XH32 ^ Q[25] ^ M32[ 9]) + (shr(XL32, 6) ^ Q[16] ^ Q[ 9]);
        H[10] = SPH_ROTL32(H[6], 11) + (XH32 ^ Q[26] ^ M32[10]) + (shl(XL32, 6) ^ Q[17] ^ Q[10]);
        H[11] = SPH_ROTL32(H[7], 12) + (XH32 ^ Q[27] ^ M32[11]) + (shl(XL32, 4) ^ Q[18] ^ Q[11]);
        H[12] = SPH_ROTL32(H[0], 13) + (XH32 ^ Q[28] ^ M32[12]) + (shr(XL32, 3) ^ Q[19] ^ Q[12]);
        H[13] = SPH_ROTL32(H[1], 14) + (XH32 ^ Q[29] ^ M32[13]) + (shr(XL32, 4) ^ Q[20] ^ Q[13]);
        H[14] = SPH_ROTL32(H[2], 15) + (XH32 ^ Q[30] ^ M32[14]) + (shr(XL32, 7) ^ Q[21] ^ Q[14]);
        H[15] = SPH_ROTL32(H[3], 16) + (XH32 ^ Q[31] ^ M32[15]) + (shr(XL32, 2) ^ Q[22] ^ Q[15]);
    }
    //-----------------------------------------------------------------------------
    void bmwHash(const uint32x8& hash_input, uint32x8& hash_output)
    {
        uint32_t dh[16] = {
            0x40414243U, 0x44454647U,
            0x48494A4BU, 0x4C4D4E4FU,
            0x50515253U, 0x54555657U,
            0x58595A5BU, 0x5C5D5E5FU,
            0x60616263U, 0x64656667U,
            0x68696A6BU, 0x6C6D6E6FU,
            0x70717273U, 0x74757677U,
            0x78797A7BU, 0x7C7D7E7FU
        };
        uint32_t final_s[16] = {
            0xaaaaaaa0U, 0xaaaaaaa1U, 0xaaaaaaa2U,
            0xaaaaaaa3U, 0xaaaaaaa4U, 0xaaaaaaa5U,
            0xaaaaaaa6U, 0xaaaaaaa7U, 0xaaaaaaa8U,
            0xaaaaaaa9U, 0xaaaaaaaaU, 0xaaaaaaabU,
            0xaaaaaaacU, 0xaaaaaaadU, 0xaaaaaaaeU,
            0xaaaaaaafU
        };

        uint32_t message[16] = { 0 };

        message[ 0] = hash_input.h[0];
        message[ 1] = hash_input.h[1];
        message[ 2] = hash_input.h[2];
        message[ 3] = hash_input.h[3];
        message[ 4] = hash_input.h[4];
        message[ 5] = hash_input.h[5];
        message[ 6] = hash_input.h[6];
        message[ 7] = hash_input.h[7];
        message[ 8] = 0x80;
        message[14] = 0x100;

        compression256(message, dh);
        compression256(dh, final_s);
        
        hash_output.h[0] = final_s[8];
        hash_output.h[1] = final_s[9];
        hash_output.h[2] = final_s[10];
        hash_output.h[3] = final_s[11];
        hash_output.h[4] = final_s[12];
        hash_output.h[5] = final_s[13];
        hash_output.h[6] = final_s[14];
        hash_output.h[7] = final_s[15];
    }
    //-----------------------------------------------------------------------------
}

#endif // !BMW_INCLUDE_ONCE
