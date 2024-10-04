/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef BROADCASTER_ACCELERATOR_HPP
#define BROADCASTER_ACCELERATOR_HPP

#include "hardware/device/Accelerator.hpp"
#include "tasks/Task.hpp"
#include "hardware/device/AcceleratorStream.hpp"
#include <nanos6/distributed.h>

class BroadcasterAccelerator : public Accelerator {
private:

	std::atomic<bool> _clusterStopService;
	std::atomic<bool> _clusterFinishedService;

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

	inline void clusterServiceLoop() {
		while (!_clusterStopService) {
			bool anyOngoing;
			do {
				anyOngoing = false;
				for (AcceleratorStream& stream : acceleratorStreams) {
					stream.streamServiceLoop();
					anyOngoing |= stream.streamPendingExecutors();
				}
			} while (_isPinnedPolling && anyOngoing);
			BlockingAPI::waitForUs(_pollingPeriodUs);
		}
	}

public:

	BroadcasterAccelerator(const std::vector<Accelerator*>& cluster);

	void mapSymbol(const void* symbol, uint64_t size);
	void unmapSymbol(const void* symbol);
	void memcpyToAll(const void* symbol, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
	void memcpyToDevice(int devId, const void* symbol, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
	void memcpyFromDevice(int devId, void* symbol, uint64_t size, uint64_t srcOffset, uint64_t dstOffset);
	void scatter(const void* symbol, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);
	void gather(void* symbol, uint64_t size, uint64_t sendOffset, uint64_t recvOffset);
	void scatterv(const void* symbol, const uint64_t *sizes, const uint64_t *sendOffsets, const uint64_t *recvOffsets);
	void memcpyVector(void* symbol, int vectorLen, nanos6_dist_memcpy_info_t* v, nanos6_dist_copy_dir_t dir);

	std::pair<void *, bool> accel_allocate([[maybe_unused]] size_t size) override {
		return {nullptr, true};
	}

	bool accel_free([[maybe_unused]] void* ptr) override {return true;}

	std::pair<std::shared_ptr<DeviceAllocation>, bool> createNewDeviceAllocation(const DataAccessRegion &region) override {
		return {std::make_shared<DeviceAllocation>(region, region, []{}), true};
	}

	int getNumDevices()
	{
		return cluster.size();
	}

	inline void setActiveDevice() const override {}
	inline int getVendorDeviceId() const override {return 0;}
	inline void submitDevice(const DeviceEnvironment&, const void*, const nanos6_task_info_t*, const nanos6_address_translation_entry_t*) const override {}
	inline std::function<bool()> getDeviceSubmissionFinished(const DeviceEnvironment&) const override {return []() -> bool {return true;};}
	inline void generateDeviceEvironment(DeviceEnvironment&, const nanos6_task_implementation_info_t*) override {}

};
#endif // BROADCASTER_ACCELERATOR_HPP

