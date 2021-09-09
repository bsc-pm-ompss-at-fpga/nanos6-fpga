/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_DISTRIBUTED_H
#define NANOS6_DISTRIBUTED_H

#include "major.h"

#pragma GCC visibility push(default)

// NOTE: The full version depends also on nanos6_major_api
// That is:   nanos6_major_api . nanos6_distributed_api
enum nanos6_distributed_api_t { nanos6_distributed_api = 1 };

#ifdef __cplusplus
extern "C" {
#endif

int nanos6_dist_num_devices();
void nanos6_dist_map_address(void* address, size_t size);
void nanos6_dist_unmap_address(void* address);
void nanos6_dist_memcpy_to_all(void* address, size_t size, size_t offset);
void nanos6_dist_memcpy_to_device(int dev_id, void* address, size_t size, size_t offset);
void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t offset);

#ifdef __cplusplus
}
#endif

#pragma GCC visibility pop

#endif /* NANOS6_DISTRIBUTED_H */

