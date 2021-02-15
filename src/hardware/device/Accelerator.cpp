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


thread_local Task* Accelerator::_currentTask;


void Accelerator::runTask(Task *task)
{
	assert(task != nullptr);

	_currentTask = task;
	AcceleratorStream* acceleratorStream = _streamPool.getStream();
	task->setComputePlace(_computePlace);
	task->setMemoryPlace(_memoryPlace);
	task->setAcceleratorStream(acceleratorStream);

	generateDeviceEvironment(task);


	AcceleratorEvent *event_copies = createEvent([]{});
	event_copies->record(acceleratorStream);

	//if preRunTask passes through the directory,
	//it will add new operations into the stream,
	//so the new event will have a correct result.
	//If not, the prerun will run synchronous, so
	//the next event will have again a correct result.
	preRunTask(task);

	AcceleratorEvent *event_pre_run = createEvent([]{});
	event_pre_run->record(acceleratorStream);

	callBody(task);

	AcceleratorEvent *event_post_run = createEvent([]{});
	event_post_run->record(acceleratorStream);

	acceleratorStream->addOperation([&, event_copies, event_pre_run, event_post_run, task, acceleratorStream]
	{
		float time_spend_in_copies = event_copies->getTimeBetweenEvents_ms(event_pre_run);
		float execution_time = event_pre_run->getTimeBetweenEvents_ms(event_post_run);

		//Future CTF Common Events

		destroyEvent(event_copies);
		destroyEvent(event_pre_run);
		destroyEvent(event_post_run);

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
	Accelerator *accel = (Accelerator *) data;
	assert(accel != nullptr);

	// Execute the service loop
	accel->acceleratorServiceLoop();
}

void Accelerator::serviceCompleted(void *data)
{
	Accelerator *accel = (Accelerator *) data;
	assert(accel != nullptr);
	assert(accel->_stopService);

	// Mark the service as completed
	accel->_finishedService = true;
}
