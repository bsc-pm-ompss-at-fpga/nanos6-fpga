/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2022 Barcelona Supercomputing Center (BSC)
*/

#include "DeviceUnsyncScheduler.hpp"
#include "scheduling/ready-queues/ReadyQueueDeque.hpp"
#include "scheduling/ready-queues/ReadyQueueMap.hpp"
#include "hardware/device/directory/DeviceDirectory.hpp"

DeviceUnsyncScheduler::DeviceUnsyncScheduler(
	SchedulingPolicy policy,
	bool enablePriority,
	size_t totalDevices
) :
	UnsyncScheduler(policy, enablePriority),
	_totalDevices(totalDevices)
{
	_readyTasksDevice.resize(_totalDevices);
	if (enablePriority) {
		for (size_t i = 0; i < _totalDevices; i++) {
			_readyTasksDevice[i] = new ReadyQueueMap(policy);
		}
	} else {
		for (size_t i = 0; i < _totalDevices; i++) {
			_readyTasksDevice[i] = new ReadyQueueDeque(policy);
		}
	}
}

DeviceUnsyncScheduler::~DeviceUnsyncScheduler()
{
	for (size_t i = 0; i < _totalDevices; i++) {
		delete _readyTasksDevice[i];
	}
}

Task *DeviceUnsyncScheduler::getReadyTask(ComputePlace *computePlace)
{
	size_t deviceId = computePlace->getIndex();

	Task* task = _readyTasksDevice[deviceId]->getReadyTask(computePlace);

	return task;
}

void DeviceUnsyncScheduler::addReadyTask(Task *task, [[maybe_unused]] ComputePlace *computePlace, ReadyTaskHint hint = NO_HINT)
{
	assert(task != nullptr);

	if (hint == DEADLINE_TASK_HINT) {
		assert(task->hasDeadline());
		assert(_deadlineTasks != nullptr);

		_deadlineTasks->addReadyTask(task, true);
		return;
	}

	//int devId = DeviceMemManager::computeDeviceAffinity(task);
	int devId = task->getAccelAffinity();
	if (devId < 0) {
		devId = DeviceDirectoryInstance::instance->computeAffininty(task->getSymbolInfo(), task->getDeviceType());
	}
	assert(devId < (int)_readyTasksDevice.size());
	// _readyTasks->addReadyTask(task, hint == UNBLOCKED_TASK_HINT);
	// The above should probably be ignored, just leaving it for testing
	_readyTasksDevice[devId]->addReadyTask(task, hint == UNBLOCKED_TASK_HINT);
}
