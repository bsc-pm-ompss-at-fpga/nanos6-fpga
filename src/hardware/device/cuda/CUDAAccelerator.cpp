/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "CUDAFunctions.hpp"
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


AcceleratorEvent *CUDAAccelerator::createEvent(std::function<void((AcceleratorEvent *))> onCompletion = [](AcceleratorEvent *) {})
{
	nanos6_cuda_device_environment_t &env = getCurrentTask()->getDeviceEnvironment().cuda;

	return (AcceleratorEvent *)new CUDAAcceleratorEvent(onCompletion, env.stream, _cudaStreamPool);
}

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
	if (task == nullptr) return [=]() -> std::function<bool(void)> 
	{
			setActiveDevice();//since this can be processed by a host-task or taskwait
			CUDAFunctions::memcpyAsync(dst, src, size, cudaMemcpyKind::cudaMemcpyHostToDevice, _cudaCopyStream);
			return [=]() -> bool {
				setActiveDevice();
				return cudaStreamQuery(_cudaCopyStream) == cudaSuccess; //since there is no gpu task associated, we do it this way
			};
	};
	else return [=]() -> std::function<bool(void)> {
		CUDAFunctions::memcpyAsync(dst, src, size, cudaMemcpyKind::cudaMemcpyHostToDevice, ((Task*)task)->getDeviceEnvironment().cuda.stream);
		return []() -> bool { return true; };
	};
}

//this functions performs a copy from the accelerator address space to host memory
std::function<std::function<bool(void)>()> CUDAAccelerator::copy_out(void *dst, void *src, size_t size, void* task) const
{
	if (task == nullptr) return [=]() -> std::function<bool(void)> {
			setActiveDevice();//since this can be processed by a host-task or taskwait (in a future)
			CUDAFunctions::memcpyAsync(dst, src, size, cudaMemcpyKind::cudaMemcpyDeviceToHost, _cudaCopyStream);
			return [=]() -> bool {
				setActiveDevice();
				return cudaStreamQuery(_cudaCopyStream) == cudaSuccess; //since there is no gpu task associated, we do it this way
			};
		};
	else return [=]() -> std::function<bool(void)> {
		CUDAFunctions::memcpyAsync(dst, src, size, cudaMemcpyKind::cudaMemcpyDeviceToHost, ((Task*)task)->getDeviceEnvironment().cuda.stream);
		return []() -> bool { return true; };
	};
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

