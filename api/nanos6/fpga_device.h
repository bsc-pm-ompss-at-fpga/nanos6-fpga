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

void nanos6_fpga_addArg(int index, unsigned char flags, unsigned long long value, void* taskHandle);
void nanos6_fpga_addArgs(int num, unsigned char flags, const unsigned long long* values, void* taskHandle);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* NANOS6_FPGAC_DEVICE_H */

