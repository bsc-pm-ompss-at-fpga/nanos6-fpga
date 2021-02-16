/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "CUDAAccelerator.hpp"
#include "CUDAAcceleratorEvent.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "scheduling/Scheduler.hpp"
#include "system/BlockingAPI.hpp"

#include <DataAccessRegistration.hpp>
#include <DataAccessRegistrationImplementation.hpp>




// For each CUDA device task a CUDA stream is required for the asynchronous
// launch; To ensure kernel completion a CUDA event is 'recorded' on the stream
// right after the kernel is queued. Then when a cudaEventQuery call returns
// succefully, we can be sure that the kernel execution (and hence the task)
// has finished.

// Get a new CUDA event and queue it in the stream the task has launched
void CUDAAccelerator::postRunTask(Task *)
{
}


AcceleratorEvent *CUDAAccelerator::createEvent(std::function<void((AcceleratorEvent*))> onCompletion = [](AcceleratorEvent*){})
{
	nanos6_cuda_device_environment_t &env =	getCurrentTask()->getDeviceEnvironment().cuda;

	return (AcceleratorEvent *)new CUDAAcceleratorEvent(onCompletion,  env.stream, _cudaStreamPool);
}

void CUDAAccelerator::destroyEvent(AcceleratorEvent* event)
{
	delete (CUDAAcceleratorEvent *) event;
}


void CUDAAccelerator::callBody(Task * task)
{
	size_t tableSize = 0;
	nanos6_address_translation_entry_t stackTranslationTable[SymbolTranslation::MAX_STACK_SYMBOLS];
	nanos6_address_translation_entry_t *translationTable =	SymbolTranslation::generateTranslationTable(task, _computePlace, stackTranslationTable,	tableSize);
	task->body(translationTable);
	if (tableSize > 0)	MemoryAllocator::free(translationTable, tableSize);
}

void CUDAAccelerator::preRunTask(Task *task)
{
	// Prefetch available memory locations to the GPU
	nanos6_cuda_device_environment_t &env =	task->getDeviceEnvironment().cuda;

	DataAccessRegistration::processAllDataAccesses(task,
		[&](const DataAccess *access) -> bool {
			if (access->getType() != REDUCTION_ACCESS_TYPE && !access->isWeak()) {
				CUDAFunctions::cudaDevicePrefetch(
					access->getAccessRegion().getStartAddress(),
					access->getAccessRegion().getSize(),
					_deviceHandler, env.stream,
					access->getType() == READ_ACCESS_TYPE);
			}
			return true;
		}
	);
}
