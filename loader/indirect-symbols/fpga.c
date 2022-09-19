/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_FPGA
#include "resolve.h"
#include "api/nanos6/fpga_device.h"

#pragma GCC visibility push(default)

void nanos6_fpga_addArg(int index, unsigned char flags, unsigned long long value, void* taskHandle)
{
    typedef void nanos6_fpga_addArg_t(int index, unsigned char flags, unsigned long long value, void* taskHandle);

    static nanos6_fpga_addArg_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_addArg_t *) _nanos6_resolve_symbol("nanos6_fpga_addArg", "essential", NULL);
    }

    (*symbol)(index, flags, value, taskHandle);
}

void nanos6_fpga_addArgs(int num, unsigned char flags, const unsigned long long* values, void* taskHandle)
{
    typedef void nanos6_fpga_addArgs_t(int num, unsigned char flags, const unsigned long long* values, void* taskHandle);

    static nanos6_fpga_addArgs_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_addArgs_t *) _nanos6_resolve_symbol("nanos6_fpga_addArgs", "essential", NULL);
    }

    (*symbol)(num, flags, values, taskHandle);
}

nanos6_fpga_stat_t nanos6_fpga_malloc(uint64_t size, uint64_t* fpga_addr)
{
    typedef void nanos6_fpga_malloc_t(uint64_t size, uint64_t* fpga_addr);

    static nanos6_fpga_malloc_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_malloc_t *) _nanos6_resolve_symbol("nanos6_fpga_malloc", "essential", NULL);
    }

    (*symbol)(size, fpga_addr);
}

nanos6_fpga_stat_t nanos6_fpga_free(uint64_t fpga_addr)
{
    typedef void nanos6_fpga_free_t(uint64_t fpga_addr);

    static nanos6_fpga_free_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_free_t *) _nanos6_resolve_symbol("nanos6_fpga_free", "essential", NULL);
    }

    (*symbol)(fpga_addr);
}

nanos6_fpga_stat_t nanos6_fpga_memcpy(void* usr_ptr, uint64_t fpga_addr, uint64_t size, nanos6_fpga_copy_t copy_type)
{
    typedef void nanos6_fpga_memcpy_t(void* usr_ptr, uint64_t fpga_addr, uint64_t size, nanos6_fpga_copy_t copy_type);

    static nanos6_fpga_memcpy_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_memcpy_t *) _nanos6_resolve_symbol("nanos6_fpga_memcpy", "essential", NULL);
    }

    (*symbol)(usr_ptr, fpga_addr, size, copy_type);
}

#pragma GCC visibility pop
#endif
