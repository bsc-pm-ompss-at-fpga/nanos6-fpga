/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_ACCELERATOR_HPP
#define FPGA_ACCELERATOR_HPP

#include <list>
#include "support/config/ConfigVariable.hpp"

#include "hardware/device/Accelerator.hpp"
#include "tasks/Task.hpp"
#include "src/memory/allocator/devices/FPGAPinnedAllocator.hpp"


class FPGAAccelerator : public Accelerator {
private:
	enum {
		REAL_ASYNC,
		FORCED_ASYNC,
		SYNC
	} _mem_sync_type;

	FPGAPinnedAllocator _allocator;

	struct _fpgaAccel
	{
		std::vector<xtasks_acc_handle> _accelHandle;
		unsigned idx = 0;
		xtasks_acc_handle getHandle(int instance) const {
			return _accelHandle[instance];
		}
		xtasks_acc_handle getHandle()
		{
			return _accelHandle[(idx++)%_accelHandle.size()]; 
		}
	};
	std::unordered_map<uint64_t, _fpgaAccel> _inner_accelerators;

    void submitDevice(const DeviceEnvironment& deviceEnvironment) const override;
	std::function<bool()> getDeviceSubmissionFinished(const DeviceEnvironment& deviceEnvironment) const override;
	inline void generateDeviceEvironment(DeviceEnvironment& env, uint64_t deviceSubtypeId) override;

	inline void finishTaskCleanup([[maybe_unused]] Task *task) override{}

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;


    std::function<std::function<bool(void)>()> copy_in(void *dst, void *src, size_t size, void* copy_extra) const override;
    std::function<std::function<bool(void)>()> copy_out(void *dst, void *src, size_t size, void* copy_extra) const override;
    std::function<std::function<bool(void)>()> copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* copy_extra) const override;

public:
	FPGAAccelerator(int fpgaDeviceIndex);
	
	std::pair<void *, bool> accel_allocate(size_t size) override;

	void accel_free(void* ptr) override;

	int getVendorDeviceId() const override
	{
		return _deviceHandler;
	}

	inline void setActiveDevice() const override {}

};

#endif // FPGA_ACCELERATOR_HPP
