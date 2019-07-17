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

#include <lyclCore/Stratum.hpp>

//-----------------------------------------------------------------------------
#ifdef _WIN32
#define socket_blocks() (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#define socket_blocks() (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
//-----------------------------------------------------------------------------
stratum_ctx stratum;
//-----------------------------------------------------------------------------
bool send_line(curl_socket_t sock, char *s)
{
    size_t sent = 0;
    int len;

    len = (int) strlen(s);
    s[len++] = '\n';

    while (len > 0)
    {
        struct timeval timeout = {0, 0};
        int n;
        fd_set wd;

        FD_ZERO(&wd);
        FD_SET(sock, &wd);
        if (select((int) (sock + 1), NULL, &wd, NULL, &timeout) < 1)
            return false;
        n = send(sock, s + sent, len, 0);
        if (n < 0) {
            if (!socket_blocks())
                return false;
            n = 0;
        }
        sent += n;
        len -= n;
    }

    return true;
}
//-----------------------------------------------------------------------------
bool stratum_send_line(struct stratum_ctx *sctx, char *s)
{
    bool ret = false;

    if (global::opt_protocol)
        Log::print(Log::LT_Debug, "> %s", s);

    pthread_mutex_lock(&sctx->sock_lock);
    ret = send_line(sctx->sock, s);
    pthread_mutex_unlock(&sctx->sock_lock);

    return ret;
}
//-----------------------------------------------------------------------------
#define RBUFSIZE 2048
#define RECVSIZE (RBUFSIZE - 4)
void stratum_buffer_append(struct stratum_ctx *sctx, const char *s)
{
    size_t old, n;

    old = strlen(sctx->sockbuf);
    n = old + strlen(s) + 1;
    if (n >= sctx->sockbuf_size) {
        sctx->sockbuf_size = n + (RBUFSIZE - (n % RBUFSIZE));
        sctx->sockbuf = (char*) realloc(sctx->sockbuf, sctx->sockbuf_size);
    }
    strcpy(sctx->sockbuf + old, s);
}
//-----------------------------------------------------------------------------
char *stratum_recv_line(struct stratum_ctx *sctx)
{
    ssize_t len, buflen;
    char *tok, *sret = NULL;

    if (!strstr(sctx->sockbuf, "\n"))
    {
        bool ret = true;
        time_t rstart;

        time(&rstart);
        if (!socket_full(sctx->sock, 60))
        {
            Log::print(Log::LT_Error, "stratum_recv_line timed out");
            goto out;
        }
        do
        {
            char s[RBUFSIZE];
            ssize_t n;

            memset(s, 0, RBUFSIZE);
            n = recv(sctx->sock, s, RECVSIZE, 0);
            if (!n) {
                ret = false;
                break;
            }
            if (n < 0)
            {
                if (!socket_blocks() || !socket_full(sctx->sock, 1))
                {
                    ret = false;
                    break;
                }
            }
            else
                stratum_buffer_append(sctx, s);

        } while (time(NULL) - rstart < 60 && !strstr(sctx->sockbuf, "\n"));

        if (!ret)
        {
            Log::print(Log::LT_Error, "stratum_recv_line failed");
            goto out;
        }
    }

    buflen = (ssize_t) strlen(sctx->sockbuf);
    tok = strtok(sctx->sockbuf, "\n");
    if (!tok)
    {
        Log::print(Log::LT_Error, "stratum_recv_line failed to parse a newline-terminated string");
        goto out;
    }
    sret = strdup(tok);
    len = (ssize_t) strlen(sret);

    if (buflen > len + 1)
        memmove(sctx->sockbuf, sctx->sockbuf + len + 1, buflen - len + 1);
    else
        sctx->sockbuf[0] = '\0';

out:
    if (sret && global::opt_protocol)
        Log::print(Log::LT_Debug, "< %s", sret);
    return sret;
}
//-----------------------------------------------------------------------------
bool stratum_connect(struct stratum_ctx *sctx, const char *url)
{
    CURL *curl;
    int rc;

    pthread_mutex_lock(&sctx->sock_lock);
    if (sctx->curl)
        curl_easy_cleanup(sctx->curl);
    sctx->curl = curl_easy_init();
    if (!sctx->curl)
    {
        Log::print(Log::LT_Error, "CURL initialization failed");
        pthread_mutex_unlock(&sctx->sock_lock);
        return false;
    }
    curl = sctx->curl;
    if (!sctx->sockbuf)
    {
        sctx->sockbuf = (char*) calloc(RBUFSIZE, 1);
        sctx->sockbuf_size = RBUFSIZE;
    }
    sctx->sockbuf[0] = '\0';
    pthread_mutex_unlock(&sctx->sock_lock);
    if (url != sctx->url)
    {
        free(sctx->url);
        sctx->url = strdup(url);
    }
    free(sctx->curl_url);
    sctx->curl_url = (char*) malloc(strlen(url));
    sprintf(sctx->curl_url, "http%s", strstr(url, "://"));

    if (global::opt_protocol)
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_URL, sctx->curl_url);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, sctx->curl_err_str);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1);
    if (opt_proxy)
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, opt_proxy);
        curl_easy_setopt(curl, CURLOPT_PROXYTYPE, opt_proxy_type);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPPROXYTUNNEL, 1);
#if LIBCURL_VERSION_NUM >= 0x070f06
    curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockopt_keepalive_cb);
#endif
#if LIBCURL_VERSION_NUM >= 0x071101
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensocket_grab_cb);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &sctx->sock);
#endif
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1);

    rc = curl_easy_perform(curl);
    if (rc)
    {
        Log::print(Log::LT_Error, "Stratum connection failed: %s", sctx->curl_err_str);
        curl_easy_cleanup(curl);
        sctx->curl = NULL;
        return false;
    }

#if LIBCURL_VERSION_NUM < 0x071101
    // CURLINFO_LASTSOCKET is broken on Win64; only use it as a last resort
    curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, (long *)&sctx->sock);
#endif

    return true;
}
//-----------------------------------------------------------------------------
const char *get_stratum_session_id(json_t *val)
{
    json_t *arr_val;
    int i, n;

    arr_val = json_array_get(val, 0);
    if (!arr_val || !json_is_array(arr_val))
        return NULL;
    n = (int) json_array_size(arr_val);
    for (i = 0; i < n; i++)
    {
        const char *notify;
        json_t *arr = json_array_get(arr_val, i);

        if (!arr || !json_is_array(arr))
            break;
        notify = json_string_value(json_array_get(arr, 0));
        if (!notify)
            continue;
        if (!strcasecmp(notify, "mining.notify"))
            return json_string_value(json_array_get(arr, 1));
    }
    return NULL;
}
//-----------------------------------------------------------------------------
bool stratum_parse_extranonce(struct stratum_ctx *sctx, json_t *params, int pndx)
{
    const char* xnonce1;
    int xn2_size;

    xnonce1 = json_string_value(json_array_get(params, pndx));
    if (!xnonce1)
    {
        Log::print(Log::LT_Error, "Failed to get extranonce1");
        goto out;
    }
    xn2_size = (int) json_integer_value(json_array_get(params, pndx+1));
    if (!xn2_size)
    {
        Log::print(Log::LT_Error, "Failed to get extranonce2_size");
        goto out;
    }
    if (xn2_size < 2 || xn2_size > 16)
    {
        Log::print(Log::LT_Info, "Failed to get valid n2size in parse_extranonce");
        goto out;
    }

    pthread_mutex_lock(&sctx->work_lock);
    if (sctx->xnonce1)
        free(sctx->xnonce1);
    sctx->xnonce1_size = strlen(xnonce1) / 2;
    sctx->xnonce1 = (unsigned char*) calloc(1, sctx->xnonce1_size);
    if (!sctx->xnonce1)
    {
        Log::print(Log::LT_Error, "Failed to alloc xnonce1");
        pthread_mutex_unlock(&sctx->work_lock);
        goto out;
    }
    hex2bin(sctx->xnonce1, xnonce1, sctx->xnonce1_size);
    sctx->xnonce2_size = xn2_size;
    pthread_mutex_unlock(&sctx->work_lock);

    if (pndx == 0 && global::opt_debug) // pool dynamic change
        Log::print(Log::LT_Debug, "Stratum set nonce %s with extranonce2 size=%d", xnonce1, xn2_size);

    return true;
out:
    return false;
}
//-----------------------------------------------------------------------------
bool stratum_subscribe(struct stratum_ctx *sctx)
{
    char *s, *sret = NULL;
    const char *sid;
    json_t *val = NULL, *res_val, *err_val;
    json_error_t err;
    bool ret = false, retry = false;

start:
    s = (char*) malloc(128 + (sctx->session_id ? strlen(sctx->session_id) : 0));
    if (retry)
        sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}");
    else if (sctx->session_id)
        sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\", \"%s\"]}", sctx->session_id);
    else
        sprintf(s, "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"" USER_AGENT "\"]}");

    if (!stratum_send_line(sctx, s))
    {
        Log::print(Log::LT_Error, "stratum_subscribe send failed");
        goto out;
    }

    if (!socket_full(sctx->sock, 30))
    {
        Log::print(Log::LT_Error, "stratum_subscribe timed out");
        goto out;
    }

    sret = stratum_recv_line(sctx);
    if (!sret)
        goto out;

    val = json_loads(sret, 0, &err);

    free(sret);
    if (!val)
    {
        Log::print(Log::LT_Error, "JSON decode failed(%d): %s", err.line, err.text);
        goto out;
    }

    res_val = json_object_get(val, "result");
    err_val = json_object_get(val, "error");

    if (!res_val || json_is_null(res_val) || (err_val && !json_is_null(err_val)))
    {
        if (global::opt_debug || retry)
        {
            free(s);
            if (err_val)
                s = json_dumps(err_val, JSON_INDENT(3));
            else
                s = strdup("(unknown reason)");
            Log::print(Log::LT_Error, "JSON-RPC call failed: %s", s);
        }
        goto out;
    }

    sid = get_stratum_session_id(res_val);
    if (global::opt_debug && sid)
        Log::print(Log::LT_Debug, "Stratum session id: %s", sid);

    pthread_mutex_lock(&sctx->work_lock);

    if (sctx->session_id)
        free(sctx->session_id);

    sctx->session_id = sid ? strdup(sid) : NULL;
    sctx->next_diff = 1.0;
    pthread_mutex_unlock(&sctx->work_lock);

    // sid is param 1, extranonce params are 2 and 3
    if (!stratum_parse_extranonce(sctx, res_val, 1))
    {
        goto out;
    }

    ret = true;

out:
    free(s);
    if (val)
        json_decref(val);

    if (!ret)
    {
        if (sret && !retry) 
        {
            retry = true;
            goto start;
        }
    }

    return ret;
}
//-----------------------------------------------------------------------------
