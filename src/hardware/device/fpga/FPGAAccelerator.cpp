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
		_supports_async(ConfigVariable<bool>("devices.fpga.real_async"))
{

	size_t deviceCount = 0, handlesCount=0;

	FatalErrorHandler::failIf(
		xtasksGetNumAccs(&deviceCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get number of accelerators"
	);

	std::vector<xtasks_acc_info> info(deviceCount);
	std::vector<xtasks_acc_handle> handles(deviceCount);

	FatalErrorHandler::failIf(
		xtasksGetAccs(deviceCount, &handles[0], &handlesCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get the accelerators"
	);

	for(size_t i=0; i<deviceCount;++i)
	{
		xtasksGetAccInfo(handles[i], &info[i]);
		_inner_accelerators[info[i].type]._accelHandle.push_back(handles[i]);
	}
}



inline void FPGAAccelerator::generateDeviceEvironment(Task *task) 
{
	xtasks_acc_handle accelerator = _inner_accelerators[task->getDeviceSubType()].getHandle();
	xtasks_task_id parent = 0;
	xtasksCreateTask((xtasks_task_id) task, accelerator, parent, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &task->getDeviceEnvironment().fpga.taskHandle);
	task->getDeviceEnvironment().fpga.taskFinished = false;
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


void FPGAAccelerator::callBody(Task *task)
{
	if(DeviceDirectoryInstance::useDirectory)
		task->getAcceleratorStream()->addOperation(
			[task = task]() -> AcceleratorStream::checker 
			{ 
				Accelerator::setCurrentTask(task);
				task->body(&task->_symbolTranslations[0]);

				FatalErrorHandler::failIf(
					xtasksSubmitTask(task->getDeviceEnvironment().fpga.taskHandle)!= XTASKS_SUCCESS,
					"Xtasks: Submit Task failed"
				);

				return [=]()->bool{
					xtasks_task_handle hand;
					xtasks_task_id tid;
					while(xtasksTryGetFinishedTask(&hand, &tid) == XTASKS_SUCCESS)
					{
						xtasksDeleteTask(&hand);
						Task* _task  = (Task*) tid;
						_task->getDeviceEnvironment().fpga.taskFinished=true;
					}
					return task->getDeviceEnvironment().fpga.taskFinished;
				};
			}
		);
	else FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");

}

void FPGAAccelerator::preRunTask(Task *task)
{
	if (DeviceDirectoryInstance::useDirectory)
			DeviceDirectoryInstance::instance->register_regions(task);
		else FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");
}



AcceleratorStream::activatorReturnsChecker FPGAAccelerator::copy_in(void *dst, void *src, size_t size, [[maybe_unused]]  void *task)
{

	if(_supports_async)
		{
			return [=]() -> AcceleratorStream::checker
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_HOST_TO_ACC);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
		}
	else
	{
		return [=]() -> AcceleratorStream::checker
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
}

//this functions performs a copy from the accelerator address space to host memory
AcceleratorStream::activatorReturnsChecker FPGAAccelerator::copy_out(void *dst, void *src, size_t size, [[maybe_unused]] void *task) 
{
	if(_supports_async)
		{
			return [=]() -> AcceleratorStream::checker
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_ACC_TO_HOST);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
		}
	else
	{
		return [=]() -> AcceleratorStream::checker
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

	}
}

//this functions performs a copy from two accelerators that can share it's data without the host intervention
AcceleratorStream::activatorReturnsChecker FPGAAccelerator::copy_between(
	[[maybe_unused]] void *dst,
	[[maybe_unused]] int dstDevice, 
	[[maybe_unused]] void *src, 
	[[maybe_unused]] int srcDevice,
	[[maybe_unused]] size_t size,
	[[maybe_unused]] void *task)
{
	return []() -> AcceleratorStream::checker
	{
		return []() -> bool{return true;}; 
	};
}


