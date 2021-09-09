/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#ifdef USE_FPGA
#include <nanos6/fpga_device.h>
#include <libxtasks.h>
#include <iostream>

extern "C"
void nanos6_fpga_addArg(int index, int symbolData, void* taskHandle, void* address)
{
    xtasksAddArg(index, (xtasks_arg_flags) symbolData, (uint64_t) address, *((xtasks_task_handle*) taskHandle));
}

#endif
