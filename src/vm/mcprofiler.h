// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#ifndef __MCPROFILER_H__
#define __MCPROFILER_H__

#ifdef FEATURE_MEMORY_CONSUMPTION_SAMPLING

#include "common.h"

class MCProfiler
{
    public:
        // Enable profiling.
        static void Enable();

        // Disable profiling.
        static void Disable();

        // Set the sampling rate.
        static void SetSamplingRate(long nanoseconds);

    private:
        // Profiling thread proc.  Invoked on a new thread when profiling is enabled.
        static DWORD WINAPI ThreadProc(void *args);

        // True when profiling is enabled.
        static Volatile<BOOL> s_profilingEnabled;

        // The sampling thread.
        static Thread *s_pSamplingThread;

        // Thread shutdown event for synchronization between Disable() and the sampling thread.
        static CLREventStatic s_threadShutdownEvent;

        // The sampling rate.
        static long s_samplingRateInNs;
};

#endif // FEATURE_MEMORY_CONSUMPTION_SAMPLING

#endif // __MCPROFILER_H__
