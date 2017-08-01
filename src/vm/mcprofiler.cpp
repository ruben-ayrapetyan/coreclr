// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#include "common.h"
#include "gcheaputilities.h"
#include "hosting.h"
#include "mcprofiler.h"
#include "threadsuspend.h"

#ifdef FEATURE_MEMORY_CONSUMPTION_SAMPLING

Volatile<BOOL> MCProfiler::s_profilingEnabled = false;
Thread* MCProfiler::s_pSamplingThread = NULL;
CLREventStatic MCProfiler::s_threadShutdownEvent;
long MCProfiler::s_samplingRateInNs = 500000000; // 0.5s

void MCProfiler::Enable()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        PRECONDITION(s_pSamplingThread == NULL);
    }
    CONTRACTL_END;

    s_profilingEnabled = true;
    s_pSamplingThread = SetupUnstartedThread();
    if(s_pSamplingThread->CreateNewThread(0, ThreadProc, NULL))
    {
        // Start the sampling thread.
        s_pSamplingThread->SetBackground(TRUE);
        s_pSamplingThread->StartThread();
    }
    else
    {
        _ASSERT(!"Unable to create sample profiler thread.");
    }
}

void MCProfiler::Disable()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Bail early if profiling is not enabled.
    if(!s_profilingEnabled)
    {
        return;
    }

    // Reset the event before shutdown.
    s_threadShutdownEvent.Reset();

    // The sampling thread will watch this value and exit
    // when profiling is disabled.
    s_profilingEnabled = false;

    // Wait for the sampling thread to clean itself up.
    s_threadShutdownEvent.Wait(0, FALSE /* bAlertable */);
}

void MCProfiler::SetSamplingRate(long nanoseconds)
{
    LIMITED_METHOD_CONTRACT;
    s_samplingRateInNs = nanoseconds;
}

extern "C" FILE *fopen(const char *path, const char *mode);
extern "C" size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern "C" int fclose(FILE *stream);
extern "C" int fprintf(FILE *stream, const char *format, ...);
extern "C" int sprintf_s(char *str, size_t, const char *format, ...);
extern "C" int getpid(void);
extern "C" int fork(void);
extern "C" int execlp(const char *file, const char *arg, ...);
extern "C" int waitpid(int pid, int *status, int options);

DWORD WINAPI MCProfiler::ThreadProc(void *args)
{
    CONTRACTL
    {
        NOTHROW;
        GC_TRIGGERS;
        MODE_COOPERATIVE;
        PRECONDITION(s_pSamplingThread != NULL);
    }
    CONTRACTL_END;

    // Complete thread initialization and start the profiling loop.
    if(s_pSamplingThread->HasStarted())
    {
        while(s_profilingEnabled)
        {
            int allocatedmanagedMemory = (int) GCHeapUtilities::GetGCHeap()->GetTotalBytesInUse();

            char path[512], cmd[1024];
            sprintf_s (path, sizeof (path), "/tmp/memory-stats-%d", getpid());
            sprintf_s (cmd, sizeof (cmd), "cat /proc/%d/smaps | grep ^Private_Dirty | awk '{ s += $2 } END { print s * 1024 }' >> %s", getpid(), path);

            FILE *f = fopen (path, "a");

            fprintf (f, "managed=%d total=", allocatedmanagedMemory);

            fclose (f);

            int c = fork();

            if (c == 0)
            {
              execlp("/bin/bash", "/bin/bash", "-c", cmd, NULL);
            }
            else
            {
              waitpid(c, NULL, 0);
            }

            // Wait until it's time to sample again.
            PAL_nanosleep(s_samplingRateInNs);
        }
    }

    // Destroy the sampling thread when it is done running.
    DestroyThread(s_pSamplingThread);
    s_pSamplingThread = NULL;

    // Signal Disable() that the thread has been destroyed.
    s_threadShutdownEvent.Set();

    return S_OK;
}

#endif // FEATURE_MEMORY_CONSUMPTION_SAMPLING
