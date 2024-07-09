/*
    This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

    Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_ACCELERATOR_INSTRUMENTATION_HPP
#define FPGA_ACCELERATOR_INSTRUMENTATION_HPP

#include <libxtasks.h>
#include <atomic>
#include <pthread.h>

class FPGAAcceleratorInstrumentation
{
public:

    struct HandleWithInfo {
        xtasks_acc_handle handle;
        xtasks_acc_info info;
        uint64_t startTimeFpga;
        uint64_t startTimeCpu;
    };

private:

    std::atomic<bool> _stopService;
    std::atomic<bool> _finishedService;
    HandleWithInfo handle;
    pthread_t serviceThread;

public:

    FPGAAcceleratorInstrumentation() :
        _stopService(false), _finishedService(false)
    {}    

    void initializeService(int cpu);
    void shutdownService();
    void serviceLoop();

    static void *serviceFunction(void *data);
    static void serviceCompleted(void *data);

    void setHandle(HandleWithInfo &handleWithInfo) {handle = handleWithInfo;}
};


#endif //FPGA_ACCELERATOR_INSTRUMENTATION_HPP
