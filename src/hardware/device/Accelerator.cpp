/*
    This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#include "Accelerator.hpp"
#include "executors/threads/TaskFinalization.hpp"
#include "hardware/HardwareInfo.hpp"
#include "scheduling/Scheduler.hpp"
#include "system/TrackingPoints.hpp"
#include "tasks/TaskImplementation.hpp"

#include <DataAccessRegistration.hpp>

thread_local Task *Accelerator::_currentTask;

void Accelerator::callTaskBody(Task *task, nanos6_address_translation_entry_t *translationTable)
{
	task->body(translationTable);
}

void Accelerator::runTask(Task *task)
{
	assert(task != nullptr);

	_currentTask = task;
	AcceleratorStream *acceleratorStream = _streamPool.getStream();
	task->setComputePlace(_computePlace);
	task->setMemoryPlace(_memoryPlace);
	task->setAcceleratorStream(acceleratorStream);
	task->setAccelerator(this);

	generateDeviceEvironment(task->getDeviceEnvironment(), task->getImplementations());

	//if preRunTask passes through the directory,
	//it will add new operations into the stream,
	//so the new event will have a correct result.
	//If not, the prerun will run synchronous, so
	//the next event will have again a correct result.
	preRunTask(task);

	callBody(task);

	//There are no copies after the execution of a normal task
#ifdef USE_DISTRIBUTED
	postRunTask(task);
#endif

	acceleratorStream->addOperation([=]() {
		finishTask(task);
		_streamPool.releaseStream(acceleratorStream);
		return true;
	});
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

bool Accelerator::accel_free(void *){return true;}

std::pair<void *, bool> Accelerator::accel_allocate(size_t size) {
	static uintptr_t fake_offset = 0x1000;
	uintptr_t fake = fake_offset;
	fake_offset += size;
	return { (void *)fake,  true};
}

void Accelerator::initializeService()
{
	// Spawn service function
	SpawnFunction::spawnFunction(
		serviceFunction, this,
		serviceCompleted, this,
		"Device service", false);
}

void Accelerator::shutdownService()
{
	// Notify the service to stop
	_stopService = true;

	// Wait until the service completes
	while (!_finishedService)
		;
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

void Accelerator::setDirectoryHandle(int handle) { _directoryHandler = handle; }

std::pair<std::shared_ptr<DeviceAllocation>, bool> Accelerator::createNewDeviceAllocation(const DataAccessRegion &region)
{
	if (getDeviceType() == nanos6_host_device)
		return { std::make_shared<DeviceAllocation>(
			DataAccessRegion((void *)region.getStartAddress(), (void *)region.getEndAddress()),
			DataAccessRegion((void *)region.getStartAddress(), (void *)region.getEndAddress()),
			[]{}
		), true};


	std::pair<void*, bool> allocation = accel_allocate(region.getSize());
	if (!allocation.second) return {nullptr, false};

	void* ptr = allocation.first;

	const DataAccessRegion host = DataAccessRegion(region.getStartAddress(), region.getEndAddress());
	const DataAccessRegion device = DataAccessRegion((void *)ptr, (void *)((uintptr_t)ptr + (uintptr_t)region.getEndAddress() - (uintptr_t)region.getStartAddress()));

	return {std::make_shared<DeviceAllocation>
		(
			host,
			device,
			[=]{ setActiveDevice(); accel_free(ptr); }
		), true};

}

// Generate the appropriate device_env pointer Mercurium uses for device tasks
//void Accelerator::generateDeviceEvironment(Task *) {}

void Accelerator::acceleratorServiceLoop() {
	WorkerThread *currentThread = WorkerThread::getCurrentWorkerThread();
	assert(currentThread != nullptr);

	while (!shouldStopService())
	{
		setActiveDevice();
		do {
			// Launch as many ready device tasks as possible
			while (_streamPool.streamAvailable()) {
				Task *task = Scheduler::getReadyTask(_computePlace, currentThread);
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

bool Accelerator::shouldStopService() const {
	return _stopService.load(std::memory_order_relaxed);
}

void Accelerator::destroyEvent(AcceleratorEvent *event) { delete event; }

Accelerator::Accelerator(int handler, nanos6_device_t type, uint32_t numOfStreams,
	size_t pollingPeriodUs, bool isPinnedPolling) :
	_stopService(false), _finishedService(false), _deviceHandler(handler),
	_deviceType(type),
	_memoryPlace(new MemoryPlace(_deviceHandler, _deviceType)),
	_computePlace(new ComputePlace(_deviceHandler, _deviceType)),
	_streamPool(numOfStreams, [&]{setActiveDevice();}), _pollingPeriodUs(pollingPeriodUs),
	_isPinnedPolling(isPinnedPolling)
{
	_computePlace->addMemoryPlace(_memoryPlace);
}

MemoryPlace * Accelerator::getMemoryPlace() { return _memoryPlace; }

ComputePlace * Accelerator::getComputePlace() { return _computePlace; }

nanos6_device_t  Accelerator::getDeviceType() const { return _deviceType; }

int  Accelerator::getDeviceHandler() const { return _deviceHandler; }

