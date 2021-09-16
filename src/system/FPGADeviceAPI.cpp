/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#ifdef USE_FPGA
#include <nanos6/fpga_device.h>
#include <libxtasks.h>

extern "C"
{

void nanos6_fpga_addArg(int index, unsigned char flags, unsigned long long value, void* taskHandle)
{
	xtasksAddArg(index, (xtasks_arg_flags) flags, (xtasks_arg_val) value, *((xtasks_task_handle*) taskHandle));
}

void nanos6_fpga_addArgs(int num, unsigned char flags, const unsigned long long* values, void* taskHandle)
{
	xtasksAddArgs(num, (xtasks_arg_flags) flags, (xtasks_arg_val*)values, *((xtasks_task_handle*) taskHandle));
}

}

#endif
