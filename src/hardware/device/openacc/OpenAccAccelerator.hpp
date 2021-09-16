/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef OPENACC_ACCELERATOR_HPP
#define OPENACC_ACCELERATOR_HPP

#include <deque>

#include <nanos6/openacc_device.h>

#include "OpenAccFunctions.hpp"
#include "OpenAccQueuePool.hpp"
#include "hardware/device/Accelerator.hpp"
#include "support/config/ConfigVariable.hpp"

class OpenAccAccelerator : public Accelerator {
private:
	std::deque<OpenAccQueue *> _activeQueues;

	OpenAccQueuePool _queuePool;

	inline bool isQueueAvailable()
	{
		return _queuePool.isQueueAvailable();
	}

	// For OpenACC tasks, the environment actually contains just an int, which is the
	// *async* argument OpenACC expects. Mercurium reads the environment and converts
	// the found acc pragmas to e.g.:
	// from:	#pragma acc kernels
	// to:		#pragma acc kernels async(asyncId)
	inline void generateDeviceEvironment(DeviceEnvironment& env, [[maybe_unused]] uint64_t deviceSubType) override
	{
		nanos6_openacc_device_environment_t &openAccEnv = env.openacc;
		OpenAccQueue *queue = _queuePool.getAsyncQueue();
		// Use the deviceEnvironment to pass the queue object to further stages without having to
		// iterate through all queues to detect the task that has it.
		openAccEnv.queue = queue;
		openAccEnv.asyncId = queue->getQueueId();
	}

	inline void preRunTask(Task *task) override
	{
		OpenAccQueue *queue = (OpenAccQueue *)task->getDeviceEnvironment().openacc.queue;
		assert(queue != nullptr);
		queue->setTask(task);
	}

	inline void postRunTask(Task *task) override
	{
		OpenAccQueue *queue = (OpenAccQueue *)task->getDeviceEnvironment().openacc.queue;
		assert(queue != nullptr);
		_activeQueues.push_back(queue);
	}

	inline void finishTaskCleanup(Task *task) override
	{
		OpenAccQueue *queue = (OpenAccQueue *)task->getDeviceEnvironment().openacc.queue;
		_queuePool.releaseAsyncQueue(queue);
	}

	void acceleratorServiceLoop() override;

	void processQueues();

public:
	OpenAccAccelerator(int openaccDeviceIndex) :
		Accelerator(openaccDeviceIndex, nanos6_openacc_device,
					0, //numOfStreams, using OpenAccQueues instead of generic streams,
					ConfigVariable<size_t>("devices.openacc.polling.period_us"),
					ConfigVariable<bool>("devices.openacc.polling.pinned")),
		_queuePool()
	{
	}

	~OpenAccAccelerator()
	{
	}

	// Set current device as the active in the runtime
	inline void setActiveDevice() const override
	{
		OpenAccFunctions::setActiveDevice(_deviceHandler);
	}

	// In OpenACC, the async FIFOs used are asynchronous queues
	inline void *getAsyncHandle() override
	{
		return (void *)_queuePool.getAsyncQueue();
	}

	inline void releaseAsyncHandle(void *queue) override
	{
		_queuePool.releaseAsyncQueue((OpenAccQueue *)queue);
	}
};

#endif // OPENACC_ACCELERATOR_HPP

