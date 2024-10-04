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

void nanos6_dist_map_address(const void* address, uint64_t size)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->mapSymbol(address, size);
}

void nanos6_dist_unmap_address(const void* address)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->unmapSymbol(address);
}

void nanos6_dist_memcpy_to_all(const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToAll(address, size, srcOffset, dstOffset);
}

void nanos6_dist_scatter(const void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset) {
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->scatter(address, size, sendOffset, recvOffset);
}

void nanos6_dist_scatterv(const void* address, const uint64_t *sizes, const uint64_t *sendOffsets, const uint64_t *recvOffsets) {
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->scatterv(address, sizes, sendOffsets, recvOffsets);
}

void nanos6_dist_gather(void* address, uint64_t size, uint64_t sendOffset, uint64_t recvOffset) {
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->gather(address, size, sendOffset, recvOffset);
}

void nanos6_dist_memcpy_to_device(int dev_id, const void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyToDevice(dev_id, address, size, srcOffset, dstOffset);
}

void nanos6_dist_memcpy_from_device(int dev_id, void* address, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyFromDevice(dev_id, address, size, srcOffset, dstOffset);
}

void nanos6_dist_memcpy_vector(void* address, int vector_len, nanos6_dist_memcpy_info_t* v, nanos6_dist_copy_dir_t dir)
{
	BroadcasterDeviceInfo* devInfo = (BroadcasterDeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_broadcaster_device);
	((BroadcasterAccelerator*)(devInfo->getAccelerators()[0]))->memcpyVector(address, vector_len, v, dir);
}

void OMPIF_Send([[maybe_unused]] const void *data,
                [[maybe_unused]] unsigned int size,
                [[maybe_unused]] int destination,
                [[maybe_unused]] int tag,
                [[maybe_unused]] int numDeps,
                [[maybe_unused]] const uint64_t deps[])
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

void OMPIF_Recv([[maybe_unused]] void *data,
                [[maybe_unused]] unsigned int size,
                [[maybe_unused]] int source,
                [[maybe_unused]] int tag,
                [[maybe_unused]] int numDeps,
                [[maybe_unused]] const uint64_t deps[])
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

void OMPIF_Allgather([[maybe_unused]] void* data,
                     [[maybe_unused]] unsigned int size)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

void OMPIF_Bcast([[maybe_unused]] void* data,
                 [[maybe_unused]] unsigned int size,
                 [[maybe_unused]] int root)
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
}

int OMPIF_Comm_rank()
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
	return 0;
}

int OMPIF_Comm_size()
{
	FatalErrorHandler::fail("You shouldn't call OMPIF from CPU");
	return 0;
}

}

#endif
