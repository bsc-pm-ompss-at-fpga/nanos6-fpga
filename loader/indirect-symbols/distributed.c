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

void nanos6_dist_map_address(void* address, size_t size)
{
	typedef void nanos6_dist_map_address_t(void* address, size_t size);

	static nanos6_dist_map_address_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_map_address_t *) _nanos6_resolve_symbol("nanos6_dist_map_address", "essential", NULL);
	}

	(*symbol)(address, size);
}

void nanos6_dist_unmap_address(void* address)
{
	typedef void nanos6_dist_unmap_address_t(void* address);

	static nanos6_dist_unmap_address_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_unmap_address_t *) _nanos6_resolve_symbol("nanos6_dist_unmap_address", "essential", NULL);
	}

	(*symbol)(address);
}

void nanos6_dist_memcpy_to_all(void* address, size_t size, size_t offset)
{
	typedef void nanos6_dist_memcpy_to_all_t(void* address, size_t size, size_t offset);

	static nanos6_dist_memcpy_to_all_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_all_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_all", "essential", NULL);
	}

	(*symbol)(address, size, offset);
}

void nanos6_dist_memcpy_to_device(int dev_id, void* address, size_t size, size_t offset)
{
	typedef void nanos6_dist_memcpy_to_device_t(int dev_id, void* address, size_t size, size_t offset);

	static nanos6_dist_memcpy_to_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_to_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_to_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, offset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t offset)
{
	typedef void nanos6_dist_memcpy_from_device_t(int dev_id, void* address, size_t size, size_t offset);

	static nanos6_dist_memcpy_from_device_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0))
	{
		symbol = (nanos6_dist_memcpy_from_device_t *) _nanos6_resolve_symbol("nanos6_dist_memcpy_from_device", "essential", NULL);
	}

	(*symbol)(dev_id, address, size, offset);
}

#pragma GCC visibility pop
#endif
