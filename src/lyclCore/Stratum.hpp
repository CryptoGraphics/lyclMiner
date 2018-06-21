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

#ifndef Stratum_INCLUDE_ONCE
#define Stratum_INCLUDE_ONCE

//#include <bsd/sys/endian.h> // le16dec
#include <external/endian.h>

#include <jansson.h> // JSON
#include <lyclCore/Threading.hpp>
#include <lyclCore/Network.hpp>
#include <lyclCore/Utils.hpp>

struct stratum_job
{
    char *job_id;
    unsigned char prevhash[32];
    size_t coinbase_size;
    unsigned char *coinbase;
    unsigned char *xnonce2;
    int merkle_count;
    unsigned char **merkle;
    unsigned char version[4];
    unsigned char nbits[4];
    unsigned char ntime[4];
    bool clean;
    double diff;
};

struct stratum_ctx
{
    char *url;

    CURL *curl;
    char *curl_url;
    char curl_err_str[CURL_ERROR_SIZE];
    curl_socket_t sock;
    size_t sockbuf_size;
    char *sockbuf;
    pthread_mutex_t sock_lock;

    double next_diff;
    double sharediff;

    char *session_id;
    size_t xnonce1_size;
    unsigned char *xnonce1;
    size_t xnonce2_size;
    struct stratum_job job;
    struct work work;
    pthread_mutex_t work_lock;

    int block_height;
};

extern stratum_ctx stratum;


#define RBUFSIZE 2048


//-----------------------------------------------------------------------------
// helper method
inline bool stratumHandleResponse(json_t* val)
{
    bool valid = false;
    json_t *err_val, *res_val, *id_val;
    res_val = json_object_get( val, "result" );
    err_val = json_object_get( val, "error" );
    id_val  = json_object_get( val, "id" );

    if ( !res_val || json_integer_value(id_val) < 4 )
         return false;
    valid = json_is_true( res_val );
    share_result(valid, NULL, err_val ?
                 json_string_value( json_array_get(err_val, 1) ) : NULL );
    return true;
}
//-----------------------------------------------------------------------------
inline bool stratum_handle_response( char *buf )
{
    json_t *val, *id_val;
    json_error_t err;
    bool ret = false;

    val = json_loads(buf, 0, &err);
    if (!val)
    {
        Log::print(Log::LT_Info, "JSON decode failed(%d): %s", err.line, err.text);
        goto out;
    }
    json_object_get( val, "result" );
    id_val = json_object_get( val, "id" );

    if ( !id_val || json_is_null(id_val) )
        goto out;

    if ( !stratumHandleResponse( val ) )
        goto out;

    ret = true;
out:
    if (val)
        json_decref(val);
    return ret;
}
//-----------------------------------------------------------------------------
bool stratum_send_line(struct stratum_ctx *sctx, char *s);
//-----------------------------------------------------------------------------
static bool socket_full(curl_socket_t sock, int timeout)
{
    struct timeval tv;
    fd_set rd;

    FD_ZERO(&rd);
    FD_SET(sock, &rd);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (select((int)(sock + 1), &rd, NULL, NULL, &tv) > 0)
        return true;
    return false;
}
//-----------------------------------------------------------------------------
inline bool stratum_socket_full(struct stratum_ctx *sctx, int timeout)
{
    return strlen(sctx->sockbuf) || socket_full(sctx->sock, timeout);
}
//-----------------------------------------------------------------------------
char *stratum_recv_line(struct stratum_ctx *sctx);
//-----------------------------------------------------------------------------
bool stratum_connect(struct stratum_ctx *sctx, const char *url);
//-----------------------------------------------------------------------------
inline void stratum_disconnect(struct stratum_ctx *sctx)
{
    pthread_mutex_lock(&sctx->sock_lock);
    if (sctx->curl)
    {
        curl_easy_cleanup(sctx->curl);
        sctx->curl = NULL;
        sctx->sockbuf[0] = '\0';
    }
    pthread_mutex_unlock(&sctx->sock_lock);
}
//-----------------------------------------------------------------------------
bool stratum_parse_extranonce(struct stratum_ctx *sctx, json_t *params, int pndx);
//-----------------------------------------------------------------------------
bool stratum_subscribe(struct stratum_ctx *sctx);
//-----------------------------------------------------------------------------
/**
 * Extract block height     L H... here len=3, height=0x1333e8
 * "...0000000000ffffffff2703e83313062f503253482f043d61105408"
 */
static uint32_t getBlockHeight(struct stratum_ctx *sctx)
{
    uint32_t height = 0;
    uint8_t hlen = 0, *p, *m;

    // find 0xffff tag
    p = (uint8_t*) sctx->job.coinbase + 32;
    m = p + 128;
    while (*p != 0xff && p < m) p++;
    while (*p == 0xff && p < m) p++;
    if (*(p-1) == 0xff && *(p-2) == 0xff)
    {
        p++; hlen = *p;
        p++; height = le16dec(p);
        p += 2;
        switch (hlen)
        {
            case 4:
                height += 0x10000UL * le16dec(p);
                break;
            case 3:
                height += 0x10000UL * (*p);
                break;
        }
    }
    return height;
}
//-----------------------------------------------------------------------------
static bool stratum_notify(struct stratum_ctx *sctx, json_t *params)
{
    const char *job_id, *prevhash, *coinb1, *coinb2, *version, *nbits, *stime;
    size_t coinb1_size, coinb2_size;
    bool clean, ret = false;
    int merkle_count, i, p = 0;
    json_t *merkle_arr;
    unsigned char **merkle = NULL;


    job_id = json_string_value(json_array_get(params, p++));
    prevhash = json_string_value(json_array_get(params, p++));
    coinb1 = json_string_value(json_array_get(params, p++));
    coinb2 = json_string_value(json_array_get(params, p++));
    merkle_arr = json_array_get(params, p++);
    if (!merkle_arr || !json_is_array(merkle_arr))
    goto out;
    merkle_count = (int) json_array_size(merkle_arr);
    version = json_string_value(json_array_get(params, p++));
    nbits = json_string_value(json_array_get(params, p++));
    stime = json_string_value(json_array_get(params, p++));
    clean = json_is_true(json_array_get(params, p)); p++;

    if (!job_id || !prevhash || !coinb1 || !coinb2 || !version || !nbits || !stime ||
        strlen(prevhash) != 64 || strlen(version) != 8 ||
        strlen(nbits) != 8 || strlen(stime) != 8)
    {
        Log::print(Log::LT_Error, "Stratum notify: invalid parameters");
        goto out;
    }

    merkle = (unsigned char**) malloc(merkle_count * sizeof(char *));
    for (i = 0; i < merkle_count; i++)
    {
        const char *s = json_string_value(json_array_get(merkle_arr, i));
        if (!s || strlen(s) != 64)
        {
            while (i--)
                free(merkle[i]);
            free(merkle);
            Log::print(Log::LT_Error, "Stratum notify: invalid Merkle branch");
            goto out;
        }
        merkle[i] = (unsigned char*) malloc(32);
        hex2bin(merkle[i], s, 32);
    }

    pthread_mutex_lock(&sctx->work_lock);

    coinb1_size = strlen(coinb1) / 2;
    coinb2_size = strlen(coinb2) / 2;
    sctx->job.coinbase_size = coinb1_size + sctx->xnonce1_size +
              sctx->xnonce2_size + coinb2_size;
    sctx->job.coinbase = (unsigned char*) realloc(sctx->job.coinbase, sctx->job.coinbase_size);
    sctx->job.xnonce2 = sctx->job.coinbase + coinb1_size + sctx->xnonce1_size;
    hex2bin(sctx->job.coinbase, coinb1, coinb1_size);
    memcpy(sctx->job.coinbase + coinb1_size, sctx->xnonce1, sctx->xnonce1_size);

    if (!sctx->job.job_id || strcmp(sctx->job.job_id, job_id))
        memset(sctx->job.xnonce2, 0, sctx->xnonce2_size);

    hex2bin(sctx->job.xnonce2 + sctx->xnonce2_size, coinb2, coinb2_size);
    free(sctx->job.job_id);
    sctx->job.job_id = strdup(job_id);
    hex2bin(sctx->job.prevhash, prevhash, 32);

    sctx->block_height = getBlockHeight(sctx);

    for (i = 0; i < sctx->job.merkle_count; i++)
        free(sctx->job.merkle[i]);

    free(sctx->job.merkle);
    sctx->job.merkle = merkle;
    sctx->job.merkle_count = merkle_count;

    hex2bin(sctx->job.version, version, 4);
    hex2bin(sctx->job.nbits, nbits, 4);
    hex2bin(sctx->job.ntime, stime, 4);
    sctx->job.clean = clean;

    sctx->job.diff = sctx->next_diff;

    pthread_mutex_unlock(&sctx->work_lock);

    ret = true;

out:
    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_set_difficulty(struct stratum_ctx *sctx, json_t *params)
{
    double diff;

    diff = json_number_value(json_array_get(params, 0));
    if (diff == 0)
        return false;

    pthread_mutex_lock(&sctx->work_lock);
    sctx->next_diff = diff;
    pthread_mutex_unlock(&sctx->work_lock);

    // store for api stats
    stratum_diff = diff;

    Log::print(Log::LT_Warning, "Stratum difficulty set to %g", diff);

    return true;
}
//-----------------------------------------------------------------------------
static bool stratum_reconnect(struct stratum_ctx *sctx, json_t *params)
{
    json_t *port_val;
    char *url;
    const char *host;
    int port;

    host = json_string_value(json_array_get(params, 0));
    port_val = json_array_get(params, 1);
    if (json_is_string(port_val))
        port = atoi(json_string_value(port_val));
    else
        port = (int) json_integer_value(port_val);
    if (!host || !port)
        return false;

    url = (char*) malloc(32 + strlen(host));
    sprintf(url, "stratum+tcp://%s:%d", host, port);

    if (!global::opt_reconnect)
    {
        Log::print(Log::LT_Info, "Ignoring request to reconnect to %s", url);
        free(url);
        return true;
    }

    Log::print(Log::LT_Notice, "Server requested reconnection to %s", url);

    free(sctx->url);
    sctx->url = url;
    stratum_disconnect(sctx);

    return true;
}
//-----------------------------------------------------------------------------
static bool json_object_set_error(json_t *result, int code, const char *msg)
{
    json_t *val = json_object();
    json_object_set_new(val, "code", json_integer(code));
    json_object_set_new(val, "message", json_string(msg));
    return json_object_set_new(result, "error", val) != -1;
}
//-----------------------------------------------------------------------------
inline void get_currentalgo(char* buf, int sz)
{
    snprintf(buf, sz, "%s", "lyra2REv2");
}
//-----------------------------------------------------------------------------
// allow to report algo perf to the pool for algo stats
static bool stratum_benchdata(json_t *result, json_t *params, int thr_id)
{
    // TODO: not finished.
    char algo[64] = { 0 };
    char devicename[80] = { 0 };
    char vendorid[32] = { 0 };
    char compiler[32] = { 0 };
    char arch[16] = { 0 };
    char os[8];
    char *p;
    double cpufreq = 0;
    json_t *val;

    if (!global::opt_stratumStats)
        return false;

    get_currentalgo(algo, sizeof(algo));

#ifdef _WIN32
    strcpy(os, "windows");
#elif defined __linux__
    strcpy(os, "linux");
#else
    strcpy(os, "unknown");
#endif

    compiler[31] = '\0';

    val = json_object();
    json_object_set_new(val, "algo", json_string(algo));
    json_object_set_new(val, "type", json_string("gpu"));
    json_object_set_new(val, "device", json_string(devicename));
    json_object_set_new(val, "vendorid", json_string(vendorid));
    json_object_set_new(val, "arch", json_string(arch));
    json_object_set_new(val, "freq", json_integer(0));
    json_object_set_new(val, "memf", json_integer(0));
    json_object_set_new(val, "power", json_integer(0));
    json_object_set_new(val, "khashes", json_real((double)global_hashrate / 1000.0));
    json_object_set_new(val, "intensity", json_real(0));
    json_object_set_new(val, "throughput", json_integer(0));
    json_object_set_new(val, "client", json_string(PACKAGE_NAME "/" PACKAGE_VERSION));
    json_object_set_new(val, "os", json_string(os));
    json_object_set_new(val, "driver", json_string(compiler));

    json_object_set_new(result, "result", val);

    return true;
}
//-----------------------------------------------------------------------------
static bool stratum_get_stats(struct stratum_ctx *sctx, json_t *id, json_t *params)
{
    char *s;
    json_t *val;
    bool ret;

    if (!id || json_is_null(id))
        return false;

    val = json_object();
    json_object_set(val, "id", id);

    ret = stratum_benchdata(val, params, 0);

    if (!ret)
    {
        json_object_set_error(val, 1, "disabled"); //EPERM
    }
    else
    {
        json_object_set_new(val, "error", json_null());
    }

    s = json_dumps(val, 0);
    ret = stratum_send_line(sctx, s);
    json_decref(val);
    free(s);

    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_unknown_method(struct stratum_ctx *sctx, json_t *id)
{
    char *s;
    json_t *val;
    bool ret = false;

    if (!id || json_is_null(id))
        return ret;

    val = json_object();
    json_object_set(val, "id", id);
    json_object_set_new(val, "result", json_false());
    json_object_set_error(val, 38, "unknown method"); // ENOSYS

    s = json_dumps(val, 0);
    ret = stratum_send_line(sctx, s);
    json_decref(val);
    free(s);

    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_pong(struct stratum_ctx *sctx, json_t *id)
{
    char buf[64];
    bool ret = false;

    if (!id || json_is_null(id))
        return ret;

    sprintf(buf, "{\"id\":%d,\"result\":\"pong\",\"error\":null}",
        (int) json_integer_value(id));
    ret = stratum_send_line(sctx, buf);

    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_get_algo(struct stratum_ctx *sctx, json_t *id, json_t *params)
{
    char algo[64] = { 0 };
    char *s;
    json_t *val;
    bool ret = true;

    if (!id || json_is_null(id))
        return false;

    get_currentalgo(algo, sizeof(algo));

    val = json_object();
    json_object_set(val, "id", id);
    json_object_set_new(val, "error", json_null());
    json_object_set_new(val, "result", json_string(algo));

    s = json_dumps(val, 0);
    ret = stratum_send_line(sctx, s);
    json_decref(val);
    free(s);

    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_get_version(struct stratum_ctx *sctx, json_t *id)
{
    char *s;
    json_t *val;
    bool ret;
    
    if (!id || json_is_null(id))
        return false;

    val = json_object();
    json_object_set(val, "id", id);
    json_object_set_new(val, "error", json_null());
    json_object_set_new(val, "result", json_string(USER_AGENT));
    s = json_dumps(val, 0);
    ret = stratum_send_line(sctx, s);
    json_decref(val);
    free(s);

    return ret;
}
//-----------------------------------------------------------------------------
static bool stratum_show_message(struct stratum_ctx *sctx, json_t *id, json_t *params)
{
    char *s;
    json_t *val;
    bool ret;

    val = json_array_get(params, 0);
    if (val)
        Log::print(Log::LT_Notice, "MESSAGE FROM SERVER: %s", json_string_value(val));
    
    if (!id || json_is_null(id))
        return true;

    val = json_object();
    json_object_set(val, "id", id);
    json_object_set_new(val, "error", json_null());
    json_object_set_new(val, "result", json_true());
    s = json_dumps(val, 0);
    ret = stratum_send_line(sctx, s);
    json_decref(val);
    free(s);

    return ret;
}
//-----------------------------------------------------------------------------
inline bool stratum_handle_method(struct stratum_ctx *sctx, const char *s)
{
    json_t *val, *id, *params;
    json_error_t err;
    const char *method;
    bool ret = false;

    //val = JSON_LOADS(s, &err);
    val = json_loads(s, 0, &err);
    if (!val)
    {
        Log::print(Log::LT_Error, "JSON decode failed(%d): %s", err.line, err.text);
        goto out;
    }

    method = json_string_value(json_object_get(val, "method"));
    if (!method)
        goto out;

    params = json_object_get(val, "params");

    id = json_object_get(val, "id");

    if (!strcasecmp(method, "mining.notify"))
    {
        ret = stratum_notify(sctx, params);
        goto out;
    }
    if (!strcasecmp(method, "mining.ping"))
    {
        if (global::opt_debug)
            Log::print(Log::LT_Debug, "Pool ping");
        ret = stratum_pong(sctx, id);
        goto out;
    }
    if (!strcasecmp(method, "mining.set_difficulty"))
    {
        ret = stratum_set_difficulty(sctx, params);
        goto out;
    }
    if (!strcasecmp(method, "mining.set_extranonce"))
    {
        ret = stratum_parse_extranonce(sctx, params, 0);
        goto out;
    }
    if (!strcasecmp(method, "client.reconnect"))
    {
        ret = stratum_reconnect(sctx, params);
        goto out;
    }
    if (!strcasecmp(method, "client.get_algo"))
    {
        // will prevent wrong algo parameters on a pool, will be used as test on rejects
        Log::print(Log::LT_Notice, "Pool asked your algo parameter");
        ret = stratum_get_algo(sctx, id, params);
        goto out;
    }
    if (!strcasecmp(method, "client.get_stats")) {
        // optional to fill device benchmarks
        ret = stratum_get_stats(sctx, id, params);
        goto out;
    }
    if (!strcasecmp(method, "client.get_version")) {
        ret = stratum_get_version(sctx, id);
        goto out;
    }
    if (!strcasecmp(method, "client.show_message")) {
        ret = stratum_show_message(sctx, id, params);
        goto out;
    }

    if (!ret) {
        // don't fail = disconnect stratum on unknown (and optional?) methods
        if (global::opt_debug)
            Log::print(Log::LT_Warning, "unknown stratum method %s!", method);
        ret = stratum_unknown_method(sctx, id);
    }
out:
    if (val)
        json_decref(val);

    return ret;
}
//-----------------------------------------------------------------------------
inline bool stratum_authorize(struct stratum_ctx *sctx, const char *user, const char *pass)
{
    json_t *val = NULL, *res_val, *err_val;
    char *s, *sret;
    json_error_t err;
    bool ret = false;

    s = (char*) malloc(80 + strlen(user) + strlen(pass));
    sprintf(s, "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"%s\"]}", user, pass);

    if (!stratum_send_line(sctx, s))
        goto out;

    while (1)
    {
        sret = stratum_recv_line(sctx);
        if (!sret)
            goto out;
        if (!stratum_handle_method(sctx, sret))
            break;
        free(sret);
    }

    val = json_loads(sret, 0, &err);

    free(sret);
    if (!val)
    {
        Log::print(Log::LT_Error, "JSON decode failed(%d): %s", err.line, err.text);
        goto out;
    }

    res_val = json_object_get(val, "result");
    err_val = json_object_get(val, "error");

    if (!res_val || json_is_false(res_val) || (err_val && !json_is_null(err_val)))
    {
        Log::print(Log::LT_Error, "Stratum authentication failed");
        goto out;
    }

    ret = true;

    if (!global::opt_extranonce)
        goto out;

    // subscribe to extranonce (optional)
    sprintf(s, "{\"id\": 3, \"method\": \"mining.extranonce.subscribe\", \"params\": []}");

    if (!stratum_send_line(sctx, s))
        goto out;

    if (!socket_full(sctx->sock, 3))
    {
        if (global::opt_debug)
            Log::print(Log::LT_Debug, "stratum extranonce subscribe timed out");
        goto out;
    }

    sret = stratum_recv_line(sctx);
    if (sret)
    {
        json_t *extra = json_loads(sret, 0, &err);

        if (!extra)
        {
            Log::print(Log::LT_Warning, "JSON decode failed(%d): %s", err.line, err.text); 
        }
        else
        {
            if (json_integer_value(json_object_get(extra, "id")) != 3)
            {
                // we receive a standard method if extranonce is ignored
                if (!stratum_handle_method(sctx, sret))
                    Log::print(Log::LT_Warning, "Stratum answer id is not correct!"); 
            }
//          res_val = json_object_get(extra, "result");
//          if (opt_debug && (!res_val || json_is_false(res_val)))
//              applog(LOG_DEBUG, "extranonce subscribe not supported");
            json_decref(extra);
        }
        free(sret);
    }

out:
    free(s);
    if (val)
        json_decref(val);

    return ret;
}
//-----------------------------------------------------------------------------
inline bool std_stratum_handle_response( json_t *val )
{
    bool valid = false;
    json_t *err_val, *res_val, *id_val;
    res_val = json_object_get( val, "result" );
    err_val = json_object_get( val, "error" );
    id_val  = json_object_get( val, "id" );

    if ( !res_val || json_integer_value(id_val) < 4 )
         return false;
    valid = json_is_true( res_val );
    share_result( valid, NULL, err_val ?
                  json_string_value( json_array_get(err_val, 1) ) : NULL );
    return true;
}
//-----------------------------------------------------------------------------

#endif // !Stratum_INCLUDE_ONCE
