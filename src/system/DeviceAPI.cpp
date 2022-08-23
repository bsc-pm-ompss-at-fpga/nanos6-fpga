/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#include <nanos6/devices.h>
#include <iostream>

#include <InstrumentTaskId.hpp>
#include <TaskDataAccessesInfo.hpp>

#include "tasks/TaskImplementation.hpp"

#include "hardware/HardwareInfo.hpp"

#include "hardware/device/directory/DeviceDirectory.hpp"
#include "hardware/device/directory/IntervalMap.hpp"
#include "hardware/device/directory/DirectoryEntry.hpp"

#include "hardware/device/Accelerator.hpp"
#include "hardware/device/AcceleratorStream.hpp"

#include <Dependencies.hpp>
#include "dependencies/DataAccessType.hpp"

#include "ompss/AddTask.hpp"
extern "C" {


void nanos6_print_directory()
{
    DeviceDirectoryInstance::instance->print();
}

int nanos6_get_device_num(nanos6_device_t device)
{
    return HardwareInfo::getDeviceInfo(device)==nullptr?0:HardwareInfo::getDeviceInfo(device)->getComputePlaceCount();
}


nanos6_task_invocation_info_t global_device_api_src{"Directory Home API src"};

struct taskArgsDeps
{
    nanos6_device_t device;
    int device_id;
    void* host_ptr;
    size_t size;
};


Task* createTaskWithInDep(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
{
    taskArgsDeps argsBlock{device, device_id, host_ptr, size};

    auto depinfoRegister = [](void* args, void *, void *handler)
    {
        taskArgsDeps *_argsBlock = (taskArgsDeps*) args;
        nanos6_register_read_depinfo(handler, _argsBlock->host_ptr, _argsBlock->size, 0);
    };

	nanos6_task_implementation_info_t *taskImplementationInfo;
	taskImplementationInfo = new nanos6_task_implementation_info_t;

	taskImplementationInfo->device_type_id = nanos6_device_t::nanos6_host_device;
	taskImplementationInfo->task_type_label = "Nanos6 API\n";
	taskImplementationInfo->declaration_source = "Spawned HOME API task\n";
	taskImplementationInfo->get_constraints = nullptr;

	nanos6_task_info_t *taskInfo = new nanos6_task_info_t;

	taskInfo->num_symbols = 1;
	taskInfo->implementations = taskImplementationInfo;
	taskInfo->implementation_count = 1;

	taskInfo->register_depinfo = depinfoRegister;

	//finish callback
	taskInfo->destroy_args_block = [](void*){};

    Task* task =  AddTask::createTask(
        taskInfo,
        &global_device_api_src,
        nullptr,
        sizeof(taskArgsDeps),
        nanos6_waiting_task,
        1,//num dependencies
        false);//from_user_code

    *((taskArgsDeps*) task->getArgsBlock()) = argsBlock;
    task->setIgnoreDirectory(true);
    
    return task;
}

    void nanos6_set_noflush(void* host_ptr, size_t size)
    {
        const DataAccessRegion dar(host_ptr, size);
        const auto set_noflush_lambda = [=](DirectoryEntry *entry) {
                entry->setNoFlushState(true);
                return true;
        };
        DeviceDirectoryInstance::instance->getIntervalMap()->addRange(dar);
        DeviceDirectoryInstance::instance->getIntervalMap()->applyToRange(dar, set_noflush_lambda);
    }

    void nanos6_set_home(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
    {    
        
        Task* task = createTaskWithInDep(device, device_id, host_ptr, size);
        
        task->getTaskInfo()->implementations[0].run = [](void* args, void*, nanos6_address_translation_entry_t*)
        {
            taskArgsDeps *_argsBlock = (taskArgsDeps*) args;
            DataAccessRegion dar(_argsBlock->host_ptr, _argsBlock->size);
            DeviceDirectoryInstance::instance->getIntervalMap()->addRange(dar);

            const auto directoryHandle = HardwareInfo::getDeviceInfo(_argsBlock->device)->getAccelerators()[_argsBlock->device_id]->getDirectoryHandler();
            const auto set_home_lambda = [=](DirectoryEntry *entry) {
                entry->setHome(directoryHandle);
                return true;
            };
            DeviceDirectoryInstance::instance->getIntervalMap()->applyToRange(dar, set_home_lambda);
        };


        WorkerThread *workerThread = WorkerThread::getCurrentWorkerThread();
        assert(workerThread != nullptr);

        Task *parent = workerThread->getTask();
        assert(parent != nullptr);

        AddTask::submitTask(task,parent);
    }




    void nanos6_device_memcpy(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
    {    
        
        Task* task = createTaskWithInDep(device, device_id, host_ptr, size);
        
        task->getTaskInfo()->implementations[0].run = [](void* args, void*, nanos6_address_translation_entry_t*)
        {
            taskArgsDeps *_argsBlock = (taskArgsDeps*) args;
            DataAccessRegion dar(_argsBlock->host_ptr, _argsBlock->size);

            AcceleratorStream accelStream;
            Accelerator* accelerator = HardwareInfo::getDeviceInfo(_argsBlock->device)->getAccelerators()[_argsBlock->device_id];
            std::vector<SymbolRepresentation> _symRep;
            nanos6_address_translation_entry_t t;
            _symRep.emplace_back(&t);
            _symRep[0].addDataAccess(dar, READ_ACCESS_TYPE);
            DeviceDirectoryInstance::instance->register_regions(_symRep, accelerator, &accelStream, nullptr);
            while(accelStream.streamPendingExecutors()) accelStream.streamServiceLoop();
            
        };


        WorkerThread *workerThread = WorkerThread::getCurrentWorkerThread();
        assert(workerThread != nullptr);

        Task *parent = workerThread->getTask();
        assert(parent != nullptr);

        AddTask::submitTask(task,parent);
    }

}
