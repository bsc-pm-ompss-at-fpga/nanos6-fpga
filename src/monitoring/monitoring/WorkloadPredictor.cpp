/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2019-2020 Barcelona Supercomputing Center (BSC)
*/

#include "WorkloadPredictor.hpp"


WorkloadPredictor *WorkloadPredictor::_predictor;


void WorkloadPredictor::taskCreated(TaskStatistics *taskStatistics) {
	assert(_predictor != nullptr);
	assert(taskStatistics != nullptr);

	// Account the new task as instantiated workload
	_predictor->increaseInstances(instantiated_load);

	// Since the cost of a task includes children tasks, if an ancestor had
	// a prediction, do not take this task into account for workloads
	if (!taskStatistics->ancestorHasPrediction() && taskStatistics->hasPrediction()) {
		const std::string &label = taskStatistics->getLabel();
		size_t cost              = taskStatistics->getCost();
		_predictor->increaseWorkload(instantiated_load, label, cost);
	}
}

void WorkloadPredictor::taskChangedStatus(
	TaskStatistics *taskStatistics,
	monitoring_task_status_t oldStatus,
	monitoring_task_status_t newStatus
) {
	assert(_predictor != nullptr);
	assert(taskStatistics != nullptr);

	const std::string &label = taskStatistics->getLabel();
	size_t cost              = taskStatistics->getCost();
	workload_t decreaseLoad  = WorkloadPredictor::getLoadId(oldStatus);
	workload_t increaseLoad  = WorkloadPredictor::getLoadId(newStatus);

	if (decreaseLoad != null_workload) {
		_predictor->decreaseInstances(decreaseLoad);
		if (!taskStatistics->ancestorHasPrediction() && taskStatistics->hasPrediction()) {
			_predictor->decreaseWorkload(decreaseLoad, label, cost);
		}
	}

	if (increaseLoad != null_workload) {
		_predictor->increaseInstances(increaseLoad);
		if (!taskStatistics->ancestorHasPrediction() && taskStatistics->hasPrediction()) {
			_predictor->increaseWorkload(increaseLoad, label, cost);
		}
	}
}

void WorkloadPredictor::taskCompletedUserCode(TaskStatistics *taskStatistics) {
	assert(_predictor != nullptr);
	assert(taskStatistics != nullptr);

	// If the task's time is taken into account by an ancestor task (an ancestor has a
	// prediction), when this task finishes we subtract the elapsed time from workloads
	if (taskStatistics->ancestorHasPrediction()) {
		// Find the ancestor who had accounted this task's cost
		TaskStatistics *ancestorStatistics = taskStatistics->getParentStatistics();
		assert(ancestorStatistics != nullptr);

		while (!ancestorStatistics->hasPrediction()) {
			ancestorStatistics = ancestorStatistics->getParentStatistics();
		}

		// Aggregate the elapsed ticks of the task in the ancestor. When the
		// ancestor has finished, the aggregation will be subtracted from workloads
		size_t elapsed =
			taskStatistics->getChronoTicks(executing_status) +
			taskStatistics->getChronoTicks(runtime_status);
		ancestorStatistics->increaseChildCompletionTimes(elapsed);

		// Aggregate the time in workloads also, to obtain better predictions
		_predictor->increaseTaskCompletionTimes(elapsed);
	}
}

void WorkloadPredictor::taskFinished(
	TaskStatistics *taskStatistics,
	monitoring_task_status_t oldStatus,
	int ancestorsUpdated
) {
	assert(_predictor != nullptr);
	assert(taskStatistics != nullptr);
	assert(ancestorsUpdated >= 0);

	// Follow the chain of ancestors and keep updating finished tasks
	while (ancestorsUpdated > 0) {
		assert(!taskStatistics->getAliveChildren());
		const std::string &label = taskStatistics->getLabel();
		size_t cost              = taskStatistics->getCost();
		size_t childTimes        = taskStatistics->getChildCompletionTimes();
		workload_t decreaseLoad  = WorkloadPredictor::getLoadId(oldStatus);

		// Decrease workloads by task's statistics
		if (decreaseLoad != null_workload) {
			_predictor->decreaseInstances(decreaseLoad);
			if (!taskStatistics->ancestorHasPrediction() && taskStatistics->hasPrediction()) {
				_predictor->decreaseWorkload(decreaseLoad, label, cost);
			}
		}

		// Increase workloads by task's statistics
		_predictor->increaseInstances(finished_load);
		if (!taskStatistics->ancestorHasPrediction() && taskStatistics->hasPrediction()) {
			_predictor->increaseWorkload(finished_load, label, cost);
		}

		// Decrease the task completion times, as the task has finished
		_predictor->decreaseTaskCompletionTimes(childTimes);

		// Follow the chain of ancestors
		taskStatistics  = taskStatistics->getParentStatistics();

		// Decrease the number of ancestors to update
		--ancestorsUpdated;

		if (taskStatistics == nullptr) {
			break;
		}
	}
}

double WorkloadPredictor::getPredictedWorkload(workload_t loadId)
{
	assert(_predictor != nullptr);

	double totalTime = 0.0;
	for (auto const &it : _predictor->_workloads) {
		assert(it.second != nullptr);

		totalTime += (
			it.second->getAccumulatedCost(loadId) *
			TaskMonitor::getAverageTimePerUnitOfCost(it.first)
		);
	}

	return totalTime;
}

size_t WorkloadPredictor::getTaskCompletionTimes()
{
	assert(_predictor != nullptr);

	return _predictor->_taskCompletionTimes.load();
}
