/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012 Luke Dashjr
 * Copyright 2012-2014 pooler
 * Copyright 2018-2019 CryptoGraphics <CrGr@protonmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef Utils_INCLUDE_ONCE
#define Utils_INCLUDE_ONCE

#include <jansson.h> // JSON
#include <lyclCore/Log.hpp>

#include <sys/stat.h>

namespace lycl
{
    namespace utils
    {
        //----------------------------------------------------------------------------
        inline bool fileExists(const char* file_name)
        {
            struct stat buffer;   
            return (stat (file_name, &buffer) == 0); 
        }
        //----------------------------------------------------------------------------
    }
}

inline bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
    char hex_byte[3];
    char *ep;

    hex_byte[2] = '\0';

    while (*hexstr && len)
    {
        if (!hexstr[1])
        {
            Log::print(Log::LT_Error, "hex2bin str truncated");
            return false;
        }
        hex_byte[0] = hexstr[0];
        hex_byte[1] = hexstr[1];
        *p = (unsigned char) strtol(hex_byte, &ep, 16);
        if (*ep)
        {
            Log::print(Log::LT_Error, "hex2bin failed on '%s'", hex_byte);
            return false;
        }
        p++;
        hexstr += 2;
        len--;
    }

    return(!len) ? true : false;
    //  return (len == 0 && *hexstr == 0) ? true : false;
}
//----------------------------------------------------------------------------
inline void bin2hex(char *s, const unsigned char *p, size_t len)
{
    for (size_t i = 0; i < len; i++)
        sprintf(s + (i * 2), "%02x", (unsigned int) p[i]);
}
//-----------------------------------------------------------------------------
inline char *abin2hex(const unsigned char *p, size_t len)
{
    char *s = (char*) malloc((len * 2) + 1);
    if (!s)
        return NULL;
    bin2hex(s, p, len);
    return s;
}
//-----------------------------------------------------------------------------
inline bool jobj_binary(const json_t *obj, const char *key, void *buf, size_t buflen)
{
    const char *hexstr;
    json_t *tmp;

    tmp = json_object_get(obj, key);
    if (!tmp)
    {
        Log::print(Log::LT_Error, "JSON key '%s' not found", key);
        return false;
    }
    hexstr = json_string_value(tmp);
    if (!hexstr)
    {
        Log::print(Log::LT_Error, "JSON key '%s' is not a string", key);
        return false;
    }
    if (!hex2bin((unsigned char*) buf, hexstr, buflen))
        return false;

    return true;
}
//-----------------------------------------------------------------------------
inline void scale_hash_for_display ( double* hashrate, char* units )
{
    if ( *hashrate < 1e4 )
        // 0 H/s to 9999 H/s
        *units = 0;
    else if ( *hashrate < 1e7 )
    {
        // 10 kH/s to 9999 kH/s
        *units = 'k';
        *hashrate /= 1e3;
    }
    else if ( *hashrate < 1e10 )
    {
        // 10 Mh/s to 9999 Mh/s
        *units = 'M';
        *hashrate /= 1e6;
    }
    else if ( *hashrate < 1e13 )
    {
        // 10 iGh/s to 9999 Gh/s
        *units = 'G';
        *hashrate /= 1e9;
    }
    else
    {
        // 10 Th/s and higher
        *units = 'T';
        *hashrate /= 1e12;
    }
}
//-----------------------------------------------------------------------------
inline int share_result( int result, struct work *work, const char *reason )
{
    char hc[16];
    char hr[16];
    const char *sres;
    double hashcount = 0.;
    double hashrate = 0.;
    char hc_units[4] = {0};
    char hr_units[4] = {0};
    uint32_t total_submits;
    float rate;
    char rate_s[8] = {0};

    pthread_mutex_lock(&stats_lock);
    for (int i = 0; i < global::numWorkerThreads; i++)
    {
        hashcount += thr_hashcount[i];
        hashrate += thr_hashrates[i];
    }
    result ? accepted_count++ : rejected_count++;
    pthread_mutex_unlock(&stats_lock);
    global_hashcount = hashcount;
    global_hashrate = hashrate;
    total_submits = accepted_count + rejected_count;

    rate = ( result ? ( 100. * accepted_count / total_submits ) : ( 100. * rejected_count / total_submits ) );

    if (global::use_colors)
        sres = (result ? CL_GRN "Accepted" CL_WHT : CL_RED "Rejected" CL_WHT );
    else
        sres = (result ? "Accepted" : "Rejected" );

    // Contrary to rounding convention 100% means zero rejects, exactly 100%. 
    // Rates > 99% and < 100% (rejects>0) display 99.9%.
    if ( result )
    {
        rate = 100. * accepted_count / total_submits;
        if ( rate == 100.0 )
            sprintf( rate_s, "%.0f", rate );
        else
            sprintf( rate_s, "%.1f", ( rate < 99.9 ) ? rate : 99.9 );
    }
    else
    {
        rate = 100. * rejected_count / total_submits;
        if ( rate < 0.1 )
            sprintf( rate_s, "%.1f", 0.10 );
        else
            sprintf( rate_s, "%.1f", rate );
    }

    scale_hash_for_display ( &hashcount, hc_units );
    scale_hash_for_display ( &hashrate, hr_units );
    if ( hc_units[0] )
    {
        sprintf(hc, "%.2f", hashcount );
        if ( hashrate < 10 )
            // very low hashrate, add digits
            sprintf(hr, "%.4f", hashrate );
        else
            sprintf(hr, "%.2f", hashrate );
    }
    else
    {
        // no fractions of a hash
        sprintf(hc, "%.0f", hashcount );
        sprintf(hr, "%.2f", hashrate );
    }

    Log::print( Log::LT_Notice, "%s %lu/%lu (%s%%), %s %sH, %s %sH/s",
                sres, ( result ? accepted_count : rejected_count ),
                total_submits, rate_s, hc, hc_units, hr, hr_units );

    if (reason)
    {
        Log::print(Log::LT_Warning, "reject reason: %s", reason);
        if (strncmp(reason, "low difficulty share", 20) == 0)
        {
            opt_diff_factor = (opt_diff_factor * 2.0) / 3.0;
            Log::print(Log::LT_Warning, "factor reduced to : %0.2f", opt_diff_factor);
            return 0;
        }
    }
    return 1;
}
//-----------------------------------------------------------------------------
inline void diff_to_target(uint32_t *target, double diff)
{
    uint64_t m;
    int k;
    
    for (k = 6; k > 0 && diff > 1.0; k--)
        diff /= 4294967296.0;
    m = (uint64_t)(4294901760.0 / diff);
    if (m == 0 && k == 6)
        memset(target, 0xff, 32);
    else {
        memset(target, 0, 32);
        target[k] = (uint32_t)m;
        target[k + 1] = (uint32_t)(m >> 32);
    }
}
//-----------------------------------------------------------------------------
inline void work_set_target(work* work_info, double diff)
{
    diff_to_target(work_info->target, diff);
    work_info->targetdiff = diff;
}
//-----------------------------------------------------------------------------
//! Modify the representation of integer numbers which would cause an overflow
//! so that they are treated as floating-point numbers.
//! This is a hack to overcome the limitations of some versions of Jansson.
inline char *hack_json_numbers(const char *in)
{
    char *out;
    int i, off, intoff;
    bool in_str, in_int;

    out = (char*) calloc(2 * strlen(in) + 1, 1);
    if (!out)
        return NULL;
    off = intoff = 0;
    in_str = in_int = false;
    for (i = 0; in[i]; i++)
    {
        char c = in[i];
        if (c == '"')
        {
            in_str = !in_str;
        }
        else if (c == '\\')
        {
            out[off++] = c;
            if (!in[++i])
                break;
        }
        else if (!in_str && !in_int && isdigit(c))
        {
            intoff = off;
            in_int = true;
        }
        else if (in_int && !isdigit(c))
        {
            if (c != '.' && c != 'e' && c != 'E' && c != '+' && c != '-')
            {
                in_int = false;
                if (off - intoff > 4)
                {
                    char *end;
#if JSON_INTEGER_IS_LONG_LONG
                    errno = 0;
                    strtoll(out + intoff, &end, 10);
                    if (!*end && errno == ERANGE)
                    {
#else
                    long l;
                    errno = 0;
                    l = strtol(out + intoff, &end, 10);
                    if (!*end && (errno == ERANGE || l > INT_MAX))
                    {
#endif
                        out[off++] = '.';
                        out[off++] = '0';
                    }
                }
            }
        }
        out[off++] = in[i];
    }
    return out;
}
//-----------------------------------------------------------------------------

#endif // !Utils_INCLUDE_ONCE
