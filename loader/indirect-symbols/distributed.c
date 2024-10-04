/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_DISTRIBUTED
#include "resolve.h"

#pragma GCC visibility push(default)

int nanos6_dist_num_devices()
{
	typedef void nanos6_dist_num_devices_t();

	static nanos6_dist_num_devices_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_num_devices_t *) _nanos6_resolve_symbol("nanos6_dist_num_devices", "essential", NULL);
	}

	(*symbol)();
}

void nanos6_dist_map_address(const void* address, uint64_t size)
{
	typedef void nanos6_dist_map_address_t(const void* address, uint64_t size);

	static nanos6_dist_map_address_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_map_address_t *) _nanos6_resolve_symbol("nanos6_dist_map_address", "essential", NULL);
	}

	(*symbol)(address, size);
}

void nanos6_dist_unmap_address(const void* address)
{
	typedef void nanos6_dist_unmap_address_t(const void* address);

	static nanos6_dist_unmap_address_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_unmap_address_t *) _nanos6_resolve_symbol("nanos6_dist_unmap_address", "essential", NULL);
	}

	(*symbol)(address);
}

void nanos6_dist_memcpy_to_all(const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	typedef void nanos6_dist_memcpy_to_all_t(const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);

	static nanos6_dist_memcpy_to_all_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_all_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_all", "essential", NULL);
	}

	(*symbol)(address, size, srcOffset, dstOffset);
}

void nanos6_dist_scatter(const void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset) {
	typedef void nanos6_dist_scatter_t(const void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);

	static nanos6_dist_scatter_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_scatter_t *) _nanos6_resolve_symbol("nanos6_dist_scatter", "essential", NULL);
	}

	(*symbol)(address, size, sendOffset, recvOffset);
}

void nanos6_dist_gather(void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset) {
	typedef void nanos6_dist_gather_t(void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);

	static nanos6_dist_gather_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_gather_t *) _nanos6_resolve_symbol("nanos6_dist_gather", "essential", NULL);
	}

	(*symbol)(address, size, sendOffset, recvOffset);
}


void nanos6_dist_memcpy_to_device(int dev_id, const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	typedef void nanos6_dist_memcpy_to_device_t(int dev_id, const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);

	static nanos6_dist_memcpy_to_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, srcOffset, dstOffset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	typedef void nanos6_dist_memcpy_from_device_t(int dev_id, void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);

	static nanos6_dist_memcpy_from_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_from_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_from_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, srcOffset, dstOffset);
}

void OMPIF_Send(const void *data, unsigned int size, int destination, int tag, int numDeps, const uint64_t deps[])
{
	typedef void OMPIF_Send_t(const void *data, unsigned int size, int destination, int tag, int numDeps, const uint64_t deps[]);

	static OMPIF_Send_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Send_t *) _nanos6_resolve_symbol("OMPIF_Send", "essential", NULL);
	}

	(*symbol)(data, size, destination, tag, numDeps, deps);
}

void OMPIF_Allgather(void* data, unsigned int size)
{
	typedef void OMPIF_Allgather_t(void* data, unsigned int size);
	static OMPIF_Allgather_t *symbol = NULL;

	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Allgather_t *) _nanos6_resolve_symbol("OMPIF_Allgather", "essential", NULL);
	}

	(*symbol)(data, size);

}

void OMPIF_Bcast(void* data, unsigned int size, int root)
{
	typedef void OMPIF_Bcast_t(void* data, unsigned int size, int root);
	static OMPIF_Bcast_t *symbol = NULL;

	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Bcast_t *) _nanos6_resolve_symbol("OMPIF_Bcast", "essential", NULL);
	}

	(*symbol)(data, size, root);
}


void OMPIF_Recv(void *data, unsigned int size, int source, int tag, int numDeps, const uint64_t deps[])
{
	typedef void OMPIF_Recv_t(void *data, unsigned int size, int source, int tag, int numDeps, const uint64_t deps[]);

	static OMPIF_Recv_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Recv_t *) _nanos6_resolve_symbol("OMPIF_Recv", "essential", NULL);
	}

	(*symbol)(data, size, source, tag, numDeps, deps);
}

int OMPIF_Comm_rank()
{
	typedef int OMPIF_Comm_rank_t();

	static OMPIF_Comm_rank_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Comm_rank_t *) _nanos6_resolve_symbol("OMPIF_Comm_rank", "essential", NULL);
	}

	return (*symbol)();
}

int OMPIF_Comm_size()
{
	typedef int OMPIF_Comm_uint64_t();

	static OMPIF_Comm_uint64_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Comm_uint64_t *) _nanos6_resolve_symbol("OMPIF_Comm_size", "essential", NULL);
	}

	return (*symbol)();
}

#pragma GCC visibility pop
#endif

