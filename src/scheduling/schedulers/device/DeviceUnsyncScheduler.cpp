/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include "DeviceUnsyncScheduler.hpp"
#include "hardware/device/DeviceMemManager.hpp"
#include "scheduling/ready-queues/ReadyQueueDeque.hpp"
#include "scheduling/ready-queues/ReadyQueueMap.hpp"

DeviceUnsyncScheduler::DeviceUnsyncScheduler(
	SchedulingPolicy policy,
	bool enablePriority,
	bool enableImmediateSuccessor,
	size_t totalDevices
) :
	UnsyncScheduler(policy, enablePriority, enableImmediateSuccessor),
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
	Task *task = nullptr;

	// Get the Accelerator's _deviceHandler
	size_t deviceId = computePlace->getIndex();

	// 1. Check if there is an immediate successor. -- DISABLED for devices
	if (_enableImmediateSuccessor && computePlace != nullptr) {
		size_t immediateSuccessorId = computePlace->getIndex();
		if (_immediateSuccessorTasks[immediateSuccessorId] != nullptr) {
			task = _immediateSuccessorTasks[immediateSuccessorId];
			assert(!task->isTaskfor());
			_immediateSuccessorTasks[immediateSuccessorId] = nullptr;
			return task;
		}
	}

	// 2. Check if there is work remaining in the ready queue.
	//task = _readyTasks->getReadyTask(computePlace);
	// 2. Check if there is work in the queue of the specific device
	task = _readyTasksDevice[deviceId]->getReadyTask(computePlace);

	// 3. Try to get work from other immediateSuccessorTasks.
	if (task == nullptr && _enableImmediateSuccessor) {
		for (size_t i = 0; i < _immediateSuccessorTasks.size(); i++) {
			if (_immediateSuccessorTasks[i] != nullptr) {
				task = _immediateSuccessorTasks[i];
				assert(!task->isTaskfor());
				_immediateSuccessorTasks[i] = nullptr;
				break;
			}
		}
	}

	assert(task == nullptr || !task->isTaskfor());

	return task;
}

void DeviceUnsyncScheduler::addReadyTask(Task *task, ComputePlace *computePlace, ReadyTaskHint hint = NO_HINT)
{
	assert(task != nullptr);

	if (hint == DEADLINE_TASK_HINT) {
		assert(task->hasDeadline());
		assert(_deadlineTasks != nullptr);

		_deadlineTasks->addReadyTask(task, true);
		return;
	}

	// -- DISABLED for devices
	if (_enableImmediateSuccessor) {
		if (computePlace != nullptr && hint == SIBLING_TASK_HINT) {
			size_t immediateSuccessorId = computePlace->getIndex();
			if (!task->isTaskfor()) {
				Task *currentIS = _immediateSuccessorTasks[immediateSuccessorId];
				if (currentIS != nullptr) {
					assert(!currentIS->isTaskfor());
					_readyTasks->addReadyTask(currentIS, false);
				}
				_immediateSuccessorTasks[immediateSuccessorId] = task;
			} else {
				// Multiply by 2 because there are 2 slots per group
				immediateSuccessorId = ((CPU *)computePlace)->getGroupId()*2;
				Task *currentIS1 = _immediateSuccessorTaskfors[immediateSuccessorId];
				Task *currentIS2 = _immediateSuccessorTaskfors[immediateSuccessorId+1];
				if (currentIS1 == nullptr) {
					_immediateSuccessorTaskfors[immediateSuccessorId] = task;
				} else if (currentIS2 == nullptr) {
					_immediateSuccessorTaskfors[immediateSuccessorId+1] = task;
				} else {
					_readyTasks->addReadyTask(currentIS1, false);
					_immediateSuccessorTaskfors[immediateSuccessorId] = task;
				}
			}
			return;
		}
	} // DISABLED

	int devId = DeviceMemManager::computeDeviceAffinity(task);

	// _readyTasks->addReadyTask(task, hint == UNBLOCKED_TASK_HINT);
	// The above should probably be ignored, just leaving it for testing
	_readyTasksDevice[devId]->addReadyTask(task, hint == UNBLOCKED_TASK_HINT);
}
