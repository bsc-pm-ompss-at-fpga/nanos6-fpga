/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_ACCELERATOR_HPP
#define FPGA_ACCELERATOR_HPP

#include <list>
#include "support/config/ConfigVariable.hpp"

#include "hardware/device/Accelerator.hpp"
#include "support/config/toml/string.hpp"
#include "tasks/Task.hpp"
#include "memory/allocator/devices/FPGAPinnedAllocator.hpp"
#include "FPGAReverseOffload.hpp"
#include "FPGAAcceleratorInstrumentation.hpp"

class FPGAAccelerator : public Accelerator {
private:
	enum {
		REAL_ASYNC,
		FORCED_ASYNC,
		SYNC
	} _mem_sync_type;

	FPGAPinnedAllocator _allocator;

	size_t accCount;
	FPGAReverseOffload _reverseOffload;
	FPGAAcceleratorInstrumentation *acceleratorInstrumentationServices;
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
	inline void generateDeviceEvironment(DeviceEnvironment&, const nanos6_task_implementation_info_t*) override;

	inline void finishTaskCleanup([[maybe_unused]] Task *task) override{}

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;

public:

	static std::unordered_map<const nanos6_task_implementation_info_t*, uint64_t> _device_subtype_map;

	FPGAAccelerator(int fpgaDeviceIndex);
	~FPGAAccelerator() override;

	std::pair<void *, bool> accel_allocate(size_t size) override;

	bool accel_free(void* ptr) override;

	int getVendorDeviceId() const override
	{
		return _deviceHandler;
	}

	inline void setActiveDevice() const override {}

	void initializeService() override {
		Accelerator::initializeService();
		if (ConfigVariable<bool>("devices.fpga.reverse_offload")) {
			_reverseOffload.initializeService();
		}
		if (ConfigVariable<std::string>("version.instrument").getValue() == "ovni") {
			for (unsigned int i = 0; i < accCount; ++i)
				acceleratorInstrumentationServices[i].initializeService();
		}
	}

	void shutdownService() override {
		Accelerator::shutdownService();
		if (ConfigVariable<bool>("devices.fpga.reverse_offload")) {
			_reverseOffload.shutdownService();
		}
		if (ConfigVariable<std::string>("version.instrument").getValue() == "ovni") {
			for (unsigned int i = 0; i < accCount; ++i)
				acceleratorInstrumentationServices[i].shutdownService();
		}
	}

	std::function<std::function<bool(void)>()> copy_in(void *dst, void *src, size_t size, void* copy_extra) const override;
	std::function<std::function<bool(void)>()> copy_out(void *dst, void *src, size_t size, void* copy_extra) const override;
	std::function<std::function<bool(void)>()> copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* copy_extra) const override;
};

#endif // FPGA_ACCELERATOR_HPP
