/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "Accelerator.hpp"
#include "executors/threads/TaskFinalization.hpp"
#include "hardware/HardwareInfo.hpp"
#include "scheduling/Scheduler.hpp"
#include "system/TrackingPoints.hpp"
#include "tasks/TaskImplementation.hpp"

#include <DataAccessRegistration.hpp>


thread_local Task *Accelerator::_currentTask;


void Accelerator::runTask(Task *task)
{
	assert(task != nullptr);

	_currentTask = task;
	AcceleratorStream *acceleratorStream = _streamPool.getStream();
	task->setComputePlace(_computePlace);
	task->setMemoryPlace(_memoryPlace);
	task->setAcceleratorStream(acceleratorStream);
	task->setAccelerator(this);

	generateDeviceEvironment(task);


	AcceleratorEvent *event_copies = createEvent();
	event_copies->record(acceleratorStream);

	//if preRunTask passes through the directory,
	//it will add new operations into the stream,
	//so the new event will have a correct result.
	//If not, the prerun will run synchronous, so
	//the next event will have again a correct result.
	preRunTask(task);

	AcceleratorEvent *event_pre_run = createEvent();
	event_pre_run->record(acceleratorStream);

	callBody(task);
	AcceleratorEvent *event_post_run = createEvent([this, acceleratorStream, event_copies, event_pre_run, task](AcceleratorEvent *own) 
	{
		[[maybe_unused]] float time_spend_in_copies = event_copies->getMillisBetweenEvents(event_pre_run);
		[[maybe_unused]] float execution_time = event_pre_run->getMillisBetweenEvents(own);

		//Future CTF Common Events

		finishTask(task);
		_streamPool.releaseStream(acceleratorStream);

		//destroyEvent(event_copies);
		//destroyEvent(event_pre_run);
		//destroyEvent(own);
		return true;
	});

	event_post_run->record(acceleratorStream);
}

void Accelerator::finishTask(Task *task)
{
	finishTaskCleanup(task);
	WorkerThread *currThread = WorkerThread::getCurrentWorkerThread();

	CPU *cpu = nullptr;
	if (currThread != nullptr)
		cpu = currThread->getComputePlace();

	CPUDependencyData localDependencyData;
	CPUDependencyData &hpDependencyData = (cpu != nullptr) ? cpu->getDependencyData() : localDependencyData;

	if (task->isIf0()) {
		Task *parent = task->getParent();
		assert(parent != nullptr);

		// Unlock parent that was waiting for this if0
		Scheduler::addReadyTask(parent, cpu, UNBLOCKED_TASK_HINT);
	}

	if (task->markAsFinished(cpu)) {
		DataAccessRegistration::unregisterTaskDataAccesses(
			task, cpu, hpDependencyData,
			task->getMemoryPlace(),
			/* from busy thread */ true);

		TaskFinalization::taskFinished(task, cpu, /* busy thread */ true);

		if (task->markAsReleased()) {
			TaskFinalization::disposeTask(task);
		}
	}
}

   AcceleratorEvent *Accelerator::createEvent(std::function<void((AcceleratorEvent *))> onCompletion)
    {
    return new AcceleratorEvent(onCompletion);
  	}

   AcceleratorEvent *Accelerator::createEvent()
    {
    	return createEvent([](AcceleratorEvent *) {});
  	}

  void *Accelerator::getAsyncHandle() { return nullptr; };


  void Accelerator::releaseAsyncHandle([[maybe_unused]] void *asyncHandle){}


  
void Accelerator::accel_free(void *){}

void *Accelerator::accel_allocate(size_t size) {
    static uintptr_t fake_offset = 0x1000;
    uintptr_t fake = fake_offset;
    fake_offset += size;
    return (void *)fake;
  }

void Accelerator::initializeService()
{
	// Spawn service function
	SpawnFunction::spawnFunction(
		serviceFunction, this,
		serviceCompleted, this,
		"Device service", false
		);
}

void Accelerator::shutdownService()
{
	// Notify the service to stop
	_stopService = true;

	// Wait until the service completes
	while (!_finishedService);
}

void Accelerator::serviceFunction(void *data)
{
	Accelerator *accel = (Accelerator *)data;
	assert(accel != nullptr);

	// Execute the service loop
	accel->acceleratorServiceLoop();
}

void Accelerator::serviceCompleted(void *data)
{
	Accelerator *accel = (Accelerator *)data;
	assert(accel != nullptr);
	assert(accel->_stopService);

	// Mark the service as completed
	accel->_finishedService = true;
}



  int  Accelerator::getDirectoryHandler() const { return _directoryHandler; }

  void  Accelerator::setDirectoryHandler(int directoryHandler) {
    _directoryHandler = directoryHandler;
  }

  // this function performs a copy from a host address space into the
  // accelerator
  AcceleratorStream::activatorReturnsChecker
  Accelerator::copy_in([[maybe_unused]] void *dst, [[maybe_unused]] void *src,
          [[maybe_unused]] size_t size, [[maybe_unused]] Task *) {
    return [] { return [] { return true; }; };
  }

  // this functions performs a copy from the accelerator address space to host
  // memory
  AcceleratorStream::activatorReturnsChecker
  Accelerator::copy_out([[maybe_unused]] void *dst, [[maybe_unused]] void *src,
           [[maybe_unused]] size_t size, [[maybe_unused]] Task *) {
    return [] { return [] { return true; }; };
  }

  // this functions performs a copy from two accelerators that can share it's
  // data without the host intervention
  AcceleratorStream::activatorReturnsChecker 
  Accelerator::copy_between(
      [[maybe_unused]] void *dst, [[maybe_unused]] int dst_device_handler,
      [[maybe_unused]] void *src, [[maybe_unused]] int src_device_handler,
      [[maybe_unused]] size_t size, [[maybe_unused]] Task *) {
    return [] { return [] { return true; }; };
  }

  void Accelerator::setDirectoryHandle(int handle) { _directoryHandler = handle; }

  std::shared_ptr<DeviceAllocation>
  Accelerator::createNewDeviceAllocation(const DataAccessRegion &region) {
    if (getDeviceType() == nanos6_host_device)
      return std::make_shared<DeviceAllocation>(
          DataAccessRegion((void *)region.getStartAddress(),
                           (void *)region.getStartAddress()),
          DataAccessRegion((void *)region.getStartAddress(),
                           (void *)region.getStartAddress()),
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


  void Accelerator::callBody([[maybe_unused]] Task *task){};


  // Device specific operations after task completion may go here (e.g. free
  // environment)
  void Accelerator::finishTaskCleanup(Task *) {}

  // Generate the appropriate device_env pointer Mercurium uses for device tasks
  void Accelerator::generateDeviceEvironment(Task *) {}

	void Accelerator::preRunTask(Task*){}

	void Accelerator::postRunTask(Task *){}


	  void Accelerator::acceleratorServiceLoop() {
    while (!shouldStopService()) 
    {
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
        std::cout<<"doing while for fpga is pinned: "<<_isPinnedPolling<<"ongoing: "<<_streamPool.ongoingStreams() <<std::endl;
        // Iterate while there are running tasks and pinned polling is enabled
      } while (_isPinnedPolling && _streamPool.ongoingStreams());

      // Sleep for a configured amount of microseconds
      BlockingAPI::waitForUs(_pollingPeriodUs);
    }

    std::cout<<"service loop out"<<std::endl;


  }


   bool Accelerator::shouldStopService() const {
    return _stopService.load(std::memory_order_relaxed);
  }

  void Accelerator::setActiveDevice(){};
 int Accelerator::getVendorDeviceId(){ return 0;}

 void Accelerator::destroyEvent(AcceleratorEvent *event) { delete event; }



 Accelerator::Accelerator(int handler, nanos6_device_t type, uint32_t numOfStreams,
              size_t pollingPeriodUs, bool isPinnedPolling)
      : _stopService(false), _finishedService(false), _deviceHandler(handler),
        _deviceType(type),
        _memoryPlace(new MemoryPlace(_deviceHandler, _deviceType)),
        _computePlace(new ComputePlace(_deviceHandler, _deviceType)),
        _streamPool(numOfStreams, [&]{setActiveDevice();}), _pollingPeriodUs(pollingPeriodUs),
        _isPinnedPolling(isPinnedPolling) {
    _computePlace->addMemoryPlace(_memoryPlace);
  }





  MemoryPlace * Accelerator::getMemoryPlace() { return _memoryPlace; }

  ComputePlace * Accelerator::getComputePlace() { return _computePlace; }

  nanos6_device_t  Accelerator::getDeviceType() const { return _deviceType; }

  int  Accelerator::getDeviceHandler() const { return _deviceHandler; }

