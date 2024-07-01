/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASK_INFO_DATA_HPP
#define TASK_INFO_DATA_HPP

#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <map>

#include <nanos6/task-info-registration.h>

#include "lowlevel/SpinLock.hpp"

#include <InstrumentTasktypeData.hpp>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if USE_FPGA
#include <iostream>
#include "hardware/device/fpga/FPGAAccelerator.hpp"
#include "hardware/device/fpga/FPGAReverseOffload.hpp"

static unsigned int simple_hash_str(const char *str)
{
    const int MULTIPLIER = 33;
    unsigned int h;
    unsigned const char *p;

    h = 0;
    for (p = (unsigned const char*)str; *p != '\0'; p++)
        h = MULTIPLIER * h + *p;

    h += (h >> 5);

    return h; // or, h % ARRAY_SIZE;
}
#endif

class TaskInfoManager;
class TasktypeStatistics;

class TaskInfoData {
	//! Task type label
	std::string _taskTypeLabel;

	//! Task declaration source
	std::string _taskDeclarationSource;

	//! Instrumentation identifier for this task info
	Instrument::TasktypeInstrument _instrumentId;

	//! Monitoring-related statistics per taskType
	TasktypeStatistics *_taskTypeStatistics;

	friend class TaskInfoManager;

public:
	inline TaskInfoData() :
		_taskTypeLabel(),
		_taskDeclarationSource(),
		_instrumentId(),
		_taskTypeStatistics(nullptr)
	{
	}

	inline const std::string &getTaskTypeLabel() const
	{
		return _taskTypeLabel;
	}

	inline const std::string &getTaskDeclarationSource() const
	{
		return _taskDeclarationSource;
	}

	inline Instrument::TasktypeInstrument &getInstrumentationId()
	{
		return _instrumentId;
	}
	
	inline void setTasktypeStatistics(TasktypeStatistics *taskTypeStatistics)
	{
		_taskTypeStatistics = taskTypeStatistics;
	}

	inline TasktypeStatistics *getTasktypeStatistics() const
	{
		return _taskTypeStatistics;
	}
};

class TaskInfoManager {
	//! A map with task info data useful for filtering duplicated task infos
	typedef std::map<nanos6_task_info_t *, TaskInfoData> task_info_map_t;
	static task_info_map_t _taskInfos;

	//! SpinLock to register and traverse task infos
	static SpinLock _lock;

	//! Check whether any device kernel must be loaded
	static void checkDeviceTaskInfo(const nanos6_task_info_t *taskInfo);

public:
	//! \brief Register a new task info
	//!
	//! \param[in,out] taskInfo A pointer to a task info
	static inline TaskInfoData &registerTaskInfo(nanos6_task_info_t *taskInfo) {
		assert(taskInfo != nullptr);
		assert(taskInfo->implementations != nullptr);
		assert(taskInfo->implementations[0].declaration_source != nullptr);

#if USE_FPGA
		if (taskInfo->implementations[0].device_type_id == nanos6_fpga_device) {
			assert(taskInfo->implementations[0].device_function_name != nullptr);
		}
		if (taskInfo->implementations[0].device_function_name != nullptr) {
			uint64_t subtype = simple_hash_str(taskInfo->implementations[0].device_function_name) & 0xFFFFFFFF;
			if (taskInfo->implementations[0].device_type_id == nanos6_fpga_device) {
				subtype |= 0x100000000llu;
				FPGAAccelerator::_device_subtype_map[taskInfo->implementations] = subtype;
			}
			else {
				subtype |= 0x200000000llu;
				FPGAReverseOffload::_reverseMap[subtype] = taskInfo;
			}
		}
#endif

		// Global number of unlabeled task infos
		static size_t unlabeledTaskInfos = 0;

		// Check whether any device kernel must be loaded
		checkDeviceTaskInfo(taskInfo);

		std::lock_guard<SpinLock> lock(_lock);

		auto result = _taskInfos.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(taskInfo),
			std::forward_as_tuple(/* empty */)
		);

		TaskInfoData &data = result.first->second;

		// Stop since it's not a new task info
		if (!result.second)
			return data;

		// Setup task type label and declaration source
		if (taskInfo->implementations[0].task_type_label) {
			data._taskTypeLabel = std::string(taskInfo->implementations[0].task_type_label);
		} else {
			data._taskTypeLabel = "Unlabeled" + std::to_string(unlabeledTaskInfos++);
		}
		data._taskDeclarationSource = std::string(taskInfo->implementations[0].declaration_source);

		// Setup the reference to the data in the task info
		taskInfo->task_type_data = &data;

		return data;
	}

	//! \brief Traverse all task infos and apply a certain function for each of them
	//!
	//! \param[in] functionToApply The function to apply to each task info
	template <typename F>
	static inline void processAllTaskInfos(F functionToApply)
	{
		std::lock_guard<SpinLock> lock(_lock);

		for (auto &taskInfo : _taskInfos) {
			functionToApply(taskInfo.first, taskInfo.second);
		}
	}
};

#endif // TASK_INFO_DATA_HPP
