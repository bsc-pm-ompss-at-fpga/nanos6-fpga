#include "FPGAReverseOffload.hpp"
#include "system/ompss/SpawnFunction.hpp"
#include "system/BlockingAPI.hpp"
#include <libxtasks.h>
#include <assert.h>
#include <vector>

std::unordered_map<uint64_t, const nanos6_task_info_t*> FPGAReverseOffload::_reverseMap;

void FPGAReverseOffload::serviceFunction(void *data)
{
	FPGAReverseOffload *reverseOffload = (FPGAReverseOffload *)data;

	// Execute the service loop
	reverseOffload->serviceLoop();
}

void FPGAReverseOffload::serviceCompleted(void *data)
{
	FPGAReverseOffload *reverseOffload = (FPGAReverseOffload *)data;

	// Mark the service as completed
	reverseOffload->_finishedService = true;
}

void FPGAReverseOffload::initializeService() {
	// Spawn service function
	SpawnFunction::spawnFunction(
				serviceFunction, this,
				serviceCompleted, this,
				"FPGA reverse offload service", false
				);
}

void FPGAReverseOffload::shutdownService() {
	_stopService = true;
	while (!_finishedService);
}

void FPGAReverseOffload::serviceLoop() {
	while (!_stopService) {
		xtasks_stat stat;
		xtasks_newtask* xtasks_task = NULL;
		stat = xtasksTryGetNewTask(&xtasks_task);
		bool foundTask = false;
		if (stat == XTASKS_SUCCESS) {
			foundTask = true;
			std::unordered_map<uint64_t, const nanos6_task_info_t*>::const_iterator it = _reverseMap.find(xtasks_task->typeInfo);
			assert(it != _reverseMap.end());
			const nanos6_task_info_t* task_info = it->second;
			for (unsigned int i = 0; i < xtasks_task->numCopies; ++i) {
				char* mem = new char[xtasks_task->copies[i].size];
				xtasks_task->args[xtasks_task->copies[i].argIdx] = (uint64_t)mem;
				if (xtasks_task->copies[i].flags & 0x01) {
					_allocator.memcpy(mem, xtasks_task->copies[i].address, xtasks_task->copies[i].size, XTASKS_ACC_TO_HOST);
				}
			}
			task_info->implementations[0].run(xtasks_task->args, nullptr, nullptr);
			for (unsigned int i = 0; i < xtasks_task->numCopies; ++i) {
				char* mem = (char*)xtasks_task->args[xtasks_task->copies[i].argIdx];
				if (xtasks_task->copies[i].flags & 0x02) {
					_allocator.memcpy(xtasks_task->copies[i].address, mem, xtasks_task->copies[i].size, XTASKS_HOST_TO_ACC);
				}
				delete mem;
			}
			stat = xtasksNotifyFinishedTask(xtasks_task->parentId, xtasks_task->taskId);
			assert(stat == XTASKS_SUCCESS);
		}
		else {
			assert(stat == XTASKS_PENDING);
		}
		if (!foundTask)
			BlockingAPI::waitForUs(_pollingPeriodUs);
	}
}
