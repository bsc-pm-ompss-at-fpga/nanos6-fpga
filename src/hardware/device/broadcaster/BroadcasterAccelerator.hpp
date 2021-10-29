/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef BROADCASTER_ACCELERATOR_HPP
#define BROADCASTER_ACCELERATOR_HPP

#include "hardware/device/Accelerator.hpp"
#include "tasks/Task.hpp"

class BroadcasterAccelerator : public Accelerator {
private:

	std::vector<Accelerator*> cluster;
	std::vector<DeviceEnvironment> deviceEnvironments;
	std::vector<AcceleratorStream> acceleratorStreams;
	std::unordered_map<const void*, std::vector<void*>> translationTable;

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;

	void finishTaskCleanup([[maybe_unused]] Task* task) override
	{
		return;
	}

	std::function<std::function<bool(void)>()> copy_in(void *dst, void *src, size_t size, void* copy_extra) const override;
	std::function<std::function<bool(void)>()> copy_out(void *dst, void *src, size_t size, void* copy_extra) const override;
	std::function<std::function<bool(void)>()> copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* copy_extra) const override;

public:
	BroadcasterAccelerator(const std::vector<Accelerator*>& cluster);

	void mapSymbol(const void* symbol, size_t size);
	void unmapSymbol(const void* symbol);
	void memcpyToAll(const void* symbol, size_t size, size_t srcOffset, size_t dstOffset);
	void memcpyToDevice(int devId, const void* symbol, size_t size, size_t srcOffset, size_t dstOffset);
	void memcpyFromDevice(int devId, void* symbol, size_t size, size_t srcOffset, size_t dstOffset);
	void scatter(const void* symbol, size_t size, size_t sendOffset, size_t recvOffset);
	void gather(void* symbol, size_t size, size_t sendOffset, size_t recvOffset);

	std::pair<void *, bool> accel_allocate([[maybe_unused]] size_t size) override {
		return {nullptr, true};
	}

	void accel_free([[maybe_unused]] void* ptr) override {}

	std::pair<std::shared_ptr<DeviceAllocation>, bool> createNewDeviceAllocation(const DataAccessRegion &region) override {
		return {std::make_shared<DeviceAllocation>(region, region, []{}), true};
	}

	int getNumDevices()
	{
		return cluster.size();
	}

	inline void setActiveDevice() const override {}
	inline int getVendorDeviceId() const override {return 0;}
	inline void submitDevice([[maybe_unused]] const DeviceEnvironment&) const override {}
	inline std::function<bool()> getDeviceSubmissionFinished(const DeviceEnvironment&) const override {return []() -> bool {return true;};}
	inline void generateDeviceEvironment(DeviceEnvironment&, uint64_t) override {}
};

#endif // BROADCASTER_ACCELERATOR_HPP
