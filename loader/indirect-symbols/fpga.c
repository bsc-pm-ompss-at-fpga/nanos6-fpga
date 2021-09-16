/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_FPGA
#include "resolve.h"

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


#pragma GCC visibility pop
#endif
