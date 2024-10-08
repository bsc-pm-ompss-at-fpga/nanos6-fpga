/*
        This file is part of Nanos6 and is licensed under the terms contained in
   the COPYING file.

	Copyright (C) 2020-2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_HPP
#define ACCELERATOR_HPP

#include <functional>

#include "AcceleratorEvent.hpp"
#include "AcceleratorStreamPool.hpp"
#include "dependencies/SymbolTranslation.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "scheduling/Scheduler.hpp"
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

public:

	Accelerator(int handler, nanos6_device_t type, uint32_t numOfStreams,
		size_t pollingPeriodUs, bool isPinnedPolling);

	virtual void setActiveDevice() const = 0;
	virtual int getVendorDeviceId() const = 0;
protected:
	// Used also to denote the device number
	int _deviceHandler;
	nanos6_device_t _deviceType;
	MemoryPlace *_memoryPlace;
	ComputePlace *_computePlace;
	AcceleratorStreamPool _streamPool;

	int _directoryHandler;
	uintptr_t _pageSize = 0x1000;

	size_t _pollingPeriodUs;
	bool _isPinnedPolling;

	inline bool shouldStopService() const;
	// Set the current instance as the selected/active device for subsequent
	// operations

	virtual void acceleratorServiceLoop();

	// Each device may use these methods to prepare or conclude task launch if
	// needed
	virtual inline void preRunTask(Task *) {}

	virtual inline void postRunTask(Task *) {}

	// The main device task launch method; It will call pre- & postRunTask
	virtual void runTask(Task *);

	// Device specific operations after task completion may go here (e.g. free
	// environment)
	virtual inline void finishTaskCleanup(Task *) {}

	virtual void callBody(Task *) {}

	virtual void callTaskBody(Task *task, nanos6_address_translation_entry_t *);

	virtual void finishTask(Task *task);

public:
	virtual ~Accelerator() {
		delete _computePlace;
		delete _memoryPlace;
	}

	virtual void initializeService();

	virtual void shutdownService();

	int getDirectoryHandler() const;

	void setDirectoryHandler(int directoryHandler);

	// this function performs a copy from a host address space into the
	// accelerator
	virtual std::function<std::function<bool(void)>()>
	copy_in([[maybe_unused]] void *dst, [[maybe_unused]] void *src,
		[[maybe_unused]] size_t size, [[maybe_unused]] void *) const = 0;

	// this functions performs a copy from the accelerator address space to host
	// memory
	virtual std::function<std::function<bool(void)>()>
	copy_out([[maybe_unused]] void *dst, [[maybe_unused]] void *src,
		[[maybe_unused]] size_t size, [[maybe_unused]] void *) const = 0;

	// this functions performs a copy from two accelerators that can share it's
	// data without the host intervention
	virtual std::function<std::function<bool(void)>()> copy_between(
		[[maybe_unused]] void *dst, [[maybe_unused]] int dst_device_handler,
		[[maybe_unused]] void *src, [[maybe_unused]] int src_device_handler,
		[[maybe_unused]] size_t size, [[maybe_unused]] void *) const = 0;

	void setDirectoryHandle(int handle);

	virtual std::pair<std::shared_ptr<DeviceAllocation>, bool> createNewDeviceAllocation(const DataAccessRegion &region);

	MemoryPlace *getMemoryPlace();

	ComputePlace *getComputePlace();

	nanos6_device_t getDeviceType() const;

	int getDeviceHandler() const;

	// We use FIFO queues that we launch the tasks on. These are used to check
	// for task completion or to enqueue related operations (in the case of
	// the Cache-Directory).  The actual object type is device-specific: In
	// CUDA, this will be a cudaStream_t, in OpenACC an asynchronous queue,
	// in FPGA a custom FPGA stream, etc.

	// This call requests an available FIFO from the runtime and returns a pointer
	// to it.
	virtual void *getAsyncHandle();

	// Return the FIFO for re-use after task has finished.
	virtual void releaseAsyncHandle(void *asyncHandle);

	static inline Task *getCurrentTask() { return _currentTask; }

	inline static void setCurrentTask(Task* task){ _currentTask = task;}

	virtual void submitDevice(const DeviceEnvironment &deviceEnvironment, const void* args, const nanos6_task_info_t* taskInfo, const nanos6_address_translation_entry_t* translationTable) const = 0;
	virtual std::function<bool()> getDeviceSubmissionFinished(const DeviceEnvironment& deviceEnvironment) const = 0;
	virtual inline void generateDeviceEvironment(DeviceEnvironment& env, const nanos6_task_implementation_info_t* task_implementation) = 0;

	virtual std::pair<void *, bool> accel_allocate(size_t size) = 0;
	virtual bool accel_free(void *) = 0;

	AcceleratorEvent *createEvent();
	virtual AcceleratorEvent *createEvent(std::function<void((AcceleratorEvent *))> onCompletion);

	virtual void destroyEvent(AcceleratorEvent *event);

private:
	static void serviceFunction(void *data);

	static void serviceCompleted(void *data);

	template <class T> uintptr_t get_page(T addr) {
		uintptr_t u_addr = (uintptr_t)addr;
		uintptr_t u_page_mask = ~(_pageSize - 1);
		return (u_addr & u_page_mask);
	}

	// helper for getting the beginning of the next page of the current address
	template <class T> uintptr_t get_page_up(T addr) {
		uintptr_t u_addr = (uintptr_t)addr;
		return (u_addr % _pageSize) ? get_page(u_addr + _pageSize) : get_page(u_addr);
	}
};

#endif // ACCELERATOR_HPP
