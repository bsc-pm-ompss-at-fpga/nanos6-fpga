/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2022 Barcelona Supercomputing Center (BSC)
*/

#include <algorithm>
#include <array>

#include "CUDAAccelerator.hpp"
#include "../directory/DeviceDirectory.hpp"
#include "CUDAAcceleratorEvent.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "scheduling/Scheduler.hpp"
#include "system/BlockingAPI.hpp"

#include <DataAccessRegistration.hpp>
#include <DataAccessRegistrationImplementation.hpp>


// For each CUDA device task a CUDA stream is required for the asynchronous
// launch; To ensure kernel completion a CUDA event is 'recorded' on the stream
// right after the kernel is queued. Then when a cudaEventQuery call returns
// succefully, we can be sure that the kernel execution (and hence the task)
// has finished.

// Get a new CUDA event and queue it in the stream the task has launched
void CUDAAccelerator::postRunTask(Task *)
{
}


thread_local Task *CUDAAccelerator::_currentTask;

void CUDAAccelerator::destroyEvent(AcceleratorEvent *event)
{
	delete (CUDAAcceleratorEvent *)event;
}


void CUDAAccelerator::callBody(Task *task)
{

	task->getAcceleratorStream()->addOperation([task]
	{
		task->body(&task->_symbolTranslations[0]);
		return true;
	});

}

void CUDAAccelerator::preRunTask(Task *task)
{

	if(DeviceDirectoryInstance::useDirectory)
	{
		DeviceDirectoryInstance::instance->register_regions(task);
	}
	else
	{
		// Prefetch available memory locations to the GPU
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;

		DataAccessRegistration::processAllDataAccesses(task,
			[&](const DataAccess *access) -> bool {
				if (access->getType() != REDUCTION_ACCESS_TYPE && !access->isWeak()) {
					CUDAFunctions::cudaDevicePrefetch(
						access->getAccessRegion().getStartAddress(),
						access->getAccessRegion().getSize(),
						_deviceHandler, env.stream,
						access->getType() == READ_ACCESS_TYPE);
				}
				return true;
			});
	}
}



std::function<std::function<bool(void)>()> CUDAAccelerator::copy_in(void *dst, void *src, size_t size, void* task) const
{
	nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;
	CUDAFunctions::recordEvent(env.event, env.stream);
	_activeEvents.push_back({env.event, task});
}

//this functions performs a copy from the accelerator address space to host memory
std::function<std::function<bool(void)>()> CUDAAccelerator::copy_out(void *dst, void *src, size_t size, void* task) const
{
	// set the thread_local static var to be used by nanos6_get_current_cuda_stream()
	CUDAAccelerator::_currentTask = task;

	// Prefetch available memory locations to the GPU
	nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;

	DataAccessRegistration::processAllDataAccesses(task,
		[&](const DataAccess *access) -> bool {
			if (access->getType() != REDUCTION_ACCESS_TYPE && !access->isWeak()) {
				CUDAFunctions::cudaDevicePrefetch(
					access->getAccessRegion().getStartAddress(),
					access->getAccessRegion().getSize(),
					_deviceHandler, env.stream,
					access->getType() == READ_ACCESS_TYPE);
			}
			return true;
		});
}

//this functions performs a copy from two accelerators that can share it's data without the host intervention
std::function<std::function<bool(void)>()> CUDAAccelerator::copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* task) const
{
	if (task == nullptr) return [=]() -> std::function<bool(void)>
	{
			 cudaMemcpyPeer(dst,dstDevice,src,srcDevice,size);
			 return []()->bool {return true;};
	};
	else return [=]() -> std::function<bool(void)>
	{
		cudaMemcpyPeerAsync(dst,dstDevice,src,srcDevice,size, ((Task*)task)->getDeviceEnvironment().cuda.stream);
		return []()-> bool{return true;};
	};
}


void CUDAAccelerator::callTaskBody(Task *task, nanos6_address_translation_entry_t *translationTable)
{
	if (task->getTaskInfo()->implementations[0].device_function_name == nullptr) {
		task->body(translationTable);
	} else {
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;

		void *args = task->getArgsBlock();
		nanos6_device_info_t &deviceInfo = *((nanos6_device_info_t *)args);

		// NDRANGE is used to define the work elements of the kernel
		// This can be 1D, 2D or 3D.
		// A grid contains blocks, which are the basic unit of parallelism.
		// If the parameters given by the user are invalid for our cuda capabilities,
		// we COULD perform some math to express the same working units in a supported way.
		// Right now we expect the user to provide valid parameters.

		size_t gridDim1 = std::max((int64_t) deviceInfo.sizes[0], (int64_t) 1);
		size_t gridDim2 = std::max((int64_t) deviceInfo.sizes[1], (int64_t) 1);
		size_t gridDim3 = std::max((int64_t) deviceInfo.sizes[2], (int64_t) 1);

		size_t blockDim1 = std::max((int64_t) deviceInfo.sizes[3], (int64_t) 1);
		size_t blockDim2 = std::max((int64_t) deviceInfo.sizes[4], (int64_t) 1);
		size_t blockDim3 = std::max((int64_t) deviceInfo.sizes[5], (int64_t) 1);

		std::array<void *, MAX_STACK_ARGS> stack_params;
		void **params = &stack_params[0];
		nanos6_task_info_t *taskInfo = task->getTaskInfo();
		int numArgs = taskInfo->num_args;

		if (numArgs > MAX_STACK_ARGS)
			params = (void **)MemoryAllocator::alloc(numArgs * sizeof(void *));

		for (int i = 0; i < numArgs; i++) {
			params[i] = (void *)((char *)args + taskInfo->offset_table[i]);
		}

		if (translationTable) {
			for (int i = 0; i < taskInfo->num_symbols; ++i) {
				int arg = taskInfo->arg_idx_table[i];
				// Translate corresponding param
				if (arg >= 0 && translationTable[i].device_address != 0) {
					// params[arg] is a void * which actually points to the location of the argument
					// What we want to translate is the argument itself, so we will have to dereference it
					uintptr_t *argument = (uintptr_t *)params[arg];
					*argument = *argument - translationTable[i].local_address + translationTable[i].device_address;
				}
			}
		}

		CUresult execution_result = cuLaunchKernel(
			CUDAFunctions::loadFunction(task->getTaskInfo()->implementations[0].device_function_name),
			gridDim1, gridDim2, gridDim3,
			blockDim1, blockDim2, blockDim3,
			deviceInfo.shm_size,
			env.stream,
			params,
			nullptr);

		if (execution_result != CUDA_SUCCESS) {
			const char *err_str = nullptr;
			cuGetErrorString(execution_result, &err_str);

			fprintf(stderr, "Error launching kernel: %s with error: %s \n launch config is: block[%zu %zu %zu] grid[%zu %zu %zu] shmem[%zu]\n",
				task->getTaskInfo()->implementations[0].device_function_name, err_str,
				blockDim1, blockDim2, blockDim3, gridDim1, gridDim2, gridDim3, deviceInfo.shm_size);
			FatalErrorHandler::fail("Failed to execute cuda kernel: ", task->getTaskInfo()->implementations[0].device_function_name);
		}

		// Here we un-translate the arguments, in case this task needs to be realunched at some point
		if (translationTable) {
			for (int i = 0; i < taskInfo->num_symbols; ++i) {
				int arg = taskInfo->arg_idx_table[i];
				if (arg >= 0 && translationTable[i].device_address != 0) {
					uintptr_t *argument = (uintptr_t *)params[arg];
					*argument = *argument - translationTable[i].device_address + translationTable[i].local_address;
				}
			}
		}

		if (numArgs > MAX_STACK_ARGS)
			MemoryAllocator::free((void *)params, numArgs * sizeof(void *));
	}
}
