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

#ifndef WorkIO_INCLUDE_ONCE
#define WorkIO_INCLUDE_ONCE

#include <unistd.h> // sleep()
#include <curl/curl.h> // CURL, curl_global_init...
#include <jansson.h> // JSON

#include <lyclCore/Log.hpp>
#include <lyclCore/Network.hpp>
#include <lyclCore/Sha256.hpp>
#include <lyclCore/Stratum.hpp>

#define NTimeIndex 17
#define NBitsIndex 18
#define NonceIndex 19   // 32 bit offset
#define WorkCmpSize 76
#define WorkDataSize 128

//-----------------------------------------------------------------------------
inline void restart_threads()
{
    for ( int i = 0; i < global::numWorkerThreads; i++)
        gwork_restart[i].restart = 1;
}
//-----------------------------------------------------------------------------
// Work IO
//-----------------------------------------------------------------------------
struct workio_cmd
{
    struct thr_info *thr;
    union
    {
        struct work *work;
    } u;
};
//-----------------------------------------------------------------------------
inline void format_hashrate(double hashrate, char *output)
{
    char prefix = '\0';

    if (hashrate < 10000)
    {
        // nop
    }
    else if (hashrate < 1e7)
    {
        prefix = 'k';
        hashrate *= 1e-3;
    }
    else if (hashrate < 1e10)
    {
        prefix = 'M';
        hashrate *= 1e-6;
    }
    else if (hashrate < 1e13)
    {
        prefix = 'G';
        hashrate *= 1e-9;
    }
    else
    {
        prefix = 'T';
        hashrate *= 1e-12;
    }

    sprintf(output,
            prefix ? "%.2f %cH/s" : "%.2f H/s%c",
            hashrate, prefix);
}
//-----------------------------------------------------------------------------
bool submit_upstream_work( CURL *curl, work *work_info );
//-----------------------------------------------------------------------------
bool work_decode( const json_t *val, struct work *work );
//-----------------------------------------------------------------------------
bool workio_submit_work(struct workio_cmd *wc, CURL *curl);
//-----------------------------------------------------------------------------
inline void workFree(work *w)
{
    if (w->job_id)
        free(w->job_id);
    if (w->xnonce2)
        free(w->xnonce2);
}
inline void workCopy(work *dest, const work *src)
{
    memcpy(dest, src, sizeof(struct work));
    if (src->job_id)
        dest->job_id = strdup(src->job_id);
    if (src->xnonce2)
    {
        dest->xnonce2 = (unsigned char*) malloc(src->xnonce2_len);
        memcpy(dest->xnonce2, src->xnonce2, src->xnonce2_len);
    }
}
//-----------------------------------------------------------------------------
inline void workio_cmd_free(struct workio_cmd *wc)
{
    if (!wc)
        return;

    workFree(wc->u.work);
    free(wc->u.work);

    memset(wc, 0, sizeof(*wc)); // poison
    free(wc);
}
//-----------------------------------------------------------------------------
inline bool submit_work(struct thr_info *thr, const work* work_info)
{
    workio_cmd *wc;
    // fill out work request message
    wc = (workio_cmd *)calloc(1, sizeof(*wc));
    if (!wc)
        return false;
    wc->u.work = (work*)malloc(sizeof(*work_info));
    if (!wc->u.work)
        goto err_out;
    wc->thr = thr;
    workCopy(wc->u.work, work_info);

    // send solution to workio thread
    if (!tq_push(gthr_info[work_thr_id].q, wc))
        goto err_out;
    return true;
err_out:
    workio_cmd_free(wc);
    return false;
}
//-----------------------------------------------------------------------------

#endif // !WorkIO_INCLUDE_ONCE
