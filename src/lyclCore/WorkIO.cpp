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

#include <lyclCore/WorkIO.hpp>

//-----------------------------------------------------------------------------
void buildStratumRequest(char* req, work* work_info)
{
    // should be char* instead of unsigned char*
    char *xnonce2str;
    uint32_t ntime,       nonce;
    char     ntimestr[9], noncestr[9];
    le32enc( &ntime, work_info->data[NTimeIndex] );
    le32enc( &nonce, work_info->data[NonceIndex] );
    bin2hex( ntimestr, (unsigned char*)(&ntime), sizeof(uint32_t) );
    bin2hex( noncestr, (unsigned char*)(&nonce), sizeof(uint32_t) );
    xnonce2str = abin2hex( work_info->xnonce2, work_info->xnonce2_len );
    snprintf(req, JSON_BUF_LEN,
             "{\"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"], \"id\":4}",
             global::connectionInfo.rpc_user.c_str(), work_info->job_id, xnonce2str, ntimestr, noncestr); 
    free( xnonce2str );
}
//-----------------------------------------------------------------------------
bool submit_upstream_work( CURL *curl, work *work_info )
{
    json_t *val, *res;
    char req[JSON_BUF_LEN];
    int i;

    // pass if the previous hash is not the current previous hash
    if ( memcmp( &work_info->data[1], &global::g_work.data[1], 32 ) )
    {
        if (global::opt_debug)
            Log::print(Log::LT_Debug, "DEBUG: stale work detected, discarding");
        return true;
    }

    buildStratumRequest( req, work_info);
    if ( !stratum_send_line( &stratum, req ) )
    {
        Log::print(Log::LT_Error, "submit_upstream_work stratum_send_line failed");
        return false;
    }
    return true;
}
//-----------------------------------------------------------------------------
bool workio_submit_work(struct workio_cmd *wc, CURL *curl)
{
    int failures = 0;

    // submit solution to bitcoin via JSON-RPC
    while (!submit_upstream_work(curl, wc->u.work))
    {
        if ((global::opt_retries >= 0) && (++failures > global::opt_retries))
        {
            Log::print(Log::LT_Error, "...terminating workio thread");
            return false;
        }
        // pause, then restart work-request loop
        Log::print(Log::LT_Error, "...retry after %d seconds", global::opt_failPause);
        sleep(global::opt_failPause);
    }
    return true;
}
//-----------------------------------------------------------------------------
