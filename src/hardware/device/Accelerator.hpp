/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_HPP
#define ACCELERATOR_HPP

#include "scheduling/Scheduler.hpp"
#include "dependencies/SymbolTranslation.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "AcceleratorStreamPool.hpp"
#include "AcceleratorEvent.hpp"
#include "tasks/Task.hpp"


// The Accelerator class should be used *per physical device*,
// to denote a separate address space. Accelerators may have
// sub-devices, if applicable, for instance in FPGAs, that
// virtual partitions of the FPGA may share the address space.
// With GPUs, each physical GPU gets its Accelerator instance.

class Accelerator {
private:
	std::atomic<bool> _stopService;
	std::atomic<bool> _finishedService;

	thread_local static Task* _currentTask;


protected:
	// Used also to denote the device number
	int _deviceHandler;
	nanos6_device_t _deviceType;
	MemoryPlace *_memoryPlace;
	ComputePlace *_computePlace;
	AcceleratorStreamPool       _streamPool;

	size_t _pollingPeriodUs;
	bool _isPinnedPolling;

	Accelerator(int handler, nanos6_device_t type, uint32_t numOfStreams, size_t pollingPeriodUs, bool isPinnedPolling) :
		_stopService(false),
		_finishedService(false),
		_deviceHandler(handler),
		_deviceType(type),
		_memoryPlace(new MemoryPlace(_deviceHandler, _deviceType)),
		_computePlace(new ComputePlace(_deviceHandler, _deviceType)),
		_streamPool(numOfStreams),
		_pollingPeriodUs(pollingPeriodUs),
		_isPinnedPolling(isPinnedPolling)
	{
		_computePlace->addMemoryPlace(_memoryPlace);
	}

	inline bool shouldStopService() const
	{
		return _stopService.load(std::memory_order_relaxed);
	}

	// Set the current instance as the selected/active device for subsequent operations
	virtual void setActiveDevice() = 0;


	void acceleratorServiceLoop()
	{
		while (!shouldStopService()) {
			setActiveDevice();
			do {
				// Launch as many ready device tasks as possible
				while (_streamPool.streamAvailable())
				{
					Task *task = Scheduler::getReadyTask(_computePlace);
					if (task == nullptr)
						break;

					runTask(task);
				}

				_streamPool.processStreams();

				// Iterate while there are running tasks and pinned polling is enabled
			} while (_isPinnedPolling && _streamPool.ongoingStreams());

			// Sleep for a configured amount of microseconds
			BlockingAPI::waitForUs(_pollingPeriodUs);
		}
	}


	// Each device may use these methods to prepare or conclude task launch if needed
	virtual inline void preRunTask(Task *)
	{
	}

	virtual inline void postRunTask(Task *)
	{
	}

	// The main device task launch method; It will call pre- & postRunTask
	virtual void runTask(Task *task);

	// Device specific operations after task completion may go here (e.g. free environment)
	virtual inline void finishTaskCleanup(Task *)
	{
	}

	// Generate the appropriate device_env pointer Mercurium uses for device tasks
	virtual inline void generateDeviceEvironment(Task *)
	{
	}

	virtual void callBody(Task * task) = 0;

	virtual void finishTask(Task *task);

public:
	virtual ~Accelerator()
	{
		delete _computePlace;
		delete _memoryPlace;
	}

	void initializeService();

	void shutdownService();

	inline MemoryPlace *getMemoryPlace()
	{
		return _memoryPlace;
	}

	inline ComputePlace *getComputePlace()
	{
		return _computePlace;
	}

	inline nanos6_device_t getDeviceType() const
	{
		return _deviceType;
	}

	inline int getDeviceHandler() const
	{
		return _deviceHandler;
	}

	// We use FIFO queues that we launch the tasks on. These are used to check
	// for task completion or to enqueue related operations (in the case of
	// the Cache-Directory).  The actual object type is device-specific: In
	// CUDA, this will be a cudaStream_t, in OpenACC an asynchronous queue,
	// in FPGA a custom FPGA stream, etc.

	// This call requests an available FIFO from the runtime and returns a pointer to it.
	virtual void *getAsyncHandle() = 0;

	// Return the FIFO for re-use after task has finished.
	virtual void releaseAsyncHandle(void *asyncHandle) = 0;

	static inline Task *getCurrentTask()
	{
		return _currentTask;
	}


private:
	static void serviceFunction(void *data);

	static void serviceCompleted(void *data);

	virtual AcceleratorEvent* createEvent(std::function<void((AcceleratorEvent*))> onCompletion = [](AcceleratorEvent*){})
	{
		return new AcceleratorEvent(onCompletion);
	};
	virtual void destroyEvent(AcceleratorEvent* event)
	{
		delete event;
	}

};

#endif // ACCELERATOR_HPP
