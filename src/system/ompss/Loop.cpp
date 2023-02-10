/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2022 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>

#include <nanos6.h>

#include "AddTask.hpp"
#include "executors/threads/WorkerThread.hpp"
#include "tasks/Taskloop.hpp"

void nanos6_create_loop(
	nanos6_task_info_t *task_info,
	nanos6_task_invocation_info_t *task_invocation_info,
	char const *,
	size_t args_block_size,
	/* OUT */ void **args_block_pointer,
	/* OUT */ void **task_pointer,
	size_t flags,
	size_t num_deps,
	size_t lower_bound,
	size_t upper_bound,
	size_t grainsize,
	size_t chunksize
) {
	// TODO: Temporary check until multiple implementations are supported
	assert(task_info->implementation_count == 1);

	nanos6_device_t deviceType = (nanos6_device_t) task_info->implementations[0].device_type_id;
	if (!HardwareInfo::canDeviceRunTasks(deviceType)) {
		FatalErrorHandler::fail("No hardware associated for task device type ", deviceType);
	}

	// Taskfor is no longer supported. Only accept taskloops
	if (flags & nanos6_taskfor_task) {
		FatalErrorHandler::fail("Taskfor no longer supported");
	}

	// The compiler passes either the num deps of a single child or -1. However, the parent
	// taskloop must register as many deps as num_deps * numTasks
	if (num_deps != (size_t) -1) {
		size_t numTasks = Taskloop::computeNumTasks((upper_bound - lower_bound), grainsize);
		num_deps *= numTasks;
	}

	Task *task = AddTask::createTask(
		task_info, task_invocation_info,
		*args_block_pointer, args_block_size,
		flags, num_deps, true
	);
	assert(task != nullptr);

	*task_pointer = (void *) task;
	*args_block_pointer = task->getArgsBlock();

	assert(task != nullptr);
	assert(task->isTaskloop());

	Taskloop *taskloop = (Taskloop *) task;
	taskloop->initialize(lower_bound, upper_bound, grainsize, chunksize);
}

