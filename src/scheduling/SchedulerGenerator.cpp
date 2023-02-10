/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2021 Barcelona Supercomputing Center (BSC)
*/

#include "SchedulerGenerator.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "scheduling/schedulers/HostScheduler.hpp"
#include "scheduling/schedulers/device/DeviceScheduler.hpp"

HostScheduler *SchedulerGenerator::createHostScheduler(
	size_t totalComputePlaces,
	SchedulingPolicy policy,
	bool enablePriority)
{
	return new HostScheduler(totalComputePlaces, policy, enablePriority);
}

DeviceScheduler *SchedulerGenerator::createDeviceScheduler(
	size_t totalComputePlaces,
	SchedulingPolicy policy,
	bool enablePriority,
	nanos6_device_t deviceType)
{
	// If there are no devices, disable the scheduler
	if (totalComputePlaces == 0)
		return nullptr;

	switch(deviceType) {
		case nanos6_cuda_device:
			return new DeviceScheduler(
				totalComputePlaces,
				policy,
				enablePriority,
				deviceType,
				"CUDADeviceScheduler");
		case nanos6_openacc_device:
			return new DeviceScheduler(
				totalComputePlaces,
				policy,
				enablePriority,
				deviceType,
				"OpenAccDeviceScheduler");
		case nanos6_opencl_device:
			FatalErrorHandler::fail("OpenCL is not supported yet.");
			break;
		case nanos6_fpga_device:
			return new DeviceScheduler(
				totalComputePlaces,
				policy,
				enablePriority,
				deviceType,
				"FPGADeviceScheduler");
		case nanos6_broadcaster_device:
			return new DeviceScheduler(
				totalComputePlaces,
				policy,
				enablePriority,
				deviceType,
				"BroadcasterDeviceScheduler");
		case nanos6_cluster_device:
			FatalErrorHandler::fail("Cluster is not actually a device.");
			break;
		default:
			FatalErrorHandler::fail("Unknown or unsupported device.");
	}
	return nullptr;
}

