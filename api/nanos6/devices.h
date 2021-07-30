/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2018 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_DEVICES_H
#define NANOS6_DEVICES_H

#include "major.h"
#include "task-instantiation.h"
#if NANOS6_CUDA
#include "cuda_device.h"
#endif

#if NANOS6_OPENACC
#include "openacc_device.h"
#endif

#if NANOS6_OPENCL
#include "opencl_device.h"
#endif

#if NANOS6_FPGA
#include "fpga_device.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

int nanos6_get_device_num(nanos6_device_t index);

void nanos6_device_memcpy(nanos6_device_t device, int device_id, void* host_ptr, size_t size);

void nanos6_set_home(nanos6_device_t device, int device_id, void* host_ptr, size_t size);

void nanos6_set_noflush(void* host_ptr, size_t size);

void nanos6_print_directory();

void nanos6_register_device_spawned_task(uint64_t spawned_task_id, void* function_addr);

#ifdef __cplusplus
}
#endif





#endif /* NANOS6_DEVICES_H */

