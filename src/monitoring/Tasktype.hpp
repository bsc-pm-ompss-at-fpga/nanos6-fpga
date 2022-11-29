/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef TASKTYPE_HPP
#define TASKTYPE_HPP

#include <atomic>
#include <map>
#include <string>

#include <nanos6/task-info-registration.h>

#include "TasktypeStatistics.hpp"
#include "lowlevel/SpinLock.hpp"


struct TasktypeId {
	std::string _taskLabel;
	std::string _taskDeclarationSource;

	inline TasktypeId(
		const std::string &taskLabel,
		const std::string &taskDeclarationSource
	) :
		_taskLabel(taskLabel),
		_taskDeclarationSource(taskDeclarationSource)
	{
	}

	//! \brief Overloaded lesser-operator for task_type_map_t
	//! Duplicated Tasktypes have the same label or the same declaration
	//! source, thus we must overload the operator to act this way
	bool operator<(const TasktypeId &other) const
	{
		assert(_taskLabel != "");
		assert(_taskDeclarationSource != "");
		assert(other._taskLabel != "");
		assert(other._taskDeclarationSource != "");

		// NOTE:
		// If either the label or the declaration source are the same,
		// both taskinfos are of the same tasktype, thus the comparison
		// must result in false. The map will compare both keys, and if the
		// following condition is met, the keys are treated as identical:
		// !(K1 < K2) && !(K2 < K1)
		//
		// -------------------------------------------
		// | Expected Comparison Results             |
		// -------------------------------------------
		// | Labels    | DeclarationSources | Result |
		// |-----------+--------------------+---------
		// | Equal     | Equal              | False  |
		// | Equal     | Different          | False  |
		// | Different | Equal              | False  |
		// | Different | Different          | True   |
		// -------------------------------------------

		bool sameLabel = (_taskLabel == other._taskLabel);
		bool sameSource = (_taskDeclarationSource == other._taskDeclarationSource);

		if (sameLabel || sameSource) {
			return false;
		} else if (!sameLabel) {
			return (_taskLabel < other._taskLabel);
		} else {
			return (_taskDeclarationSource < other._taskDeclarationSource);
		}
	}
};

class Tasktype {

private:

	//! A map with task type data
	typedef std::map<TasktypeId, TasktypeStatistics> task_type_map_t;
	static task_type_map_t _tasktypes;

	//! SpinLock to register tasktypes
	static SpinLock _lock;

	//! Keep track of the number of unlabeled tasktypes
	static std::atomic<size_t> _numUnlabeledTasktypes;

public:

	//! \brief Register a new tasktype
	//!
	//! \param[in,out] taskInfo A pointer to a taskinfo
	static void registerTasktype(nanos6_task_info_t *taskInfo);

	//! \brief Traverse all tasktypes and apply a certain function for each of them
	//!
	//! \param[in] functionToApply The function to apply to each child node
	template <typename F>
	static inline void processAllTasktypes(F functionToApply)
	{
		_lock.lock();

		for (auto &tasktype : _tasktypes) {
			const std::string &label = tasktype.first._taskLabel;
			const std::string &source = tasktype.first._taskDeclarationSource;
			TasktypeStatistics &tasktypeStatistics = tasktype.second;

			functionToApply(label, source, tasktypeStatistics);
		}

		_lock.unlock();
	}

};

#endif // TASKTYPE_HPP
