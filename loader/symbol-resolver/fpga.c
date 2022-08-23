/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_FPGA

#include "resolve.h"

RESOLVE_API_FUNCTION(nanos6_fpga_addArg, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_fpga_addArgs, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_fpga_malloc, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_fpga_free, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_fpga_memcpy, "essential", NULL);

#endif
