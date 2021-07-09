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
				std::cout<<"Submit task"<<std::endl;
				if (xtasksSubmitTask(task->getDeviceEnvironment().fpga.taskHandle)!= XTASKS_SUCCESS)
				{
					abort();
				}
				return [=]()->bool{
					xtasks_task_handle hand;
					xtasks_task_id tid;
					while(xtasksTryGetFinishedTask(&hand, &tid) == XTASKS_SUCCESS)
					{
						std::cout<<"task has finished"<<std::endl;
						xtasksDeleteTask(&hand);
						Task* _task  = (Task*) tid;
						_task->getDeviceEnvironment().fpga.taskFinished=true;
					}
					return task->getDeviceEnvironment().fpga.taskFinished;
				};
			}
		);
	else abort();
}

void FPGAAccelerator::preRunTask(Task *task)
{
	if (DeviceDirectoryInstance::useDirectory)
			DeviceDirectoryInstance::instance->register_regions(task);
		else abort();
}



AcceleratorStream::activatorReturnsChecker FPGAAccelerator::copy_in(void *dst, void *src, size_t size, [[maybe_unused]]  Task *task)
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
AcceleratorStream::activatorReturnsChecker FPGAAccelerator::copy_out(void *dst, void *src, size_t size, [[maybe_unused]] Task *task) 
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
	[[maybe_unused]] Task *task)
{
	return []() -> AcceleratorStream::checker
	{
		return []() -> bool{return true;}; 
	};
}


