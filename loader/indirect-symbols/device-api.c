/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/



#include "resolve.h"

#pragma GCC visibility push(default)

int nanos6_get_device_num(nanos6_device_t device)
{
	typedef int nanos6_get_device_num_t(nanos6_device_t device);

	static nanos6_get_device_num_t *symbol = NULL;
	if (__builtin_expect(symbol == NULL, 0)) 
    {
		symbol = (nanos6_get_device_num_t *) _nanos6_resolve_symbol("nanos6_get_device_num", "essential", NULL);
	}

	return (*symbol)(device);
}


void nanos6_device_memcpy(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
{
    typedef void nanos6_device_memcpy_t(nanos6_device_t device, int device_id, void* host_ptr, size_t size);

    static nanos6_device_memcpy_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_device_memcpy_t *) _nanos6_resolve_symbol("nanos6_device_memcpy", "essential", NULL);
    }
    
    (*symbol)(device,device_id,host_ptr,size);
}

void nanos6_set_home(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
{
    typedef void nanos6_set_home_t(nanos6_device_t device, int device_id, void* host_ptr, size_t size);

    static nanos6_set_home_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_set_home_t *) _nanos6_resolve_symbol("nanos6_set_home", "essential", NULL);
    }
    
    (*symbol)(device,device_id,host_ptr,size);
}


void nanos6_set_noflush(void* host_ptr, size_t size)
{
    typedef void nanos6_set_noflush_t(void* host_ptr, size_t size);

    static nanos6_set_noflush_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_set_noflush_t *) _nanos6_resolve_symbol("nanos6_set_noflush", "essential", NULL);
    }
    
    (*symbol)(host_ptr,size);
}



void nanos6_print_directory()
{
    typedef void nanos6_print_directory_t();

    static nanos6_print_directory_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_print_directory_t *) _nanos6_resolve_symbol("nanos6_print_directory", "essential", NULL);
    }
    
    (*symbol)();
}



#pragma GCC visibility pop
