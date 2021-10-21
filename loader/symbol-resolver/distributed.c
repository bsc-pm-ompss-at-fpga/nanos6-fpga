/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>
#ifdef USE_DISTRIBUTED

#include "resolve.h"

RESOLVE_API_FUNCTION(nanos6_dist_num_devices, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_map_address, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_unmap_address, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_memcpy_to_all, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_scatter, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_gather, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_memcpy_to_device, "essential", NULL);
RESOLVE_API_FUNCTION(nanos6_dist_memcpy_from_device, "essential", NULL);
RESOLVE_API_FUNCTION(OMPIF_Send, "essential", NULL);
RESOLVE_API_FUNCTION(OMPIF_Recv, "essential", NULL);
RESOLVE_API_FUNCTION(OMPIF_Comm_rank, "essential", NULL);
RESOLVE_API_FUNCTION(OMPIF_Comm_size, "essential", NULL);

#endif
