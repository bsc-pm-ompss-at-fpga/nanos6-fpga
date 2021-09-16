/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "FPGAAccelerator.hpp"
#include "../directory/DeviceDirectory.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "scheduling/Scheduler.hpp"
#include "system/BlockingAPI.hpp"

#include <DataAccessRegistration.hpp>
#include <DataAccessRegistrationImplementation.hpp>
#include "lowlevel/FatalErrorHandler.hpp"


FPGAAccelerator::FPGAAccelerator(int fpgaDeviceIndex) :
	Accelerator(fpgaDeviceIndex,
		nanos6_fpga_device,
		ConfigVariable<uint32_t>("devices.fpga.streams"),
		ConfigVariable<size_t>("devices.fpga.polling.period_us"),
		ConfigVariable<bool>("devices.fpga.polling.pinned")),
        _allocator(fpgaDeviceIndex)
{
	std::string memSyncString = ConfigVariable<std::string>("devices.fpga.mem_sync_type");
	if (memSyncString == "async") {
		_mem_sync_type = REAL_ASYNC;
	}
	else if (memSyncString == "forced async") {
		_mem_sync_type = FORCED_ASYNC;
	}
	else if (memSyncString == "sync") {
		_mem_sync_type = SYNC;
	}
	else {
		FatalErrorHandler::fail("Config value", memSyncString, " is not valid for devices.fpga.mem_sync_type");
	}
    size_t accCount = 0, handlesCount=0;

	FatalErrorHandler::failIf(
        xtasksGetNumAccs(&accCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get number of accelerators"
	);

    std::vector<xtasks_acc_info> info(accCount);
    std::vector<xtasks_acc_handle> handles(accCount);

	FatalErrorHandler::failIf(
        xtasksGetAccs(accCount, &handles[0], &handlesCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get the accelerators"
	);

    for(size_t i=0; i<accCount;++i)
	{
		xtasksGetAccInfo(handles[i], &info[i]);
		_inner_accelerators[info[i].type]._accelHandle.push_back(handles[i]);
	}
}

inline void FPGAAccelerator::generateDeviceEvironment(DeviceEnvironment& env, uint64_t deviceSubtypeId) {
#ifndef NDEBUG
	FatalErrorHandler::failIf(
		_inner_accelerators.find(deviceSubtypeId) == _inner_accelerators.end(),
		"Device subtype ", deviceSubtypeId, " not found"
	);
#endif
    xtasks_acc_handle accelerator = _inner_accelerators[deviceSubtypeId].getHandle();
    xtasks_task_id parent = 0;
	xtasksCreateTask((xtasks_task_id) &env, accelerator, parent, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &env.fpga.taskHandle);
	env.fpga.taskFinished = false;
}

std::pair<void *, bool> FPGAAccelerator::accel_allocate(size_t size) 
{
	return _allocator.allocate(size);
}

void FPGAAccelerator::accel_free(void* ptr)
{
	_allocator.free(ptr);
}

void FPGAAccelerator::postRunTask(Task *)
{
}

void FPGAAccelerator::submitDevice(const DeviceEnvironment &deviceEnvironment) const {
    FatalErrorHandler::failIf(
        xtasksSubmitTask(getDeviceHandler(), deviceEnvironment.fpga.taskHandle)!= XTASKS_SUCCESS,
        "Xtasks: Submit Task failed"
    );
}

bool FPGAAccelerator::checkDeviceSubmissionFinished(const DeviceEnvironment& deviceEnvironment) const {
    xtasks_task_handle hand;
    xtasks_task_id tid;
    while(xtasksTryGetFinishedTask(&hand, &tid) == XTASKS_SUCCESS)
    {
        xtasksDeleteTask(&hand);
        DeviceEnvironment* env  = (DeviceEnvironment*) tid;
        env->fpga.taskFinished = true;
    }
    return deviceEnvironment.fpga.taskFinished;
}

void FPGAAccelerator::callBody(Task *task)
{
    if(DeviceDirectoryInstance::useDirectory) {
		task->getAcceleratorStream()->addOperation(
			[task, env = &task->getDeviceEnvironment(), handler = getDeviceHandler()]() -> std::function<bool(void)>
			{ 
				task->bodyWithInternalTranslation();

				FatalErrorHandler::failIf(
					xtasksSubmitTask(handler, task->getDeviceEnvironment().fpga.taskHandle)!= XTASKS_SUCCESS,
					"Xtasks: Submit Task failed"
				);

				return [=]() -> bool {
					xtasks_task_handle hand;
					xtasks_task_id tid;
					while(xtasksTryGetFinishedTask(&hand, &tid) == XTASKS_SUCCESS)
					{
						xtasksDeleteTask(&hand);
						DeviceEnvironment* _env = (DeviceEnvironment*) tid;
						_env->fpga.taskFinished = true;
					}
					return env->fpga.taskFinished;
				};
			}
		);
    }
    else FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");
}

void FPGAAccelerator::preRunTask(Task *task)
{
	if (DeviceDirectoryInstance::useDirectory)
			DeviceDirectoryInstance::instance->register_regions(task);
		else FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");
}

std::function<std::function<bool(void)>()> FPGAAccelerator::copy_in(void *dst, void *src, size_t size, [[maybe_unused]]  void *task) const
{

	if(_mem_sync_type == REAL_ASYNC)
		{
			return [=]() -> std::function<bool(void)>
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_HOST_TO_ACC);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
		}
	else if (_mem_sync_type == FORCED_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
		{

			bool* copy_finished_flag = new bool;
			*copy_finished_flag=false;
			auto do_copy = [](void* t)
			{  
				std::function<void()>* fn = (std::function<void()>*) t;
				(*fn)();
				delete fn;
			};

			auto copy  = new std::function<void()>([=](){_allocator.memcpy(dst,src,size,XTASKS_HOST_TO_ACC);});

			auto finish_copy = [](void* flag){ bool* ff = (bool*) flag; *ff=true;};

			SpawnFunction::spawnFunction(do_copy, (void*) copy, finish_copy, (void*) copy_finished_flag,"CopyInFPGA", false);
			return [=]() -> bool
			{
				if(*copy_finished_flag)
				{
					delete copy_finished_flag;
					return true;
				}
				return false;
				//IF FINISHED FLAG -> CONTINUE
			};
		};
	}
	else {
		return [&, dst, src, size]() -> std::function<bool(void)> {
			return [&, dst, src, size]() -> bool {
				_allocator.memcpy(dst, src, size, XTASKS_HOST_TO_ACC);
				return true;
			};
		};
	}
}

//this functions performs a copy from the accelerator address space to host memory
std::function<std::function<bool(void)>()> FPGAAccelerator::copy_out(void *dst, void *src, size_t size, [[maybe_unused]] void *task) const
{
	if(_mem_sync_type == REAL_ASYNC)
		{
			return [=]() -> std::function<bool(void)>
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_ACC_TO_HOST);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
		}
	else if (_mem_sync_type == FORCED_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
		{

			bool* copy_finished_flag = new bool;
			*copy_finished_flag=false;
			auto do_copy = [](void* t)
			{  
				std::function<void()>* fn = (std::function<void()>*) t;
				(*fn)();
				delete fn;
			};

			auto copy  = new std::function<void()>([=](){_allocator.memcpy(dst,src,size,XTASKS_ACC_TO_HOST);});

			auto finish_copy = [](void* flag){ bool* ff = (bool*) flag; *ff=true;};

			SpawnFunction::spawnFunction(do_copy, (void*) copy, finish_copy, (void*) copy_finished_flag,"CopyInFPGA", false);
			return [=]() -> bool
			{
				if(*copy_finished_flag)
				{
					delete copy_finished_flag;
					return true;
				}
				return false;
				//IF FINISHED FLAG -> CONTINUE
			};
		};
	} else {
		return [&, dst, src, size]() -> std::function<bool(void)> {
			return [&, dst, src, size]() -> bool {
				_allocator.memcpy(dst, src, size, XTASKS_ACC_TO_HOST);
				return true;
			};
		};
	}
}

//this functions performs a copy from two accelerators that can share their data without the host intervention
std::function<std::function<bool(void)>()> FPGAAccelerator::copy_between(
	[[maybe_unused]] void *dst,
	[[maybe_unused]] int dstDevice, 
	[[maybe_unused]] void *src, 
	[[maybe_unused]] int srcDevice,
	[[maybe_unused]] size_t size,
    [[maybe_unused]] void *task) const
{
	return []() -> std::function<bool(void)>
	{
		return []() -> bool{return true;}; 
	};
}
