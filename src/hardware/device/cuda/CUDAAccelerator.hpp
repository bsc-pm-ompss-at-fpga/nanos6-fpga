/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2023 Barcelona Supercomputing Center (BSC)
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
	// Maximum number of kernel args before we allocate extra memory for them
	const static int MAX_STACK_ARGS = 16;

	// Name to not confuse with other more general events hadled in other portions of the runtime
	struct CUDAEvent {
		cudaEvent_t event;
		Task *task;
	};

	std::list<CUDAEvent> _activeEvents, _preallocatedEvents;
	cudaDeviceProp _deviceProperties;
	CUDAStreamPool _cudaStreamPool;

	int _cudaDeviceId;

	// Whether the task data dependencies should be prefetched to the device
	static ConfigVariable<bool> _prefetchDataDependencies;

	// To be used in order to obtain the current task in nanos6_get_current_cuda_stream() call
	thread_local static Task *_currentTask;

	inline void generateDeviceEvironment(DeviceEnvironment& env, [[maybe_unused]] uint64_t deviceSubtType) override
	{
		// The Accelerator::runTask() function has already set the device so it's safe to proceed
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;
		env.stream = _streamPool.getCUDAStream();
		env.event = _streamPool.getCUDAEvent();
	}

	inline void finishTaskCleanup(Task *task) override
	{
		nanos6_cuda_device_environment_t &env = task->getDeviceEnvironment().cuda;
		_streamPool.releaseCUDAEvent(env.event);
		_streamPool.releaseCUDAStream(env.stream);
	}

	void processCUDAEvents();

	void preRunTask(Task *task) override;

	void callBody(Task *task) override;

	void postRunTask(Task *task) override;

	void callTaskBody(Task *task, nanos6_address_translation_entry_t *translation);

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

	static inline Task *getCurrentTask()
	{
		return _currentTask;
	}
};

#endif // CUDA_ACCELERATOR_HPP
