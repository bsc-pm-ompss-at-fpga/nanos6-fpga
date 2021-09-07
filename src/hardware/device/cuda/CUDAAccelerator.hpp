/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef CUDA_ACCELERATOR_HPP
#define CUDA_ACCELERATOR_HPP

#include <list>

#include <nanos6/cuda_device.h>

#include "CUDAFunctions.hpp"
#include "CUDAStreamPool.hpp"
#include "hardware/device/Accelerator.hpp"
#include "support/config/ConfigVariable.hpp"
#include "tasks/Task.hpp"

class CUDAAccelerator : public Accelerator {
private:
	// Name to not confuse with other more general events hadled in other portions of the runtime
	struct CUDAEvent {
		cudaEvent_t event;
		Task *task;
	};

	std::list<CUDAEvent> _activeEvents, _preallocatedEvents;
	cudaDeviceProp _deviceProperties;
	CUDAStreamPool _cudaStreamPool;

	int _cudaDeviceId;

	// To be used in order to obtain the current task in nanos6_get_current_cuda_stream() call

	inline void generateDeviceEvironment(Task *task) override
	{
		// The Accelerator::runTask() function has already set the device so it's safe to proceed
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;
		env.stream = _cudaStreamPool.getCUDAStream();
	}

	inline void finishTaskCleanup(Task *task) override
	{
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;
		_cudaStreamPool.releaseCUDAStream(env.stream);
	}


	void processCUDAEvents();

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;

	cudaStream_t   _cudaCopyStream;


    std::function<std::function<bool(void)>()> copy_in(void *dst, void *src, size_t size, void* task) const override;
    std::function<std::function<bool(void)>()> copy_out(void *dst, void *src, size_t size, void* task) const override;
    std::function<std::function<bool(void)>()> copy_between(void *dst, int dstDevice, void *src, int srcDevice, size_t size, void* task) const override;


public:
	CUDAAccelerator(int cudaDeviceIndex) :
		Accelerator(cudaDeviceIndex,
			nanos6_cuda_device,
			ConfigVariable<uint32_t>("devices.cuda.streams"),
			ConfigVariable<size_t>("devices.cuda.polling.period_us"),
			ConfigVariable<bool>("devices.cuda.polling.pinned")),
		_cudaStreamPool(cudaDeviceIndex),
		_cudaDeviceId(cudaDeviceIndex),
		_cudaCopyStream(CUDAFunctions::createStream())

	{
		CUDAFunctions::getDeviceProperties(_deviceProperties, _deviceHandler);
	}

	~CUDAAccelerator()
	{
	}

	AcceleratorEvent *createEvent(std::function<void((AcceleratorEvent *))> onCompletion) override;
	
	std::pair<void *, bool> accel_allocate(size_t size) override
	{
		void * devalloc =  CUDAFunctions::malloc(size);
		//std::cout<<"CUDA Allocate: "<<devalloc<<std::endl;
		return {devalloc, devalloc!=nullptr};
	}

	void accel_free(void* ptr) override
	{
		//std::cout<<"CUDA Free: "<<ptr<<std::endl;
		CUDAFunctions::free(ptr);
	}

	void destroyEvent(AcceleratorEvent *event) override;
	
	// Set current device as the active in the runtime
    inline void setActiveDevice() const override
	{
		CUDAFunctions::setActiveDevice(_deviceHandler);
	}

    virtual int getVendorDeviceId() const override { return _cudaDeviceId;}

	// In CUDA, the async FIFOs used are CUDA streams
	inline void *getAsyncHandle() override
	{
		return (void *)_cudaStreamPool.getCUDAStream();
	}

	inline void releaseAsyncHandle(void *stream) override
	{
		_cudaStreamPool.releaseCUDAStream((cudaStream_t)stream);
	}
};

#endif // CUDA_ACCELERATOR_HPP
