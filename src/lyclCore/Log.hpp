/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012 Luke Dashjr
 * Copyright 2012-2014 pooler
 * Copyright 2018 CryptoGraphics ( CrGraphics@protonmail.com )
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef Log_INCLUDE_ONCE
#define Log_INCLUDE_ONCE

#include <stdarg.h> // va_list, va_start, va_arg, va_end
#include <cstring> // strlen
#include <lyclCore/Global.hpp>
#include <time.h> // time

namespace Log
{
    extern pthread_mutex_t applog_lock;

    //-----------------------------------------------------------------------------
    // Colors in console
    #define CL_N    "\x1B[0m"
    #define CL_RED  "\x1B[31m"
    #define CL_GRN  "\x1B[32m"
    #define CL_YLW  "\x1B[33m"
    #define CL_BLU  "\x1B[34m"
    #define CL_MAG  "\x1B[35m"
    #define CL_CYN  "\x1B[36m"

    #define CL_BLK  "\x1B[22;30m" // black
    #define CL_RD2  "\x1B[22;31m" // red
    #define CL_GR2  "\x1B[22;32m" // green
    #define CL_BRW  "\x1B[22;33m" // brown
    #define CL_BL2  "\x1B[22;34m" // blue
    #define CL_MA2  "\x1B[22;35m" // magenta
    #define CL_CY2  "\x1B[22;36m" // cyan
    #define CL_SIL  "\x1B[22;37m" // gray

#ifdef _WIN32
    #define CL_GRY  "\x1B[01;30m" // dark gray
#else
    #define CL_GRY  "\x1B[90m"    // dark gray selectable in putty
#endif
    #define CL_LRD  "\x1B[01;31m" // light red
    #define CL_LGR  "\x1B[01;32m" // light green
    #define CL_YL2  "\x1B[01;33m" // yellow
    #define CL_LBL  "\x1B[01;34m" // light blue
    #define CL_LMA  "\x1B[01;35m" // light magenta
    #define CL_LCY  "\x1B[01;36m" // light cyan

    #define CL_WHT  "\x1B[01;37m" // white

    typedef enum
    {
        LT_Error,
        LT_Warning,
        LT_Notice,
        LT_Info,
        LT_Debug,
        // custom notices
        LT_Blue = 0x10,
    } ELogType;

    inline void print(ELogType prio, const char *fmt, ...)
    {
        va_list ap;

        va_start(ap, fmt);

        const char* color = "";
        char *f;
        int len;
        struct tm timeInfo;
        time_t rawTime = time(NULL);

#ifdef _WIN32
    localtime_s(&timeInfo, &rawTime);
#else
    localtime_r(&rawTime, &timeInfo);
#endif

        switch (prio)
        {
            case LT_Error:   color = CL_RED; break;
            case LT_Warning: color = CL_YLW; break;
            case LT_Notice:  color = CL_WHT; break;
            case LT_Info:    color = ""; break;
            case LT_Debug:   color = CL_GRY; break;

            case LT_Blue:
                prio = LT_Notice;
                color = CL_CYN;
                break;
        }
        if (!global::use_colors)
            color = "";

        len = 64 + (int) strlen(fmt) + 2;
        //f = (char*) malloc(len);
        f = (char*) alloca(len);
        sprintf(f, "[%d-%02d-%02d %02d:%02d:%02d]%s %s%s\n",
                timeInfo.tm_year + 1900,
                timeInfo.tm_mon + 1,
                timeInfo.tm_mday,
                timeInfo.tm_hour,
                timeInfo.tm_min,
                timeInfo.tm_sec,
                color,
                fmt,
                global::use_colors ? CL_N : ""
                );
        pthread_mutex_lock(&applog_lock);
        vfprintf(stdout, f, ap); // atomic write to stdout
        fflush(stdout);
        //free(f);
        pthread_mutex_unlock(&applog_lock);

        va_end(ap);
    }
}

#endif// !Log_INCLUDE_ONCE
