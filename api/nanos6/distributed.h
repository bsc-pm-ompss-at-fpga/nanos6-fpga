/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_DISTRIBUTED_H
#define NANOS6_DISTRIBUTED_H

#include "major.h"
#include <stdint.h>
#include <stddef.h>

// NOTE: The full version depends also on nanos6_major_api
// That is:   nanos6_major_api . nanos6_distributed_api
enum nanos6_distributed_api_t { nanos6_distributed_api = 1 };

typedef enum {
	OMPIF_INT,
	OMPIF_DOUBLE,
	OMPIF_FLOAT
} OMPIF_Datatype;

typedef enum {
	OMPIF_COMM_WORLD
} OMPIF_Comm;

#ifdef __cplusplus
extern "C" {
#endif

int nanos6_dist_num_devices();
void nanos6_dist_map_address(const void* address, size_t size);
void nanos6_dist_unmap_address(const void* address);
void nanos6_dist_memcpy_to_all(const void* address, size_t size, size_t offset);
void nanos6_dist_scatter(const void* address, size_t size, size_t sendOffset, size_t recvOffset);
void nanos6_dist_gather(void* address, size_t size, size_t sendOffset, size_t recvOffset);
void nanos6_dist_memcpy_to_device(int dev_id, const void* address, size_t size, size_t offset);
void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t offset);
void OMPIF_Send(const void* data, int count, OMPIF_Datatype datatype, int destination, uint8_t tag, OMPIF_Comm communicator);
void OMPIF_Recv(void* data, int count, OMPIF_Datatype datatype, int source, uint8_t tag, OMPIF_Comm communicator);
int OMPIF_Comm_rank(OMPIF_Comm communicator);
int OMPIF_Comm_size(OMPIF_Comm communicator);

#ifdef __cplusplus
}
#endif

#endif /* NANOS6_DISTRIBUTED_H */

