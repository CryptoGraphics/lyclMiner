/*
 * Copyright 2018-2019 CryptoGraphics <CrGr@protonmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version. See LICENSE for more details.
 */

#include <iostream>

#include <lyclCore/OtherThreads.hpp>
#include <lyclCore/Global.hpp>
#include <lyclCore/Blake256.hpp>
#include <lyclCore/Uint256.hpp>
#include <lyclCore/ConfigFile.hpp>
#include <external/endian.h>

// Applets
#include <lyclApplets/AppLyra2REv2.hpp>
#include <lyclApplets/AppLyra2REv3.hpp>

#include <lyclHostValidators/BMW.hpp>

#include <chrono> // timing
#include <algorithm> // sort

//-----------------------------------------------------------------------------
// compute the diff ratio between a found hash and the target
inline double hash_target_ratio(const uint32_t* hash, uint32_t* target)
{
    uint256 h, t;
    double dhash;

    if (!opt_showdiff)
        return 0.0;

    memcpy(&t, (void*) target, 32);
    memcpy(&h, (void*) hash, 32);

    dhash = h.getdouble();
    if (dhash > 0.)
        return t.getdouble() / dhash;
    else
        return dhash;
}
//-----------------------------------------------------------------------------
// store ratio in work struct
inline void work_set_target_ratio(work* work_info, const uint32_t* hash)
{
    // only if the option is enabled (to reduce cpu usage)
    if (opt_showdiff)
    {
        work_info->shareratio = hash_target_ratio(hash, work_info->target);
        work_info->sharediff = work_info->targetdiff * work_info->shareratio;
    }
}
//-----------------------------------------------------------------------------
bool fulltestU32x8(const lycl::uint32x8& hash, const uint32_t *target)
{
    bool rc = true;

    for (int i = 7; i >= 0; i--)
    {
        if (hash.h[i] > target[i])
        {
            rc = false;
            break;
        }
        if (hash.h[i] < target[i])
        {
            rc = true;
            break;
        }
    }

    if (global::opt_debug)
    {
        uint32_t hash_be[8], target_be[8];
        char hash_str[65], target_str[65];

        for (int i = 0; i < 8; i++)
        {
            be32enc(hash_be + i, hash.h[7 - i]);
            be32enc(target_be + i, target[7 - i]);
        }
        bin2hex(hash_str, (unsigned char *)hash_be, 32);
        bin2hex(target_str, (unsigned char *)target_be, 32);

        Log::print(Log::LT_Debug, "DEBUG: %s\nHash:   %s\nTarget: %s",
                   rc ? "hash <= target" : "hash > target (false positive)",
                   hash_str, target_str);
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Lyra2REv2 worker
//-----------------------------------------------------------------------------
void* workerThread_lyra2REv2( void *userdata )
{
    thr_info *mythr = (thr_info *) userdata;
    int thr_id = mythr->id;

    // Init device context.
    lycl::AppLyra2REv2 deviceCtx;
    lycl::device clDevice = mythr->clDevice;
    if (!deviceCtx.onInit(mythr->clDevice))
    {
        Log::print(Log::LT_Error, "Failed to initialize device(%d)! Skipping...", thr_id);
        // exit
        deviceCtx.onDestroy();
        tq_freeze(mythr->q);
        return NULL;
    }

    work workInfo;
    memset(&workInfo, 0, sizeof(work));
    time_t firstwork_time = 0;

    std::chrono::steady_clock::time_point m_start;
    std::chrono::steady_clock::time_point m_end;

    //-------------------------------------
    // compute max runs
    // 4294967295 max nonce
    const uint64_t numNonces = 4294967296ULL / (uint64_t)global::numWorkerThreads;
    uint32_t maxRuns = (uint32_t)(numNonces / (uint32_t)clDevice.workSize);
    // last device
    if (thr_id == (global::numWorkerThreads-1))
        maxRuns += (uint32_t)(numNonces % global::numWorkerThreads);

    // Host side validation
    //std::vector<lycl::lyraHash> m_hashes(clDevice.workSize);
    std::vector<uint32_t> m_potentialNonces;
    std::vector<uint32_t> m_nonces;

    Log::print(Log::LT_Debug, "Device: %d max runs: %u", thr_id, maxRuns);

    uint32_t numRuns = 0;

    for (;;)
    {
        uint64_t hashes_done;

        //-------------------------------------
        // wait for diff
        while(time(NULL) >= g_work_time + 120)
            sleep(1);

        pthread_mutex_lock( &g_work_lock );
        //-------------------------------------
        // get new work from stratum.
        if (numRuns >= maxRuns)
        {
            // generate new work
            stratumGenWork(&stratum, &global::g_work);
        }
        //-------------------------------------
        // setup a nonce range for each worker thread
        const int32_t workCmpSize = WorkCmpSize;
        if ( memcmp( workInfo.data, global::g_work.data, workCmpSize)
             && (stratum.job.clean || (numRuns >= maxRuns) || (workInfo.job_id != global::g_work.job_id)) )
        {
            Log::print(Log::LT_Debug, "Device: %d has completed its nonce range", thr_id);

            // get new work
            workFree(&workInfo );
            workCopy(&workInfo, &global::g_work);
            // reset run counter
            numRuns = 0;
        }

        pthread_mutex_unlock( &g_work_lock );
        //-------------------------------------
        // if work is not available
        if (!workInfo.data[0])
        {
            sleep(1);
            continue;
        }
        //-------------------------------------
        // time limit
        if ( global::opt_timeLimit && firstwork_time )
        {
            int passed = (int)( time(NULL) - firstwork_time );
            int remain = (int)( global::opt_timeLimit - passed );
            if ( remain < 0 )
            {
                if ( thr_id != 0 )
                {
                    sleep(1);
                    continue;
                }
                Log::print(Log::LT_Notice, "Mining timeout of %ds reached, exiting...", global::opt_timeLimit);
                
                // exit
                deviceCtx.onDestroy();
                tq_freeze(mythr->q);
                return NULL;
            }
        }
        // init time
        if (firstwork_time == 0)
            firstwork_time = time(NULL);
        gwork_restart[thr_id].restart = 0;
        hashes_done = 0;
        m_start = std::chrono::steady_clock::now();

//-----------------------------------------------------------------------------
        // Scan for nonce
        uint32_t* pdata = workInfo.data;
        uint32_t* ptarget = workInfo.target;
        const uint32_t Htarg = ptarget[7];
        const uint32_t offsetN = (maxRuns * clDevice.workSize)*thr_id;
        const uint32_t first_nonce = offsetN + (numRuns * clDevice.workSize);
        uint32_t nonce = first_nonce;

        //-------------------------------------
        // compute a midstate
        if (!numRuns)
        {
            lycl::KernelData kernelData;
            memset(&kernelData, 0, sizeof(kernelData));
            
            uint32_t h[8] =
            {
                0x6A09E667, 0xBB67AE85,
                0x3C6EF372, 0xA54FF53A,
                0x510E527F, 0x9B05688C,
                0x1F83D9AB, 0x5BE0CD19
            };
            
            kernelData.in16 = pdata[16];
            kernelData.in17 = pdata[17];
            kernelData.in18 = pdata[18];

            blake256_compress(h, pdata);

            kernelData.uH0 = h[0];
            kernelData.uH1 = h[1];
            kernelData.uH2 = h[2];
            kernelData.uH3 = h[3];
            kernelData.uH4 = h[4];
            kernelData.uH5 = h[5];
            kernelData.uH6 = h[6];
            kernelData.uH7 = h[7];
            kernelData.htArg = Htarg;
            //Log::print(Log::LT_Notice, "Device:%d block:%u,%u,%u,%u,%u,%u,%u,%u", thr_id, h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
            //-------------------------------------
            // upload data
            deviceCtx.setKernelData(kernelData);
        }

        bool nonceFound = false;
        bool isMultiNonce = false; // numNonces > 1

        uint32_t numPotentialNonces = 0;
        uint32_t singleNonce;
        do
        {
            deviceCtx.onRun(nonce, clDevice.workSize);

            // assume only 1 potential nonce was found.
            deviceCtx.getHtArgTestResultAndSize(singleNonce, numPotentialNonces);

            // check if nonce was found
            if (numPotentialNonces != 0)
            {
                //Log::print(Log::LT_Notice, "Num potential nonces found: %u", numPotentialNonces);
                lycl::uint32x8 clhash;
                deviceCtx.getLatestHashResultForIndex(singleNonce, clhash);
                // compute bmw hash
                lycl::uint32x8 lhash;
                lycl::bmwHash(clhash, lhash);

                if (fulltestU32x8(lhash, ptarget))
                {
                    // add nonce local offset
                    singleNonce += nonce;
                    work_set_target_ratio(&workInfo, &lhash.h[0]);
                    nonceFound = true;
                    if (numPotentialNonces == 1)
                    {
                        ++numRuns; // run completed
                        break;
                    }
                    else
                        m_nonces.push_back(singleNonce);
                }

                // continue with remaining potential nonces if they were found
                if (numPotentialNonces > 1)
                {
                    size_t numRemainingNonces = numPotentialNonces-1; // skip first nonce
                    deviceCtx.getHtArgTestResults(m_potentialNonces, numRemainingNonces, 2);
                    
                    for (size_t g = 0; g < numRemainingNonces; ++g)
                    {
                        deviceCtx.getLatestHashResultForIndex(m_potentialNonces[g], clhash);
                        // compute bmw hash
                        lycl::bmwHash(clhash, lhash);

                        if (fulltestU32x8(lhash, ptarget))
                        {
                            isMultiNonce = true;
                            // add nonce local offset
                            m_nonces.push_back(m_potentialNonces[g] + nonce); 

                            work_set_target_ratio(&workInfo, &lhash.h[0]);
                        }
                    }
                }

                if (isMultiNonce)
                {
                    ++numRuns; // run completed
                    break;
                }

                // all nonces are invalid.
                // clear result to prevent duplicate shares
                deviceCtx.clearResult(numPotentialNonces);
            }

            // prepare for the next run
            nonce += clDevice.workSize;
            ++numRuns;

        } while (numRuns < maxRuns && !gwork_restart[thr_id].restart);

        hashes_done = uint64_t(offsetN + (numRuns * clDevice.workSize)) - uint64_t(first_nonce);

        //-----------------------------------------------------------------------------

        // record scanhash elapsed time
        m_end = std::chrono::steady_clock::now();
        auto diff = m_end - m_start;
        double elapsedTimeMs = std::chrono::duration<double, std::milli>(diff).count();
        if (elapsedTimeMs)
        {
            pthread_mutex_lock( &stats_lock );
            thr_hashcount[thr_id] = hashes_done;
            thr_hashrates[thr_id] = hashes_done / (elapsedTimeMs * 0.001);
            pthread_mutex_unlock( &stats_lock );
        }

        // if nonce(s) found submit work 
        if (nonceFound)
        {
            if (!isMultiNonce) // nonces == 1
            {
                pdata[19] = singleNonce;
                if ( !submit_work( mythr, &workInfo ) )
                {
                    Log::print(Log::LT_Warning, "Failed to submit share.");
                    break;
                }
                else
                    Log::print(Log::LT_Notice, "Share submitted.");

                // clear result to prevent duplicate shares
                deviceCtx.clearResult(1);
            }
            else
            {
                for (size_t i = 0; i < m_nonces.size(); ++i)
                {
                    pdata[19] = m_nonces[i];
                    if ( !submit_work( mythr, &workInfo ) )
                    {
                        Log::print(Log::LT_Warning, "Failed to submit share.");
                        break;
                    }
                    else
                        Log::print(Log::LT_Notice, "Share submitted.");
                }
                // no longer needed.
                m_nonces.clear();
                // clear result to prevent duplicate shares
                deviceCtx.clearResult(numPotentialNonces);
            }
        }

        // display hashrate
        char hc[16];
        char hr[16];
        char hc_units[2] = {0,0};
        char hr_units[2] = {0,0};
        double hashcount = thr_hashcount[thr_id];
        double hashrate  = thr_hashrates[thr_id];
        if ( hashcount )
        {
            scale_hash_for_display( &hashcount, hc_units );
            scale_hash_for_display( &hashrate,  hr_units );
            if ( hc_units[0] )
                sprintf( hc, "%.2f", hashcount );
            else // no fractions of a hash
                sprintf( hc, "%.0f", hashcount );
            sprintf( hr, "%.2f", hashrate );
            Log::print( Log::LT_Info, "Device #%d: %s %sH, %s %sH/s", thr_id, hc, hc_units, hr, hr_units );
        }
    }  // worker_thread loop

    deviceCtx.onDestroy();

    tq_freeze(mythr->q);
    return NULL;
}

//-----------------------------------------------------------------------------
// Lyra2REv3 worker.
//-----------------------------------------------------------------------------
void* workerThread_lyra2REv3( void *userdata )
{
    thr_info *mythr = (thr_info *) userdata;
    int thr_id = mythr->id;

    // Init device context.
    lycl::AppLyra2REv3 deviceCtx;
    lycl::device clDevice = mythr->clDevice;
    if (!deviceCtx.onInit(mythr->clDevice))
    {
        Log::print(Log::LT_Error, "Failed to initialize device(%d)! Skipping...", thr_id);
        // exit
        deviceCtx.onDestroy();
        tq_freeze(mythr->q);
        return NULL;
    }

    work workInfo;
    memset(&workInfo, 0, sizeof(work));
    time_t firstwork_time = 0;

    std::chrono::steady_clock::time_point m_start;
    std::chrono::steady_clock::time_point m_end;

    //-------------------------------------
    // compute max runs
    // 4294967295 max nonce
    const uint64_t numNonces = 4294967296ULL / (uint64_t)global::numWorkerThreads;
    uint32_t maxRuns = (uint32_t)(numNonces / (uint32_t)clDevice.workSize);
    // last device
    if (thr_id == (global::numWorkerThreads-1))
        maxRuns += (uint32_t)(numNonces % global::numWorkerThreads);

    // Host side validation
    //std::vector<lycl::lyraHash> m_hashes(clDevice.workSize);
    std::vector<uint32_t> m_potentialNonces;
    std::vector<uint32_t> m_nonces;

    Log::print(Log::LT_Debug, "Device: %d max runs: %u", thr_id, maxRuns);

    uint32_t numRuns = 0;

    for (;;)
    {
        uint64_t hashes_done;

        //-------------------------------------
        // wait for diff
        while(time(NULL) >= g_work_time + 120)
            sleep(1);

        pthread_mutex_lock( &g_work_lock );
        //-------------------------------------
        // get new work from stratum.
        if (numRuns >= maxRuns)
        {
            // generate new work
            stratumGenWork(&stratum, &global::g_work);
        }
        //-------------------------------------
        // setup a nonce range for each worker thread
        const int32_t workCmpSize = WorkCmpSize;
        if ( memcmp( workInfo.data, global::g_work.data, workCmpSize)
             && (stratum.job.clean || (numRuns >= maxRuns) || (workInfo.job_id != global::g_work.job_id)) )
        {
            Log::print(Log::LT_Debug, "Device: %d has completed its nonce range", thr_id);

            // get new work
            workFree(&workInfo );
            workCopy(&workInfo, &global::g_work);
            // reset run counter
            numRuns = 0;
        }

        pthread_mutex_unlock( &g_work_lock );
        //-------------------------------------
        // if work is not available
        if (!workInfo.data[0])
        {
            sleep(1);
            continue;
        }
        //-------------------------------------
        // time limit
        if ( global::opt_timeLimit && firstwork_time )
        {
            int passed = (int)( time(NULL) - firstwork_time );
            int remain = (int)( global::opt_timeLimit - passed );
            if ( remain < 0 )
            {
                if ( thr_id != 0 )
                {
                    sleep(1);
                    continue;
                }
                Log::print(Log::LT_Notice, "Mining timeout of %ds reached, exiting...", global::opt_timeLimit);
                
                // exit
                deviceCtx.onDestroy();
                tq_freeze(mythr->q);
                return NULL;
            }
        }
        // init time
        if (firstwork_time == 0)
            firstwork_time = time(NULL);
        gwork_restart[thr_id].restart = 0;
        hashes_done = 0;
        m_start = std::chrono::steady_clock::now();

//-----------------------------------------------------------------------------
        // Scan for nonce
        uint32_t* pdata = workInfo.data;
        uint32_t* ptarget = workInfo.target;
        const uint32_t Htarg = ptarget[7];
        const uint32_t offsetN = (maxRuns * clDevice.workSize)*thr_id;
        const uint32_t first_nonce = offsetN + (numRuns * clDevice.workSize);
        uint32_t nonce = first_nonce;

        //-------------------------------------
        // compute a midstate
        if (!numRuns)
        {
            lycl::KernelData kernelData;
            memset(&kernelData, 0, sizeof(kernelData));
            
            uint32_t h[8] =
            {
                0x6A09E667, 0xBB67AE85,
                0x3C6EF372, 0xA54FF53A,
                0x510E527F, 0x9B05688C,
                0x1F83D9AB, 0x5BE0CD19
            };
            
            kernelData.in16 = pdata[16];
            kernelData.in17 = pdata[17];
            kernelData.in18 = pdata[18];

            blake256_compress(h, pdata);

            kernelData.uH0 = h[0];
            kernelData.uH1 = h[1];
            kernelData.uH2 = h[2];
            kernelData.uH3 = h[3];
            kernelData.uH4 = h[4];
            kernelData.uH5 = h[5];
            kernelData.uH6 = h[6];
            kernelData.uH7 = h[7];
            kernelData.htArg = Htarg;
            //Log::print(Log::LT_Notice, "Device:%d block:%u,%u,%u,%u,%u,%u,%u,%u", thr_id, h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
            //-------------------------------------
            // upload data
            deviceCtx.setKernelData(kernelData);
        }

        bool nonceFound = false;
        bool isMultiNonce = false; // numNonces > 1

        uint32_t numPotentialNonces = 0;
        uint32_t singleNonce;
        do
        {
            deviceCtx.onRun(nonce, clDevice.workSize);

            // assume only 1 potential nonce was found.
            deviceCtx.getHtArgTestResultAndSize(singleNonce, numPotentialNonces);

            // check if nonce was found
            if (numPotentialNonces != 0)
            {
                //Log::print(Log::LT_Notice, "Num potential nonces found: %u", numPotentialNonces);
                lycl::uint32x8 clhash;
                deviceCtx.getLatestHashResultForIndex(singleNonce, clhash);
                // compute bmw hash
                lycl::uint32x8 lhash;
                lycl::bmwHash(clhash, lhash);

                if (fulltestU32x8(lhash, ptarget))
                {
                    // add nonce local offset
                    singleNonce += nonce;
                    work_set_target_ratio(&workInfo, &lhash.h[0]);
                    nonceFound = true;
                    if (numPotentialNonces == 1)
                    {
                        ++numRuns; // run completed
                        break;
                    }
                    else
                        m_nonces.push_back(singleNonce);
                }

                // continue with remaining potential nonces if they were found
                if (numPotentialNonces > 1)
                {
                    size_t numRemainingNonces = numPotentialNonces-1; // skip first nonce
                    deviceCtx.getHtArgTestResults(m_potentialNonces, numRemainingNonces, 2);
                    
                    for (size_t g = 0; g < numRemainingNonces; ++g)
                    {
                        deviceCtx.getLatestHashResultForIndex(m_potentialNonces[g], clhash);
                        // compute bmw hash
                        lycl::bmwHash(clhash, lhash);

                        if (fulltestU32x8(lhash, ptarget))
                        {
                            isMultiNonce = true;
                            // add nonce local offset
                            m_nonces.push_back(m_potentialNonces[g] + nonce); 

                            work_set_target_ratio(&workInfo, &lhash.h[0]);
                        }
                    }
                }

                if (isMultiNonce)
                {
                    ++numRuns; // run completed
                    break;
                }

                // all nonces are invalid.
                // clear result to prevent duplicate shares
                deviceCtx.clearResult(numPotentialNonces);
            }

            // prepare for the next run
            nonce += clDevice.workSize;
            ++numRuns;

        } while (numRuns < maxRuns && !gwork_restart[thr_id].restart);

        hashes_done = uint64_t(offsetN + (numRuns * clDevice.workSize)) - uint64_t(first_nonce);

        //-----------------------------------------------------------------------------

        // record scanhash elapsed time
        m_end = std::chrono::steady_clock::now();
        auto diff = m_end - m_start;
        double elapsedTimeMs = std::chrono::duration<double, std::milli>(diff).count();
        if (elapsedTimeMs)
        {
            pthread_mutex_lock( &stats_lock );
            thr_hashcount[thr_id] = hashes_done;
            thr_hashrates[thr_id] = hashes_done / (elapsedTimeMs * 0.001);
            pthread_mutex_unlock( &stats_lock );
        }

        // if nonce(s) found submit work 
        if (nonceFound)
        {
            if (!isMultiNonce) // nonces == 1
            {
                pdata[19] = singleNonce;
                if ( !submit_work( mythr, &workInfo ) )
                {
                    Log::print(Log::LT_Warning, "Failed to submit share.");
                    break;
                }
                else
                    Log::print(Log::LT_Notice, "Share submitted.");

                // clear result to prevent duplicate shares
                deviceCtx.clearResult(1);
            }
            else
            {
                for (size_t i = 0; i < m_nonces.size(); ++i)
                {
                    pdata[19] = m_nonces[i];
                    if ( !submit_work( mythr, &workInfo ) )
                    {
                        Log::print(Log::LT_Warning, "Failed to submit share.");
                        break;
                    }
                    else
                        Log::print(Log::LT_Notice, "Share submitted.");
                }
                // no longer needed.
                m_nonces.clear();
                // clear result to prevent duplicate shares
                deviceCtx.clearResult(numPotentialNonces);
            }
        }

        // display hashrate
        char hc[16];
        char hr[16];
        char hc_units[2] = {0,0};
        char hr_units[2] = {0,0};
        double hashcount = thr_hashcount[thr_id];
        double hashrate  = thr_hashrates[thr_id];
        if ( hashcount )
        {
            scale_hash_for_display( &hashcount, hc_units );
            scale_hash_for_display( &hashrate,  hr_units );
            if ( hc_units[0] )
                sprintf( hc, "%.2f", hashcount );
            else // no fractions of a hash
                sprintf( hc, "%.0f", hashcount );
            sprintf( hr, "%.2f", hashrate );
            Log::print( Log::LT_Info, "Device #%d: %s %sH, %s %sH/s", thr_id, hc, hc_units, hr, hr_units );
        }
    }  // worker_thread loop

    deviceCtx.onDestroy();

    tq_freeze(mythr->q);
    return NULL;
}



int main(int argc, char** argv)
{
    Log::print(Log::LT_Notice, "*** lyclMiner beta %s. ***", PACKAGE_VERSION);
    Log::print(Log::LT_Notice, "Developer: CryptoGraphics.\n");

    //-----------------------------------------------------------------------------
    // Config file management
    lycl::ConfigFile cf;
    if (argc == 2)
    {
        if (!cf.setSource(argv[1], true))
        {
            Log::print(Log::LT_Error, "Failed to load a config file. (%s)", argv[1]);
            return 1;
        }
        else
            Log::print(Log::LT_Debug, "Loaded a config file (%s) successfully", argv[1]);
    }
    else if (argc == 1) // try to load lyclMiner.conf by default.
    {
        if (!cf.setSource("lyclMiner.conf", true))
        {
            Log::print(Log::LT_Error, "Failed to load a config file. (lyclMiner.conf)");
            return 1;
        }
        else
            Log::print(Log::LT_Debug, "Loaded a config file (lyclMiner.conf) successfully");
    }

    lycl::ConfigSetting* csetting = nullptr;
    // get global settings
    csetting = cf.getSetting("Global", "TerminalColors");
    if (csetting) global::use_colors = csetting->AsBool;
    csetting = cf.getSetting("Global", "ExtraNonce");
    if (csetting) global::opt_extranonce = csetting->AsBool;


    cl_int errorCode = CL_SUCCESS;
    //-----------------------------------------------------------------------------
    // get platform IDs
    cl_uint numPlatformIDs = 0;
    errorCode = clGetPlatformIDs(0, nullptr, &numPlatformIDs);
    if (errorCode != CL_SUCCESS || numPlatformIDs <= 0)
    {
        Log::print(Log::LT_Error, "Failed to find any OpenCL platforms.");
        return 1;
    }
    std::vector<cl_platform_id> platformIds(numPlatformIDs);
    clGetPlatformIDs(numPlatformIDs, platformIds.data(), nullptr);
    //-----------------------------------------------------------------------------
    // get logical device list
    // tested on AMDCL2(Windows) and ROCm.
    const std::string platformVendorAMD("Advanced Micro Devices");
    // logical device list sorted by PCIe bus ID
    std::vector<lycl::device> logicalDevices;
    for (size_t i = 0; i < (size_t)numPlatformIDs; ++i)
    {
        // check if platform is supported (currently AMD only)
        size_t infoSize = 0;
        clGetPlatformInfo(platformIds[i], CL_PLATFORM_VENDOR, 0, nullptr, &infoSize);
        std::string infoString(infoSize, ' ');
        clGetPlatformInfo(platformIds[i], CL_PLATFORM_VENDOR, infoSize, (void*)infoString.data(), nullptr);
        if (infoString.find(platformVendorAMD) == std::string::npos)
        {
            Log::print(Log::LT_Warning, "Unsupported platform vendor: %s (id:%u)", infoString.c_str(), i);
            continue;
        }
        /*else
        {
            // print an extension list
            clGetPlatformInfo(platformIds[i], CL_PLATFORM_EXTENSIONS, 0, nullptr, &infoSize);
            infoString.resize(infoSize);
            clGetPlatformInfo(platformIds[i], CL_PLATFORM_EXTENSIONS, infoSize, (void*)infoString.data(), nullptr);
            std::cout << "Platform extensions: " << infoString << std::endl;
        }*/

        // get devices available on this platform
        cl_uint numDeviceIDs = 0;
        errorCode = clGetDeviceIDs(platformIds[i], CL_DEVICE_TYPE_GPU, 0, nullptr, &numDeviceIDs);
        if (errorCode != CL_SUCCESS || numPlatformIDs <= 0)
        {
            Log::print(Log::LT_Warning, "No devices available on platform id:%u", i);
            continue;
        }

        std::vector<cl_device_id> deviceIds(numDeviceIDs);
        clGetDeviceIDs(platformIds[i], CL_DEVICE_TYPE_GPU, numDeviceIDs, deviceIds.data(), nullptr);

        cl_device_topology_amd topology;
        for (size_t j = 0; j < deviceIds.size(); ++j)
        {
            lycl::device clDevice;
            clDevice.clPlatformId = platformIds[i];
            clDevice.clId = deviceIds[j];
            clDevice.platformIndex = (int32_t)i;
            clDevice.binaryFormat = lycl::BF_None;
            clDevice.asmProgram = lycl::AP_None;
            clDevice.workSize = global::defaultWorkSize;
        
            cl_int status = clGetDeviceInfo(deviceIds[j], CL_DEVICE_TOPOLOGY_AMD, 
                                            sizeof(cl_device_topology_amd), &topology, nullptr);
            if(status == CL_SUCCESS)
            {
                if (topology.raw.type == CL_DEVICE_TOPOLOGY_TYPE_PCIE_AMD)
                    clDevice.pcieBusId = (int32_t)topology.pcie.bus;
            }
            else
            {
                Log::print(Log::LT_Warning, "Failed to get CL_DEVICE_TOPOLOGY_AMD info. Platform index: %u", i);
                clDevice.pcieBusId = 0;
            }
                
            logicalDevices.push_back(clDevice);
        }
    }

    if (!logicalDevices.size())
    {
        Log::print(Log::LT_Error, "Failed to find any supported devices.");
        return 1;
    }
    //-----------------------------------------------------------------------------
    // sort device list based on pcieBusID -> platformID
    std::sort(logicalDevices.begin(), logicalDevices.end(), lycl::compareLogicalDevices);

    //-----------------------------------------------------------------------------
    // generate a config file if it was requested
    bool generateConfig = false;
    bool rawDeviceList = false;
    if (argc >= 2)
    {
        std::string command(argv[1]);
        if (command.compare("-g") == 0) 
            generateConfig = true;
        if (command.compare("-gr") == 0)
        {
            rawDeviceList = true;
            generateConfig = true;
        }
    }

    // check for duplicate PCIeBusIds(rare case), only if raw device list is disabled
    if (!rawDeviceList)
    {
        int32_t prevPlatformIndex = -1;
        int32_t prevPcieBusId = -1;
        for (size_t i = 0; i < logicalDevices.size(); ++i)
        {
            if((logicalDevices[i].pcieBusId == prevPcieBusId) &&
               (logicalDevices[i].platformIndex == prevPlatformIndex))
            {
                if (generateConfig)
                {
                    Log::print(Log::LT_Warning, "Failed to group logical devices by PCIe slot ID. Duplicates Found!");
                    Log::print(Log::LT_Warning, "Configuration file will be generated in \"raw device list\" format.");
                    Log::print(Log::LT_Warning, "All possible device/platform combinations will be listed.");
                }
                rawDeviceList = true;
                break;
            }
            else
            {
                prevPlatformIndex = logicalDevices[i].platformIndex;
                prevPcieBusId = logicalDevices[i].pcieBusId;
            }
        }
    }

    if (generateConfig)
    {
        std::string confFileName;
        if (argc > 2)
            confFileName.append(argv[2]);
        else
            confFileName = "lyclMiner.conf";

        std::string configText("#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n"
                               "# Global settings:\n"
                               "#\n"
                               "#    TerminalColors\n"
                               "#        Enable colors for logging. Windows Command Prompt is not supported.\n"
                               "#        Default: false\n"
                               "#\n"
                               "#    ExtraNonce\n"
                               "#        Enable extranonce subscription.\n"
                               "#        Default: true\n"
                               "#\n"
                               "#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n"
                               "\n"
                               "<Global TerminalColors = \"false\"\n"
                               "        ExtraNonce = \"true\">\n"
                               "\n"
                               "#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n"
                               "# Pool connection setup:\n"
                               "#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n"
                               "\n"
                               "<Connection Url = \"stratum+tcp://example.com:port\"\n"
                               "            Username = \"user\"\n"
                               "            Password = \"x\"\n"
                               "            Algorithm = \"Lyra2REv3\">\n"
                               "\n"
                               "#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n"
                               "# Device config:\n"
                               "#\n"
                               "# Available platforms:\n"
                               "#\n");

        // generate platform list
        std::string platformListText;
        size_t infoSize = 0;
        std::string infoString(infoSize, ' ');
        for (size_t i = 0; i < (size_t)numPlatformIDs; ++i)
        {
            infoString.clear();
            clGetPlatformInfo(platformIds[i], CL_PLATFORM_VENDOR, 0, nullptr, &infoSize);
            //infoString.resize(infoSize-1);
            infoString.resize(infoSize);
            clGetPlatformInfo(platformIds[i], CL_PLATFORM_VENDOR, infoSize, (void*)infoString.data(), nullptr);
            infoString.pop_back();

            platformListText += "# ";
            platformListText += std::to_string(i+1);
            platformListText += ". Platform name: ";
            platformListText += infoString;
            platformListText += "\n#    Index: ";
            platformListText += std::to_string(i);
            platformListText += "\n";
        }

        const std::string defaultWorkSizeString(std::to_string(global::defaultWorkSize));
        std::string deviceConfText;
        std::string deviceListText;
        std::string deviceName;
        std::string asmProgramName;
        std::string deviceBoardName;
        if(!rawDeviceList)
        {
            int32_t prevPcieBusID = -1;
            const std::string availablePlatformIndices("\n#    Available platforms indices: ");
            std::string pcieBusIdString;
            std::string platformIndexString;
            // config texts
            size_t deviceIndex = 0;
            for (size_t i = 0; i < logicalDevices.size(); ++i)
            {
                if (logicalDevices[i].pcieBusId != prevPcieBusID)
                {
                    prevPcieBusID = logicalDevices[i].pcieBusId;

                    // info
                    deviceListText += "\n#\n# ";
                    deviceListText += std::to_string(deviceIndex+1);
                    deviceListText +=". ";
                    deviceListText += "Device: ";
                
                    // get device name
                    size_t infoSize = 0;
                    deviceName.clear();
                    clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_NAME, 0, NULL, &infoSize);
                    //deviceName.resize(infoSize - 1);
                    deviceName.resize(infoSize);
                    clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_NAME, infoSize, (void *)deviceName.data(), NULL);
                    deviceName.pop_back();

                    deviceListText += deviceName;
                    // get device board name
                    deviceBoardName.clear();
                    clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_BOARD_NAME_AMD, 0, NULL, &infoSize);
                    //deviceBoardName.resize(infoSize - 1);
                    deviceBoardName.resize(infoSize);
                    clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_BOARD_NAME_AMD, infoSize, (void *)deviceBoardName.data(), NULL);
                    deviceBoardName.pop_back();
                
                    deviceListText += "\n#    Board name: ";
                    deviceListText += deviceBoardName;                

                    deviceListText += "\n#    PCIe bus id: ";
                    pcieBusIdString = std::to_string(prevPcieBusID);
                    deviceListText += pcieBusIdString;
                    deviceListText += availablePlatformIndices;
                    platformIndexString = std::to_string(logicalDevices[i].platformIndex);
                    deviceListText += platformIndexString;
                    // config
                    deviceConfText += "<Device";
                    deviceConfText += std::to_string(deviceIndex);
                    ++deviceIndex;

                    deviceConfText += " PCIeBusId = \"";
                    deviceConfText += pcieBusIdString;
                    deviceConfText += "\"";

                    deviceConfText += " PlatformIndex = \"";
                    deviceConfText += platformIndexString;
                    deviceConfText += "\"";

                    deviceConfText += " BinaryFormat = \"";
                    deviceConfText += "amdcl2";
                    deviceConfText += "\"";

                    deviceConfText += " AsmProgram = \"";
                    lycl::getAsmProgramNameFromDeviceName(deviceName, asmProgramName);
                    deviceConfText += asmProgramName;
                    deviceConfText += "\"";
                
                    deviceConfText += " WorkSize = \"";
                    deviceConfText += defaultWorkSizeString;
                    deviceConfText += "\">\n";
                }
                else
                {
                    // if it is the same device, then it means a different platform.
                    deviceListText += ", " + std::to_string(logicalDevices[i].platformIndex); 
                }
            }
        }
        else // generate raw device list
        {
            for (size_t i = 0; i < logicalDevices.size(); ++i)
            {
                // get device info
                deviceListText += "\n#\n# DeviceIndex: ";
                deviceListText += std::to_string(i);

                deviceListText += "\n#    Name: ";
                // get device name
                size_t infoSize = 0;
                deviceName.clear();
                clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_NAME, 0, NULL, &infoSize);
                //deviceName.resize(infoSize - 1);
                deviceName.resize(infoSize);
                clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_NAME, infoSize, (void *)deviceName.data(), NULL);
                deviceName.pop_back();

                deviceListText += deviceName;
                // get device board name
                deviceBoardName.clear();
                clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_BOARD_NAME_AMD, 0, NULL, &infoSize);
                //deviceBoardName.resize(infoSize - 1);
                deviceBoardName.resize(infoSize);
                clGetDeviceInfo(logicalDevices[i].clId, CL_DEVICE_BOARD_NAME_AMD, infoSize, (void *)deviceBoardName.data(), NULL);
                deviceBoardName.pop_back();
            
                deviceListText += "\n#    Board name: ";
                deviceListText += deviceBoardName;                

                deviceListText += "\n#    PCIe bus id: ";
                deviceListText += std::to_string(logicalDevices[i].pcieBusId);

                deviceListText += "\n#    Platform index: ";
                deviceListText += std::to_string(logicalDevices[i].platformIndex);

                //  get device config
                deviceConfText += "<Device";
                deviceConfText += std::to_string(i);

                deviceConfText += " DeviceIndex = \"";
                deviceConfText += std::to_string(i);
                deviceConfText += "\"";

                deviceConfText += " BinaryFormat = \"";
                deviceConfText += "amdcl2";
                deviceConfText += "\"";

                deviceConfText += " AsmProgram = \"";
                lycl::getAsmProgramNameFromDeviceName(deviceName, asmProgramName);
                deviceConfText += asmProgramName;
                deviceConfText += "\"";
                
                deviceConfText += " WorkSize = \"";
                deviceConfText += defaultWorkSizeString;
                deviceConfText += "\">\n";
            }
        }

        // final composition
        configText += platformListText;
        configText += "#\n# Available devices:";
        configText += deviceListText;
        configText += "\n#\n#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#\n\n";
        configText += deviceConfText;

        // save a config file.
        if (lycl::utils::fileExists(confFileName.c_str())) 
        {
            Log::print(Log::LT_Error, "Failed to create a config file. %s already exists.", confFileName.c_str());
            return 1;
        }
        std::ofstream cfgOutput(confFileName.c_str());
        cfgOutput << configText;
        cfgOutput.close();

        Log::print(Log::LT_Notice, "Config file: (%s) has been generated.", confFileName.c_str());

        return 0;
    }

    //-------------------------------------
    // setup a pool connection.
    // get url
    csetting = cf.getSetting("Connection", "Url");
    if (csetting) global::connectionInfo.rpc_url = csetting->AsString;
    else { Log::print(Log::LT_Error, "Failed to get an \"Url\" option inside \"Connection\" section"); return 1; }
    // get username
    csetting = cf.getSetting("Connection", "Username");
    if (csetting) global::connectionInfo.rpc_user = csetting->AsString;
    else { Log::print(Log::LT_Error, "Failed to get a \"Username\" option inside \"Connection\" section"); return 1; }
    // get password
    csetting = cf.getSetting("Connection", "Password");
    if (csetting) global::connectionInfo.rpc_pass = csetting->AsString;
    else { Log::print(Log::LT_Error, "Failed to get a \"Password\" option inside \"Connection\" section"); return 1; }
    // rpc user:pass
    global::connectionInfo.rpc_userpass = global::connectionInfo.rpc_user + ":" + global::connectionInfo.rpc_pass;

    // get algorithm
    csetting = cf.getSetting("Connection", "Algorithm");
    if (csetting) global::connectionInfo.algo = lycl::getAlgorithmFromName(csetting->AsString);
    else { Log::print(Log::LT_Error, "Failed to get an \"Algorithm\" option inside \"Connection\" section"); return 1; }

    // validate algorithm
    std::string algoName = csetting->AsString;
    if (global::connectionInfo.algo == lycl::A_None) 
    {
        Log::print(Log::LT_Error, "\"Algorithm\" parameter is not set or incorrect(%s).", csetting->AsString.c_str());
        return 1;
    }


    //-------------------------------------
    // setup devices
    size_t deviceBlockIndex = 0;
    std::string dBlockName("Device");
    std::string binaryFormatName;
    std::string asmProgramName;
    std::string deviceBlock = dBlockName + std::to_string(deviceBlockIndex);
    
    std::vector<lycl::device> configuredDevices;

    // check if configuration file is in "raw device list" format
    csetting = cf.getSetting(deviceBlock.c_str(), "PCIeBusId");
    if (!csetting)
        rawDeviceList = true;

    // PCIeBusId is the only required setting, others are optional
    if (!rawDeviceList)
    {
        while ( (csetting = cf.getSetting(deviceBlock.c_str(), "PCIeBusId")) )
        {
            int pcieBusId = csetting->AsInt;
            int platformIndex = -1;
            int workSize = 0;
            lycl::EBinaryFormat binaryFormat = lycl::BF_None;
            lycl::EAsmProgram asmProgram = lycl::AP_None; 

            // get platform index
            csetting = cf.getSetting(deviceBlock.c_str(), "PlatformIndex"); 
            if (csetting) platformIndex = csetting->AsInt;

            // get program binary format
            csetting = cf.getSetting(deviceBlock.c_str(), "BinaryFormat"); 
            if (csetting)
            {
                binaryFormatName = csetting->AsString;
                binaryFormat = lycl::getBinaryFormatFromName(binaryFormatName);
            }

            // get asm program name
            csetting = cf.getSetting(deviceBlock.c_str(), "AsmProgram"); 
            if (csetting)
            {
                asmProgramName = csetting->AsString;
                asmProgram = lycl::getAsmProgramName(asmProgramName);
            }

            // get work size flag
            csetting = cf.getSetting(deviceBlock.c_str(), "WorkSize"); 
            if (csetting) workSize = csetting->AsInt;

            // check if pcieBusID and platfromIndex are correct
            ptrdiff_t foundPCIeBusId = -1;
            ptrdiff_t foundPlatformIndex = -1;
            for (size_t i = 0; i < logicalDevices.size(); ++i)
            {
                lycl::device& dv = logicalDevices[i];
                if (dv.pcieBusId == pcieBusId) 
                {
                    foundPCIeBusId = i;
                    if (dv.platformIndex == platformIndex) 
                    {
                        foundPlatformIndex = i;
                        break;
                    }
                }
                // check all logical devices in case they were sorted wrong...
            }

            // add a configured device if it was found
            if (foundPCIeBusId >= 0)
            {
                if (foundPlatformIndex >= 0)
                    configuredDevices.push_back(logicalDevices[foundPlatformIndex]);
                else
                {
                    // unlike pcieBusId, platform index may not be static.
                    Log::print(Log::LT_Warning, "\"PlatformIndex\" parameter is not set or incorrect inside \"%s\" section. Using default(%d).",
                               deviceBlock.c_str(), logicalDevices[foundPCIeBusId].platformIndex);
                    configuredDevices.push_back(logicalDevices[foundPCIeBusId]);
                }

                // check if workSize is set correct.
                // TODO: review for other vendors/drivers
                if ( (workSize > 0) && ((workSize % 256) == 0) )
                    configuredDevices[configuredDevices.size() - 1].workSize = workSize;
                else
                {
                    Log::print(Log::LT_Warning, "\"WorkSize\" parameter is not set or incorrect inside \"%s\" section. It must be multiple of 256. Using default(%d).",
                               deviceBlock.c_str(), logicalDevices[configuredDevices.size()- 1].workSize); 
                }

                // AsmProgram will be detected on context init
                configuredDevices[configuredDevices.size() - 1].binaryFormat = binaryFormat;
                configuredDevices[configuredDevices.size() - 1].asmProgram = asmProgram;
            }
            else
                Log::print(Log::LT_Warning, "\"PCIeBusId\" is invalid inside \"%s\" section. Skipping device...", deviceBlock.c_str());
            

            ++deviceBlockIndex;
            deviceBlock = dBlockName + std::to_string(deviceBlockIndex);
        }
    }
    else
    {
        while( (csetting = cf.getSetting(deviceBlock.c_str(), "DeviceIndex")) )
        {
            size_t deviceIndex = (size_t)csetting->AsInt;
            int workSize = 0;
            lycl::EBinaryFormat binaryFormat = lycl::BF_None;
            lycl::EAsmProgram asmProgram = lycl::AP_None; 

            // get program binary format
            csetting = cf.getSetting(deviceBlock.c_str(), "BinaryFormat"); 
            if (csetting)
            {
                binaryFormatName = csetting->AsString;
                binaryFormat = lycl::getBinaryFormatFromName(binaryFormatName);
            }

            // get asm program name
            csetting = cf.getSetting(deviceBlock.c_str(), "AsmProgram"); 
            if (csetting)
            {
                asmProgramName = csetting->AsString;
                asmProgram = lycl::getAsmProgramName(asmProgramName);
            }

            // get work size flag
            csetting = cf.getSetting(deviceBlock.c_str(), "WorkSize"); 
            if (csetting) workSize = csetting->AsInt;

            // check if pcieBusID and platfromIndex are correct
            if ((deviceIndex < logicalDevices.size()) && (deviceIndex >= 0))
            {
                configuredDevices.push_back(logicalDevices[deviceIndex]);

                // check if workSize is set correct.
                // TODO: review for other vendors/drivers
                if ( (workSize > 0) && ((workSize % 256) == 0) )
                    configuredDevices[configuredDevices.size() - 1].workSize = workSize;
                else
                {
                    Log::print(Log::LT_Warning, "\"WorkSize\" parameter is not set or incorrect inside \"%s\" section. It must be multiple of 256. Using default(%d).",
                               deviceBlock.c_str(), logicalDevices[configuredDevices.size()- 1].workSize); 
                }

                // AsmProgram will be detected on context init
                configuredDevices[configuredDevices.size()- 1].binaryFormat = binaryFormat;
                configuredDevices[configuredDevices.size()- 1].asmProgram = asmProgram;
            }
            else
                Log::print(Log::LT_Warning, "\"DeviceIndex\" is invalid inside \"%s\" section. Skipping device...", deviceBlock.c_str());

            ++deviceBlockIndex;
            deviceBlock = dBlockName + std::to_string(deviceBlockIndex);
        }
    }

//-----------------------------------------------------------------------------
    // Init miner

    // 1 device per thread
    global::numWorkerThreads = configuredDevices.size();
    if (!global::numWorkerThreads)
    {
        Log::print(Log::LT_Warning, "Found 0 configured devices. Exiting...");
        return 0;
    }

    pthread_mutex_init(&Log::applog_lock, NULL);
    pthread_mutex_init(&stats_lock, NULL);
    pthread_mutex_init(&g_work_lock, NULL);
    pthread_mutex_init(&stratum.sock_lock, NULL);
    pthread_mutex_init(&stratum.work_lock, NULL);

    long flags = strncmp(global::connectionInfo.rpc_url.c_str(), "https:", 6) ? (CURL_GLOBAL_ALL & ~CURL_GLOBAL_SSL) : CURL_GLOBAL_ALL;
    if (curl_global_init(flags))
    {
        Log::print(Log::LT_Error, "CURL initialization failed");
        return 1;
    }

#ifndef _WIN32
    signal(SIGINT, signal_handler);
#else
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);
#endif

    // allocate global thread objects
    gwork_restart = (struct work_restart*) calloc(global::numWorkerThreads, sizeof(*gwork_restart));
    if (!gwork_restart)
        return 1;
    gthr_info = (struct thr_info*) calloc(global::numWorkerThreads + 4, sizeof(*thr));
    if (!gthr_info)
        return 1;
    thr_hashrates = (double *) calloc(global::numWorkerThreads, sizeof(double));
    if (!thr_hashrates)
        return 1;
    thr_hashcount = (double *) calloc(global::numWorkerThreads, sizeof(double));
    if (!thr_hashcount)
        return 1;

    // Currect thread layout:
    // [workIO,stratum,Device0...DeviceN]

    //-----------------------------------------------------------------------------
    // create work I/O thread
    work_thr_id = global::numWorkerThreads;
    thr = &gthr_info[work_thr_id];
    thr->id = work_thr_id;
    thr->q = tq_new();
    if (!thr->q)
        return 1;

    if (thread_create(thr, workio_thread))
    {
        Log::print(Log::LT_Error, "work I/O thread create failed");
        return 1;
    }

    //-----------------------------------------------------------------------------
    // create stratum thread
    stratum_thr_id = global::numWorkerThreads + 1;
    thr = &gthr_info[stratum_thr_id];
    thr->id = stratum_thr_id;
    thr->q = tq_new();
    if (!thr->q)
        return 1;

    if (thread_create(thr, stratum_thread))
    {
        Log::print(Log::LT_Error, "stratum thread create failed");
        return 1;
    }

    tq_push(gthr_info[stratum_thr_id].q, strdup(global::connectionInfo.rpc_url.c_str()));

    //-----------------------------------------------------------------------------
    // create worker threads
    int numWorkerThreads = 0;
    for (int i = 0; i < global::numWorkerThreads; i++)
    {
        thr = &gthr_info[i];
        thr->id = i;
        thr->q = tq_new();
        thr->clDevice = configuredDevices[(size_t)i];
        if (!thr->q)
            return 1;

        lycl::EAlgorithm selectedAlgo = global::connectionInfo.algo;
        int thrResult = 0;
        if (selectedAlgo == lycl::A_Lyra2REv3) 
            thrResult = thread_create(thr, workerThread_lyra2REv3);
        else if (selectedAlgo == lycl::A_Lyra2REv2)
            thrResult = thread_create(thr, workerThread_lyra2REv2);
        else // should never happen
        {
            Log::print(Log::LT_Error, "worker thread %d create failed. Algorithm is not set or incorrect!", i);
            continue;
        }

        if (thrResult)
        {
            Log::print(Log::LT_Error, "worker thread %d create failed", i);
            continue;
        }

        ++numWorkerThreads;
    }

    if (numWorkerThreads > 0)
    {
        Log::print(Log::LT_Info, "%d worker threads started, using %s algorithm.",
                   numWorkerThreads, algoName.c_str());
    }
    else
    {
        Log::print(Log::LT_Error, "No workers available.");
    }

//-----------------------------------------------------------------------------

    pthread_join(gthr_info[work_thr_id].pth, NULL);

    return 0;
}
