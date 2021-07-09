/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_ACCELERATOR_HPP
#define FPGA_ACCELERATOR_HPP

#include <list>

#include "hardware/device/Accelerator.hpp"
#include "support/config/ConfigVariable.hpp"
#include "tasks/Task.hpp"
#include "src/memory/allocator/devices/FPGAPinnedAllocator.hpp"


class FPGAAccelerator : public Accelerator {
private:
	bool _supports_async;
	FPGAPinnedAllocator _allocator;

	struct _fpgaAccel
	{
		std::vector<xtasks_acc_handle> _accelHandle;
		unsigned idx;
		xtasks_acc_handle getHandle()
		{
			return _accelHandle[(idx++)%_accelHandle.size()]; 
		}
	};

	std::unordered_map<uint64_t, _fpgaAccel> _inner_accelerators;

	inline void generateDeviceEvironment(Task *task) override
	{
		xtasks_acc_handle accelerator = _inner_accelerators[task->getDeviceSubType()].getHandle();
		xtasks_task_id parent = 0;
		xtasksCreateTask((xtasks_task_id) task, accelerator, parent, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &task->getDeviceEnvironment().fpga.taskHandle);
		task->getDeviceEnvironment().fpga.taskFinished = false;
	}

	inline void finishTaskCleanup([[maybe_unused]] Task *task) override
	{

	}

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;


	AcceleratorStream::activatorReturnsChecker copy_in(void *dst, void *src, size_t size, Task* task) override;
    AcceleratorStream::activatorReturnsChecker copy_out(void *dst, void *src, size_t size, Task* task) override;
    AcceleratorStream::activatorReturnsChecker copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, Task* task) override;


public:
	FPGAAccelerator(int fpgaDeviceIndex) :
		Accelerator(fpgaDeviceIndex,
			nanos6_fpga_device,
			ConfigVariable<uint32_t>("devices.fpga.streams"),
			ConfigVariable<size_t>("devices.fpga.polling.period_us"),
			ConfigVariable<bool>("devices.fpga.polling.pinned")),
			_supports_async(ConfigVariable<bool>("devices.fpga.real_async"))
	{

		size_t _deviceCount = 0, _handlesCount=0;
		if(xtasksGetNumAccs(&_deviceCount) != XTASKS_SUCCESS) abort();

		int numAccel = _deviceCount;

		std::vector<xtasks_acc_info> info(numAccel);
		std::vector<xtasks_acc_handle> handles(numAccel);
		if(xtasksGetAccs(numAccel, &handles[0], &_handlesCount) != XTASKS_SUCCESS) abort();

		for(int i=0; i<numAccel;++i)
		{
			xtasksGetAccInfo(handles[i], &info[i]);
			_inner_accelerators[info[i].type]._accelHandle.push_back(handles[i]);
		}
	}

	~FPGAAccelerator()
	{
	}

	

	
	void *accel_allocate(size_t size) override
	{
		return _allocator.allocate(size);
	}

	void accel_free(void* ptr) override
	{
		_allocator.free(ptr);
	}
	


};

#endif // FPGA_ACCELERATOR_HPP
