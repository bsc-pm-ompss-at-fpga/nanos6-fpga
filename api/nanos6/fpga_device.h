/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_FPGA_DEVICE_H
#define NANOS6_FPGA_DEVICE_H

#include "major.h"

#pragma GCC visibility push(default)

// NOTE: The full version depends also on nanos6_major_api
// That is:   nanos6_major_api . nanos6_fpga_device_api
enum nanos6_fpga_device_api_t { nanos6_fpga_device_api = 1 };

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void*  taskHandle;
    char   taskFinished;
} nanos6_fpga_device_environment_t;

typedef enum {
	NANOS6_FPGA_SUCCESS,
	NANOS6_FPGA_ERROR
} nanos6_fpga_stat_t;

typedef enum {
	NANOS6_FPGA_DEV_TO_HOST,
	NANOS6_FPGA_HOST_TO_DEV
} nanos6_fpga_copy_t;

void nanos6_fpga_addArg(int index, unsigned char flags, unsigned long long value, void* taskHandle);
void nanos6_fpga_addArgs(int num, unsigned char flags, const unsigned long long* values, void* taskHandle);

nanos6_fpga_stat_t nanos6_fpga_malloc(uint64_t size, uint64_t* fpga_addr);
nanos6_fpga_stat_t nanos6_fpga_free(uint64_t fpga_addr);
nanos6_fpga_stat_t nanos6_fpga_memcpy(void* usr_ptr, uint64_t fpga_addr, uint64_t size, nanos6_fpga_copy_t copy_type);

nanos6_fpga_stat_t nanos6_fpga_memcpy_wideport_in(void* dst, const unsigned long long int addr, const unsigned int num_elems);
nanos6_fpga_stat_t nanos6_fpga_memcpy_wideport_out(void* dst, const unsigned long long int addr, const unsigned int num_elems);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* NANOS6_FPGA_DEVICE_H */

