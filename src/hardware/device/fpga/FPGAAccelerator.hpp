/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_ACCELERATOR_HPP
#define FPGA_ACCELERATOR_HPP

#include <list>

#include "FPGAFunctions.hpp"
#include "hardware/device/Accelerator.hpp"
#include "support/config/ConfigVariable.hpp"
#include "tasks/Task.hpp"

class FPGAAccelerator : public Accelerator {
private:
	bool _supports_async;

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
	FPGAAccelerator(int cudaDeviceIndex) :
		Accelerator(cudaDeviceIndex,
			nanos6_cuda_device,
			ConfigVariable<uint32_t>("devices.fpga.streams"),
			ConfigVariable<size_t>("devices.fpga.polling.period_us"),
			ConfigVariable<bool>("devices.fpga.polling.pinned")),
			_supports_async(false)
	{

	}

	~FPGAAccelerator()
	{
	}

	

	
	void *accel_allocate(size_t size) override
	{
		return FPGAFunctions::malloc(size);
	}

	void accel_free(void* ptr) override
	{
		return FPGAFunctions::free(ptr);
	}
	


};

#endif // FPGA_ACCELERATOR_HPP
