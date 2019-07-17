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

#ifndef Network_INCLUDE_ONCE
#define Network_INCLUDE_ONCE

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <netinet/tcp.h> // SOL_TCP
#endif

#include <curl/curl.h> // CURL

//-----------------------------------------------------------------------------
#if LIBCURL_VERSION_NUM >= 0x070f06
inline int sockopt_keepalive_cb(void *userdata, curl_socket_t fd, curlsocktype purpose)
{
#ifdef __linux
    int tcp_keepcnt = 3;
#endif
    int tcp_keepintvl = 50;
    int tcp_keepidle = 50;
#ifndef WIN32
    int keepalive = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)))
        return 1;
#ifdef __linux
    if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &tcp_keepcnt, sizeof(tcp_keepcnt)))
        return 1;
    if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &tcp_keepidle, sizeof(tcp_keepidle)))
        return 1;
    if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &tcp_keepintvl, sizeof(tcp_keepintvl)))
        return 1;
#endif // __linux
#ifdef __APPLE_CC__
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &tcp_keepintvl, sizeof(tcp_keepintvl)))
        return 1;
#endif // __APPLE_CC__
#else // WIN32
    struct tcp_keepalive vals;
    vals.onoff = 1;
    vals.keepalivetime = tcp_keepidle * 1000;
    vals.keepaliveinterval = tcp_keepintvl * 1000;
    DWORD outputBytes;
    if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, &vals, sizeof(vals), NULL, 0, &outputBytes, NULL, NULL))
        return 1;
#endif // WIN32

    return 0;
}
#endif
//-----------------------------------------------------------------------------
#if LIBCURL_VERSION_NUM >= 0x071101
inline curl_socket_t opensocket_grab_cb(void *clientp, curlsocktype purpose, struct curl_sockaddr *addr)
{
    curl_socket_t *sock = (curl_socket_t*) clientp;
    *sock = socket(addr->family, addr->socktype, addr->protocol);
    return *sock;
}
#endif
//-----------------------------------------------------------------------------

#endif // !Network_INCLUDE_ONCE
