/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_FPGA
#include "resolve.h"

#pragma GCC visibility push(default)



void nanos6_fpga_addArg(int index, int symbolData, void* taskHandle, void* address)
{
    typedef void nanos6_fpga_addArg_t(int index, int symbolData, void* taskHandle, void* address);

    static nanos6_fpga_addArg_t *symbol = NULL;
    if (__builtin_expect(symbol == NULL, 0))
    {
        symbol = (nanos6_fpga_addArg_t *) _nanos6_resolve_symbol("nanos6_fpga_addArg", "essential", NULL);
    }
    
    (*symbol)(index,symbolData,taskHandle,  address);
}


#pragma GCC visibility pop
#endif