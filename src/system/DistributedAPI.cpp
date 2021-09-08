/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#ifdef USE_DISTRIBUTED
#include "hardware/device/broadcaster/BroadcasterDeviceInfo.hpp"
#include "hardware/device/broadcaster/BroadcasterAccelerator.hpp"

extern "C" {

int nanos6_dist_num_devices()
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	return ((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->getNumDevices();
}

void nanos6_dist_map_symbol(void* address, size_t size)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->mapSymbol(address, size);
}

void nanos6_dist_unmap_symbol(void* address)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->unmapSymbol(address);
}

void nanos6_dist_memcpy_to_all(void* address, size_t size, size_t offset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToAll(address, size, offset);
}

void nanos6_dist_memcpy_to_device(int dev_id, void* address, size_t size, size_t offset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToDevice(dev_id, address, size, offset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t offset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyFromDevice(dev_id, address, size, offset);
}

}

#endif
