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

void nanos6_dist_map_address(const void* address, size_t size)
{
	typedef void nanos6_dist_map_address_t(const void* address, size_t size);

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

void nanos6_dist_memcpy_to_all(const void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	typedef void nanos6_dist_memcpy_to_all_t(const void* address, size_t size, size_t srcOffset, size_t dstOffset);

	static nanos6_dist_memcpy_to_all_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_all_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_all", "essential", NULL);
	}

	(*symbol)(address, size, srcOffset, dstOffset);
}

void nanos6_dist_scatter(const void* address, size_t size, size_t sendOffset, size_t recvOffset) {
	typedef void nanos6_dist_scatter_t(const void* address, size_t size, size_t sendOffset, size_t recvOffset);

	static nanos6_dist_scatter_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_scatter_t *) _nanos6_resolve_symbol("nanos6_dist_scatter", "essential", NULL);
	}

	(*symbol)(address, size, sendOffset, recvOffset);
}

void nanos6_dist_gather(void* address, size_t size, size_t sendOffset, size_t recvOffset) {
	typedef void nanos6_dist_gather_t(void* address, size_t size, size_t sendOffset, size_t recvOffset);

	static nanos6_dist_gather_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_gather_t *) _nanos6_resolve_symbol("nanos6_dist_gather", "essential", NULL);
	}

	(*symbol)(address, size, sendOffset, recvOffset);
}


void nanos6_dist_memcpy_to_device(int dev_id, const void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	typedef void nanos6_dist_memcpy_to_device_t(int dev_id, const void* address, size_t size, size_t srcOffset, size_t dstOffset);

	static nanos6_dist_memcpy_to_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, srcOffset, dstOffset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	typedef void nanos6_dist_memcpy_from_device_t(int dev_id, void* address, size_t size, size_t srcOffset, size_t dstOffset);

	static nanos6_dist_memcpy_from_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_from_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_from_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, srcOffset, dstOffset);
}

void OMPIF_Send(const void* data, int count, OMPIF_Datatype datatype, int destination, uint8_t tag, OMPIF_Comm communicator)
{
	typedef void OMPIF_Send_t(const void* data, int count, OMPIF_Datatype datatype, int destination, uint8_t tag, OMPIF_Comm communicator);

	static OMPIF_Send_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Send_t *) _nanos6_resolve_symbol("OMPIF_Send", "essential", NULL);
	}

	(*symbol)(data, datatype, destination, tag, communicator);
}

void OMPIF_Recv(void* data, int count, OMPIF_Datatype datatype, int source, uint8_t tag, OMPIF_Comm communicator)
{
	typedef void OMPIF_Recv_t(void* data, int count, OMPIF_Datatype datatype, int source, uint8_t tag, OMPIF_Comm communicator);

	static OMPIF_Recv_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Recv_t *) _nanos6_resolve_symbol("OMPIF_Recv", "essential", NULL);
	}

	(*symbol)(data, datatype, destination, tag, communicator);
}

int OMPIF_Comm_rank(OMPIF_Comm communicator)
{
	typedef int OMPIF_Comm_rank_t(OMPIF_Comm communicator);

	static OMPIF_Comm_rank_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Comm_rank_t *) _nanos6_resolve_symbol("OMPIF_Comm_rank", "essential", NULL);
	}

	return (*symbol)(communicator);
}

int OMPIF_Comm_size(OMPIF_Comm communicator)
{
	typedef int OMPIF_Comm_size_t(OMPIF_Comm communicator);

	static OMPIF_Comm_size_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (OMPIF_Comm_size_t *) _nanos6_resolve_symbol("OMPIF_Comm_size", "essential", NULL);
	}

	return (*symbol)(communicator);
}

#pragma GCC visibility pop
#endif
