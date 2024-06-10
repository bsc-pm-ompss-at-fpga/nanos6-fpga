#include "FPGAReverseOffload.hpp"
#include "system/SpawnFunction.hpp"
#include "system/BlockingAPI.hpp"
#include "tasks/Task.hpp"
#include <libxtasks.h>
#include <assert.h>
#include <vector>
#include "InstrumentFPGAEvents.hpp"

std::unordered_map<uint64_t, const nanos6_task_info_t*> FPGAReverseOffload::_reverseMap;

void FPGAReverseOffload::serviceFunction(void *data)
{
	FPGAReverseOffload *reverseOffload = (FPGAReverseOffload *)data;
	assert(reverseOffload != nullptr);

	// Execute the service loop
	reverseOffload->serviceLoop();
}

void FPGAReverseOffload::serviceCompleted(void *data)
{
	FPGAReverseOffload *reverseOffload = (FPGAReverseOffload *)data;
	assert(reverseOffload != nullptr);
	assert(reverseOffload->_stopService);

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
	Instrument::startFPGAInstrumentation();
	while (!_stopService) {
		bool foundTask = false;
		do {
			xtasks_stat stat;
			xtasks_newtask* xtasks_task = NULL;
			stat = xtasksTryGetNewTask(&xtasks_task);
			if (stat == XTASKS_SUCCESS) {
				foundTask = true;
				std::unordered_map<uint64_t, const nanos6_task_info_t*>::const_iterator it = _reverseMap.find(xtasks_task->typeInfo);
#ifndef NDEBUG
				FatalErrorHandler::failIf(
					it == _reverseMap.end(),
					"Device subtype ", xtasks_task->typeInfo, " not found in reverse map"
				);
#endif
				const nanos6_task_info_t* task_info = it->second;

				if (_isInstrumented)
				{
					uint64_t eventValue = 0x24; // SMP task
					uint32_t eventType = 0;
					Instrument::emitReverseOffloadingEvent(eventValue, eventType);
				}

				for (unsigned int i = 0; i < xtasks_task->numCopies; ++i) {
					void* mem = MemoryAllocator::alloc(xtasks_task->copies[i].size);
					xtasks_task->args[xtasks_task->copies[i].argIdx] = (uint64_t)mem;
					if (xtasks_task->copies[i].flags & 0x01) {
						_allocator.memcpy(mem, xtasks_task->copies[i].address, xtasks_task->copies[i].size, XTASKS_ACC_TO_HOST);
					}
				}

				int numArgs = task_info->num_args;
				int argsBlockSize = 0;
				for (int i = 0; i < numArgs; ++i) {
					argsBlockSize += task_info->sizeof_table[i];
				}
				void* argsBlock = MemoryAllocator::alloc(argsBlockSize);
				for (int i = 0; i < numArgs; ++i) {
					assert(task_info->sizeof_table[i] <= (int)sizeof(xtasks_newtask_arg));
					memcpy((char*)argsBlock + task_info->offset_table[i], &xtasks_task->args[i], task_info->sizeof_table[i]);
				}

				task_info->implementations[0].run(argsBlock, nullptr, nullptr);

				for (unsigned int i = 0; i < xtasks_task->numCopies; ++i) {
					void* mem = (void*)xtasks_task->args[xtasks_task->copies[i].argIdx];
					if (xtasks_task->copies[i].flags & 0x02) {
						_allocator.memcpy(xtasks_task->copies[i].address, mem, xtasks_task->copies[i].size, XTASKS_HOST_TO_ACC);
					}
					MemoryAllocator::free(mem, xtasks_task->copies[i].size);
				}

				if (_isInstrumented)
				{
					uint64_t eventValue = 0x24; // SMP task
					uint32_t eventType = 1;
					Instrument::emitReverseOffloadingEvent(eventValue, eventType);
				}

				stat = xtasksNotifyFinishedTask(xtasks_task->parentId, xtasks_task->taskId);
				assert(stat == XTASKS_SUCCESS);
				MemoryAllocator::free(argsBlock, argsBlockSize);
			}
			else {
				assert(stat == XTASKS_PENDING);
			}
		} while (_isPinnedPolling && !_stopService);

		if (!foundTask)
			BlockingAPI::waitForUs(_pollingPeriodUs);
	}
//	Instrument::stopFPGAInstrumentation();
}
