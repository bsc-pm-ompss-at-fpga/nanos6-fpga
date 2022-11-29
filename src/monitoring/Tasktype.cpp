/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2022 Barcelona Supercomputing Center (BSC)
*/

#include <config.h>
#include <string>

#include "Tasktype.hpp"
#include "tasks/TaskInfoManager.hpp"


Tasktype::task_type_map_t Tasktype::_tasktypes;
SpinLock Tasktype::_lock;
std::atomic<size_t> Tasktype::_numUnlabeledTasktypes(0);


void Tasktype::registerTasktype(nanos6_task_info_t *taskInfo)
{
	assert(taskInfo != nullptr);
	assert(taskInfo->implementations != nullptr);
	assert(taskInfo->implementations->declaration_source != nullptr);

	std::string label;
	if (taskInfo->implementations->task_type_label != nullptr) {
		label = std::string(taskInfo->implementations->task_type_label);
	} else {
		// Avoid comparing empty strings and identify them separately
		size_t unlabeledId = _numUnlabeledTasktypes++;
		label = "Unlabeled" + std::to_string(unlabeledId);
	}

	std::string declarationSource(taskInfo->implementations->declaration_source);

	// NOTE: We try to emplace the new Tasktype in the map:
	// 1) If the element is emplaced, it's a new type of task and a new
	// TasktypeStatistics has been created
	// 2) If the key already existed, it's a duplicated type of task, and the
	// iterator points to the original copy
	std::pair<task_type_map_t::iterator, bool> emplacedElement;

	_lock.lock();

	emplacedElement = _tasktypes.emplace(
		std::piecewise_construct,
		std::forward_as_tuple(label, declarationSource),
		std::forward_as_tuple());

	_lock.unlock();

	// Save a ref of the tasktype statistics in the tasktype data of the taskinfo
	task_type_map_t::iterator it = emplacedElement.first;
	TaskInfoData *taskInfoData = (TaskInfoData *) taskInfo->task_type_data;
	assert(taskInfoData != nullptr);
	
	taskInfoData->setTasktypeStatistics(&(it->second));
}
