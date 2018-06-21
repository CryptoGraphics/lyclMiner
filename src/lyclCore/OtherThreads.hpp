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

#ifndef OtherThreads_INCLUDE_ONCE
#define OtherThreads_INCLUDE_ONCE

#ifndef _WIN32
#include <signal.h>
#endif

#include <lyclCore/Stratum.hpp>
#include <lyclCore/WorkIO.hpp>

//-----------------------------------------------------------------------------
// This file contains other threads. TODO: this need to be sorted
//-----------------------------------------------------------------------------
inline void buildExtraHeader(work* g_work, stratum_ctx* sctx)
{
    unsigned char merkle_root[64] = { 0 };
    size_t t;
    int i;

    // generate Merkle Root
    sha256d(merkle_root, sctx->job.coinbase, (int) sctx->job.coinbase_size);
    for ( int i = 0; i < sctx->job.merkle_count; i++ )
    {
        memcpy( merkle_root + 32, sctx->job.merkle[i], 32 );
        sha256d( merkle_root, merkle_root, 64 );
    }

    // Increment extranonce2
    for ( t = 0; t < sctx->xnonce2_size && !( ++sctx->job.xnonce2[t] ); t++ );
    // Assemble block header
    memset( g_work->data, 0, sizeof(g_work->data) );
    g_work->data[0] = le32dec( sctx->job.version );
    for ( i = 0; i < 8; i++ )
    {
        g_work->data[1 + i] = le32dec( (uint32_t *) sctx->job.prevhash + i );
    }
    for ( i = 0; i < 8; i++ )
    {
        g_work->data[9 + i] = be32dec( (uint32_t *) merkle_root + i );
    }

    g_work->data[NTimeIndex] = le32dec(sctx->job.ntime);
    g_work->data[NBitsIndex] = le32dec(sctx->job.nbits);
    g_work->data[20] = 0x80000000;
    g_work->data[31] = 0x00000280;
}
//-----------------------------------------------------------------------------
inline void setTarget(work* work_info, double job_diff)
{
    work_set_target(work_info, job_diff / (256.0 * opt_diff_factor));
}
//-----------------------------------------------------------------------------
inline double calcNetworkDiff(work* work_info)
{
    // sample for diff 43.281 : 1c05ea29
    int nbits_index = NBitsIndex;
    uint32_t nbits = swab32( work_info->data[ nbits_index ] );
    uint32_t bits  = ( nbits & 0xffffff );
    int16_t  shift = ( swab32(nbits) & 0xff ); // 0x1c = 28
    int m;
    double d = (double)0x0000ffff / (double)bits;
    for ( m = shift; m < 29; m++ )
        d *= 256.0;

    for ( m = 29; m < shift; m++ )
        d /= 256.0;

    if ( opt_debug_diff )
        Log::print(Log::LT_Debug, "net diff: %f -> shift %u, bits %08x", d, shift, bits);
    return d;
}
//-----------------------------------------------------------------------------
inline void stratumGenWork(stratum_ctx *sctx, work *g_work)
{
    pthread_mutex_lock( &sctx->work_lock );
    free( g_work->job_id );
    g_work->job_id = strdup( sctx->job.job_id );
    g_work->xnonce2_len = sctx->xnonce2_size;
    g_work->xnonce2 = (unsigned char*) realloc( g_work->xnonce2, sctx->xnonce2_size );
    memcpy( g_work->xnonce2, sctx->job.xnonce2, sctx->xnonce2_size );

    buildExtraHeader( g_work, sctx );

    global::net_diff = calcNetworkDiff( g_work );
    pthread_mutex_unlock( &sctx->work_lock );

//-------------------------------------
#ifdef LYRAAPP_Debug
        char *xnonce2str = abin2hex(g_work->xnonce2, g_work->xnonce2_len);
        Log::print(Log::LT_Debug, "DEBUG: job_id='%s' extranonce2=%s ntime=%08x",
                   g_work->job_id, xnonce2str, swab32( g_work->data[17] ) );
        free(xnonce2str);
#endif
//-------------------------------------

    setTarget(g_work, sctx->job.diff);

    if (stratum_diff != sctx->job.diff)
    {
        char sdiff[32] = { 0 };
        // store for api stats
        stratum_diff = sctx->job.diff;
        if ( opt_showdiff && g_work->targetdiff != stratum_diff )
        {
            snprintf( sdiff, 32, " (%.5f)", g_work->targetdiff );
            Log::print(Log::LT_Warning, "Stratum difficulty set to %g%s", stratum_diff, sdiff );
        }
    }
}
//-----------------------------------------------------------------------------
static void *stratum_thread(void *userdata )
{
    struct thr_info *mythr = (struct thr_info *) userdata;
    char *s;

    stratum.url = (char*) tq_pop(mythr->q, NULL);
    if (!stratum.url)
        goto out;

    Log::print(Log::LT_Info, "Starting Stratum on %s", stratum.url);

    while (1)
    {
        int failures = 0;

        if ( stratum_need_reset )
        {
            stratum_need_reset = false;
            stratum_disconnect( &stratum );
            if (strcmp(stratum.url, global::connectionInfo.rpc_url.c_str())) 
            {
                free( stratum.url );
                stratum.url = strdup(global::connectionInfo.rpc_url.c_str()); 
                Log::print(Log::LT_Blue, "Connection changed to %s", global::connectionInfo.short_url);
            }
            else
                Log::print(Log::LT_Debug, "Stratum connection reset");
        }

        while ( !stratum.curl )
        {
            pthread_mutex_lock( &g_work_lock );
            g_work_time = 0;
            pthread_mutex_unlock( &g_work_lock );
            restart_threads();
            if ( !stratum_connect( &stratum, stratum.url )
                 || !stratum_subscribe( &stratum )
                 || !stratum_authorize(&stratum, global::connectionInfo.rpc_user.c_str(),
                                       global::connectionInfo.rpc_pass.c_str())) 
            {
                stratum_disconnect( &stratum );
                if (global::opt_retries >= 0 && ++failures > global::opt_retries)
                {
                    Log::print(Log::LT_Error, "...terminating workio thread");
                    tq_push(gthr_info[work_thr_id].q, NULL);
                    goto out;
                }
                Log::print(Log::LT_Error, "...retry after %d seconds", global::opt_failPause);
                sleep(global::opt_failPause);
            }
        }

        if ( stratum.job.job_id && ( !g_work_time || strcmp( stratum.job.job_id, global::g_work.job_id ) ) )
        {
            pthread_mutex_lock(&g_work_lock);
            stratumGenWork( &stratum, &global::g_work );
            time(&g_work_time);
            pthread_mutex_unlock(&g_work_lock);
            //           restart_threads();

            if (stratum.job.clean)
            {
                static uint32_t last_block_height;
                if ( last_block_height != stratum.block_height )
                {
                    last_block_height = stratum.block_height;
                    if (global::net_diff > 0.)
                        Log::print(Log::LT_Blue, "lyra2REv2 block %d, diff %.3f", stratum.block_height, global::net_diff);
                    else
                        Log::print(Log::LT_Blue, "%s lyra2REv2 block %d", global::connectionInfo.short_url, stratum.block_height);
                }
                restart_threads();
            }
            else if (global::opt_debug)
            {
                Log::print(Log::LT_Blue, "%s asks job %d for block %d", global::connectionInfo.short_url,
                           strtoul(stratum.job.job_id, NULL, 16), stratum.block_height);
            }
        }  // stratum.job.job_id

        if ( !stratum_socket_full( &stratum, global::opt_timeout ) )
        {
            Log::print(Log::LT_Error, "Stratum connection timeout");
            s = NULL;
        }
        else
            s = stratum_recv_line(&stratum);

        if ( !s )
        {
            stratum_disconnect(&stratum);
            Log::print(Log::LT_Error, "Stratum connection interrupted");
            continue;
        }

        if (!stratum_handle_method(&stratum, s))
            stratum_handle_response(s);

        free(s);
    }  // loop
out:
    return NULL;
}
//-----------------------------------------------------------------------------
static void *workio_thread(void *userdata)
{
    struct thr_info *mythr = (struct thr_info *) userdata;
    CURL *curl;
    bool ok = true;

    curl = curl_easy_init();
    if (!curl)
    {
        Log::print(Log::LT_Error, "CURL initialization failed");
        return NULL;
    }

    while (ok)
    {
        workio_cmd *wc;

        // wait for workio_cmd sent to us, on our queue
        wc = (workio_cmd*) tq_pop(mythr->q, NULL);
        if (!wc)
        {
            ok = false;
            break;
        }

        // process workio_cmd
        ok = workio_submit_work(wc, curl);
        workio_cmd_free(wc);
    }
    tq_freeze(mythr->q);
    curl_easy_cleanup(curl);
    return NULL;
}
//-----------------------------------------------------------------------------
#ifndef _WIN32
static void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        Log::print(Log::LT_Info, "SIGHUP received");
        break;
    case SIGINT:
        signal(sig, SIG_IGN);
        Log::print(Log::LT_Info, "SIGINT received, exiting");
        exit(0);
        break;
    case SIGTERM:
        Log::print(Log::LT_Info, "SIGTERM received, exiting");
        exit(0);
        break;
    }
}
#else
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch (dwType)
    {
    case CTRL_C_EVENT:
        Log::print(Log::LT_Info, "CTRL_C_EVENT received, exiting");
        exit(0);
        break;
    case CTRL_BREAK_EVENT:
        Log::print(Log::LT_Info, "CTRL_BREAK_EVENT received, exiting");
        exit(0);
        break;
    case CTRL_LOGOFF_EVENT:
        Log::print(Log::LT_Info, "CTRL_LOGOFF_EVENT received, exiting");
        exit(0);
        break;
    case CTRL_SHUTDOWN_EVENT:
        Log::print(Log::LT_Info, "CTRL_SHUTDOWN_EVENT received, exiting");
        exit(0);
        break;
    default:
        return false;
    }
    return true;
}
#endif
//-----------------------------------------------------------------------------

#endif // !OtherThreads_INCLUDE_ONCE
