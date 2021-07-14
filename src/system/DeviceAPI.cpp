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
#include "hardware/device/AcceleratorStream.hpp"
#include "dependencies/DataAccessType.hpp"

#include "hardware/device/directory/DeviceDirectory.hpp"

extern "C" {


void nanos6_enable_noflush()
{
    DeviceDirectoryInstance::noflush = true;
}

void nanos6_disable_noflush()
{
    DeviceDirectoryInstance::noflush = false;
}


void nanos6_print_directory()
{
    DeviceDirectoryInstance::instance->print();
}

int nanos6_get_device_num(nanos6_device_t device)
{
    return HardwareInfo::getDeviceInfo(device)==nullptr?0:HardwareInfo::getDeviceInfo(device)->getComputePlaceCount();
}

void nanos6_device_memcpy(nanos6_device_t device, int device_id, void* host_ptr, size_t size)
{
    
    void *nullargs = nullptr;
    size_t argsBlockSize = 0;
    
    nanos6_task_info_t taskinfo; taskinfo.num_symbols = 1;

    nanos6_task_invocation_info_t taskInvokationInfo;
    Task* parent = nullptr;
    Instrument::task_id_t instrumentationTaskId;
    size_t flags = 0;
    TaskDataAccessesInfo taskAccessInfo(1);
    void* taskCountersAddress = nullptr;
    void* taskStatistics = nullptr;
    

    AcceleratorStream accelStream;

    
    nanos6_task_implementation_info_t impl_info;
    
    impl_info.device_type_id = device;
    
    taskinfo.implementations=&impl_info;

    Task task(
        nullargs,
        argsBlockSize,
        &taskinfo,
        &taskInvokationInfo,
        parent,
        instrumentationTaskId,
        flags,
        taskAccessInfo,
        taskCountersAddress,
        taskStatistics
    );



    task.setAccelerator(HardwareInfo::getDeviceInfo(device)->getAccelerators()[device_id]);


    task.setAcceleratorStream(&accelStream);

    task.addAccessToSymbol(0, {host_ptr, size}, READWRITE_ACCESS_TYPE);

    DeviceDirectoryInstance::instance->register_regions(&task);
    while(accelStream.streamPendingExecutors()) accelStream.streamServiceLoop();
  

}


}
