/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef NANOS6_DISTRIBUTED_H
#define NANOS6_DISTRIBUTED_H

#include "major.h"
#include <stdint.h>

// NOTE: The full version depends also on nanos6_major_api
// That is:   nanos6_major_api . nanos6_distributed_api
enum nanos6_distributed_api_t { nanos6_distributed_api = 1 };

typedef struct {
    uint64_t size;
    uint64_t sendOffset;
    uint64_t recvOffset;
    int devId;
} nanos6_dist_memcpy_info_t;

typedef enum {
    NANOS6_DIST_COPY_TO,
    NANOS6_DIST_COPY_FROM
} nanos6_dist_copy_dir_t;

#ifdef __cplusplus
extern "C" {
#endif

int nanos6_dist_num_devices();
void nanos6_dist_map_address(const void* address, uint64_t size);
void nanos6_dist_unmap_address(const void* address);
void nanos6_dist_memcpy_to_all(const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
void nanos6_dist_scatter(const void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);
void nanos6_dist_gather(void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);
void nanos6_dist_scatterv(const void* address, const uint64_t *sizes, const uint64_t *sendOffsets, const uint64_t *recvOffsets);
void nanos6_dist_memcpy_to_device(int dev_id, const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
void nanos6_dist_memcpy_from_device(int dev_id, void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
void nanos6_dist_memcpy_vector(void* address, int vector_len, nanos6_dist_memcpy_info_t* v, nanos6_dist_copy_dir_t dir);

void OMPIF_Send(const void *data, unsigned int size, int destination, int tag, int numDeps, const uint64_t deps[]);
void OMPIF_Recv(void *data, unsigned int size, int source, int tag, int numDeps, const uint64_t deps[]);
void OMPIF_Allgather(void* data, unsigned int size);
void OMPIF_Bcast(void* data, unsigned int size, int root);
int OMPIF_Comm_rank();
int OMPIF_Comm_size();

#ifdef __cplusplus
}
#endif

#endif /* NANOS6_DISTRIBUTED_H */

