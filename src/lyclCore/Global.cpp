/*
 * Copyright 2018-2019 CryptoGraphics <CrGr@protonmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#include <lyclCore/Global.hpp>

namespace global
{
    //! number of worker threads(number of Devices/GPUs)
    int numWorkerThreads = 0;
    //! stratum connection info
    ConnectionInfo connectionInfo;
    //! Report statistics to the pool.
    bool opt_stratumStats = false;
    //! enable terminal colors for logging
    bool use_colors = false;
    //! network difficulty
    double net_diff = 0.;
    //! global work info
    work g_work = {{ 0 }};


    //! Enable extra nonce.
    bool opt_extranonce = true;
}

//! PROXY SETUP. Needs to be implemented
char *opt_proxy; // original(case 'x':)
long opt_proxy_type;
char *opt_cert;

//! thread ids
int stratum_thr_id;
int work_thr_id;

struct work_restart *gwork_restart = NULL;
struct thr_info *thr;
struct thr_info *gthr_info;

//! thread mutexes
pthread_mutex_t stats_lock;
pthread_mutex_t g_work_lock;

//! thread hashrates and thr hashcount
double *thr_hashrates;
double *thr_hashcount;
uint32_t accepted_count = 0;
uint32_t rejected_count = 0;
double global_hashcount = 0;
double global_hashrate = 0;

//! stratum difficulty
double stratum_diff = 0.;

double opt_diff_factor = 1.0;

bool stratum_need_reset = false;
time_t g_work_time = 0;

