/*
 * Copyright 2018-2019 CryptoGraphics <CrGr@protonmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#ifndef Global_INCLUDE_ONCE
#define Global_INCLUDE_ONCE

#include <string>

// Define to the full name of this package.
#define PACKAGE_NAME "lyclMiner"
// Define to the version of this package.
#define PACKAGE_VERSION "0.2.2"
#define USER_AGENT PACKAGE_NAME "/" PACKAGE_VERSION

namespace lycl
{
    typedef enum
    {
        A_Lyra2REv2,
        A_Lyra2REv3,

        A_None
    } EAlgorithm;
}

struct ConnectionInfo
{
    std::string rpc_user;
    std::string rpc_pass;
    std::string short_url;
    std::string rpc_url;
    std::string rpc_userpass;
    lycl::EAlgorithm algo;
};

//! WorkInfo
struct work
{
    uint32_t data[48];
    uint32_t target[8];

    double targetdiff;
    double shareratio;
    double sharediff;

    int height;

    char *job_id;
    size_t xnonce2_len;
    unsigned char *xnonce2;
};

// TODO: sort these.
namespace global
{
    //! number of worker threads(number of Devices/GPUs)
    extern int numWorkerThreads;
    //! stratum connection info
    extern ConnectionInfo connectionInfo;
    //! Report statistics to the pool. Currently hardcoded. Needs to be properly implemented.
    extern bool opt_stratumStats;
    //! number of retries -1, infinite
    const int opt_retries = -1;
    //! Worker time limit
    const int opt_timeLimit = 0;
    //! timer in seconds between retries
    const int opt_failPause = 10;
    //! Stratum reconnect
    const bool opt_reconnect = true;
    //! Default num hashes per run
    static const int32_t defaultWorkSize = 1048576;
    //! network difficulty
    extern double net_diff;
    //! enable terminal colors for logging
    extern bool use_colors;
    //! global work info
    extern work g_work;
    //! debug log enabled
    const bool opt_debug = false;
    //! Enable CURLOPT_VERBOSE
    const bool opt_protocol = false;
    //! Stratum connection timeout
    const int opt_timeout = 300;
    //! Enable extra nonce.
    extern bool opt_extranonce;
}


//! PROXY SETUP. TODO: review.
extern char *opt_proxy; // original(case 'x':)
extern long opt_proxy_type;
extern char *opt_cert;

//! thread ids
extern int stratum_thr_id;
extern int work_thr_id;

//! Work restart
struct work_restart
{
    volatile uint8_t restart;
    char padding[128 - sizeof(uint8_t)];
};
extern struct work_restart *gwork_restart;
extern struct thr_info *thr;
extern struct thr_info *gthr_info;

//! thread mutexes
extern pthread_mutex_t stats_lock;
extern pthread_mutex_t g_work_lock;

//! thread hashrates and thr hashcount
extern double *thr_hashrates;
extern double *thr_hashcount;
extern uint32_t accepted_count;
extern uint32_t rejected_count;
extern double global_hashcount;
extern double global_hashrate;

//! Show difficulty
const bool opt_showdiff = false;
//! stratum difficulty
extern double stratum_diff; // Gets modified in several stratum factions

extern double opt_diff_factor;

extern bool stratum_need_reset; // Gets modified in stratum_thread()
extern time_t g_work_time; // Gets modified in stratum_thread()


#define JSON_BUF_LEN 512

const bool opt_debug_diff = false;


#endif // !Global_INCLUDE_ONCE
