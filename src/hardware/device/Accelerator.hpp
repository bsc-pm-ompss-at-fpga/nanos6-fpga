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

	thread_local static Task *_currentTask;


protected:
	// Used also to denote the device number
	int _deviceHandler;
	nanos6_device_t _deviceType;
	MemoryPlace *_memoryPlace;
	ComputePlace *_computePlace;
	AcceleratorStreamPool _streamPool;

	int _directoryHandler;
	uintptr_t        _pageSize = 0x1000;

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
				while (_streamPool.streamAvailable()) {
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
	virtual inline void preRunTask(Task *task)
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

	virtual void callBody(Task *task) = 0;

	virtual void finishTask(Task *task);

public:
	virtual ~Accelerator()
	{
		delete _computePlace;
		delete _memoryPlace;
	}

	void initializeService();

	void shutdownService();

	int getDirectoryHandler() const
	{
		return _directoryHandler;
	}


	void setDirectoryHandler(int directoryHandler)
	{
		_directoryHandler = directoryHandler;
	}

	//this function performs a copy from a host address space into the accelerator
	virtual AcceleratorStream::activatorReturnsChecker copy_in([[maybe_unused]] void *dst, [[maybe_unused]]void *src, [[maybe_unused]] size_t size,[[maybe_unused]] Task *) {return []{return []{return true;};};}

	//this functions performs a copy from the accelerator address space to host memory
	virtual AcceleratorStream::activatorReturnsChecker copy_out([[maybe_unused]] void * dst,[[maybe_unused]] void * src,[[maybe_unused]] size_t size,[[maybe_unused]] Task *) {return []{return []{return true;};};}

	//this functions performs a copy from two accelerators that can share it's data without the host intervention
	virtual AcceleratorStream::activatorReturnsChecker copy_between
	([[maybe_unused]] void * dst, [[maybe_unused]] int dst_device_handler, [[maybe_unused]] void * src, [[maybe_unused]] int src_device_handler, [[maybe_unused]] size_t size, [[maybe_unused]] Task *) {return []{return []{return true;};};}

	void setDirectoryHandle(int handle)
	{
		_directoryHandler = handle;

	}

	std::shared_ptr<DeviceAllocation>
	createNewDeviceAllocation(const DataAccessRegion &region)
	{
		if(getDeviceType() == nanos6_host_device)
			return std::make_shared<DeviceAllocation>(
				DataAccessRegion((void *)region.getStartAddress(), (void *)region.getStartAddress()), 
				DataAccessRegion((void *)region.getStartAddress(), (void *)region.getStartAddress()),
				[](void *) {});
		
		const uintptr_t page_down = get_page(region.getStartAddress());
		const uintptr_t page_up =
			get_page_up((uintptr_t)(region.getEndAddress()) + 1);

		void *ptr = accel_allocate(page_up - page_down);
		if (ptr == nullptr) {
			return nullptr;
		}
		const DataAccessRegion host =
			DataAccessRegion((void *)page_down, (void *)page_up);
		const DataAccessRegion device = DataAccessRegion(
			(void *)ptr, (void *)(((uintptr_t)ptr) + page_up - page_down));

		return std::make_shared<DeviceAllocation>(host, device, [&](void *f) {
			setActiveDevice();
			accel_free(f);
		});
		;
	}


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

virtual void accel_free(void *)
{
}

virtual AcceleratorEvent *createEvent(std::function<void((AcceleratorEvent *))> onCompletion = [](AcceleratorEvent *) {})
{
	return new AcceleratorEvent(onCompletion);
}


virtual void *accel_allocate(size_t size)
{
	static uintptr_t fake_offset = 0x1000;
	uintptr_t fake = fake_offset;
	fake_offset+=size;
	return (void*) fake;
}

	virtual void destroyEvent(AcceleratorEvent *event)
	{
		delete event;
	}
	
private:
	static void serviceFunction(void *data);

	static void serviceCompleted(void *data);


	


	template <class T>
	uintptr_t get_page(T addr)
	{
		uintptr_t u_addr = (uintptr_t)addr;
		uintptr_t u_page_mask = ~(_pageSize - 1);
		return (u_addr & u_page_mask);
	}

	//helper for getting the beginning of the next page of the current address
	template <class T>
	uintptr_t get_page_up(T addr)
	{
		uintptr_t u_addr = (uintptr_t)addr;
		return (u_addr % _pageSize) ? get_page(u_addr + _pageSize) : get_page(u_addr);
	}
};

#endif // ACCELERATOR_HPP
