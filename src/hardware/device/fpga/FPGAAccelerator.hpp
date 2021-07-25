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
	bool _supports_async;
	FPGAPinnedAllocator _allocator;

	struct _fpgaAccel
	{
		std::vector<xtasks_acc_handle> _accelHandle;
		unsigned idx = 0;
		xtasks_acc_handle getHandle()
		{
			return _accelHandle[(idx++)%_accelHandle.size()]; 
		}
	};
	std::unordered_map<uint64_t, _fpgaAccel> _inner_accelerators;


	inline void generateDeviceEvironment(Task *task) override;

	inline void finishTaskCleanup([[maybe_unused]] Task *task) override{}

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;


	AcceleratorStream::activatorReturnsChecker copy_in(void *dst, void *src, size_t size, void* copy_extra) override;
    AcceleratorStream::activatorReturnsChecker copy_out(void *dst, void *src, size_t size, void* copy_extra) override;
    AcceleratorStream::activatorReturnsChecker copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* copy_extra) override;


public:
	FPGAAccelerator(int fpgaDeviceIndex);
	
	std::pair<void *, bool> accel_allocate(size_t size) override;

	void accel_free(void* ptr) override;


};

#endif // FPGA_ACCELERATOR_HPP
