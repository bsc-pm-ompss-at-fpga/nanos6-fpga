/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#ifdef USE_DISTRIBUTED
#include "hardware/device/broadcaster/BroadcasterDeviceInfo.hpp"
#include "hardware/device/broadcaster/BroadcasterAccelerator.hpp"

#include <nanos6/distributed.h>

extern "C" {

int nanos6_dist_num_devices()
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	return ((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->getNumDevices();
}

void nanos6_dist_map_address(const void* address, size_t size)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->mapSymbol(address, size);
}

void nanos6_dist_unmap_address(const void* address)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->unmapSymbol(address);
}

void nanos6_dist_memcpy_to_all(const void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToAll(address, size, srcOffset, dstOffset);
}

void nanos6_dist_scatter(const void* address, size_t size, size_t sendOffset, size_t recvOffset) {
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->scatter(address, size, sendOffset, recvOffset);
}

void nanos6_dist_gather(void* address, size_t size, size_t sendOffset, size_t recvOffset) {
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->gather(address, size, sendOffset, recvOffset);
}

void nanos6_dist_memcpy_to_device(int dev_id, const void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToDevice(dev_id, address, size, srcOffset, dstOffset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, size_t size, size_t srcOffset, size_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyFromDevice(dev_id, address, size, srcOffset, dstOffset);
}

void OMPIF_Send([[maybe_unused]] const void* data,
				[[maybe_unused]] int count,
				[[maybe_unused]] OMPIF_Datatype datatype,
				[[maybe_unused]] int destination,
				[[maybe_unused]] uint8_t tag,
				[[maybe_unused]] OMPIF_Comm communicator)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

void OMPIF_Recv([[maybe_unused]] void* data,
				[[maybe_unused]] int count,
				[[maybe_unused]] OMPIF_Datatype datatype,
				[[maybe_unused]] int source,
				[[maybe_unused]] uint8_t tag,
				[[maybe_unused]] OMPIF_Comm communicator)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

int OMPIF_Comm_rank([[maybe_unused]] OMPIF_Comm communicator)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
	return 0;
}

int OMPIF_Comm_size([[maybe_unused]] OMPIF_Comm communicator)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
	return 0;
}

}

#endif
