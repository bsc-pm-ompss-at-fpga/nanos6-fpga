/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/
#ifndef DEVICE_DIRECTORY_HPP
#define DEVICE_DIRECTORY_HPP

#include <functional>
#include <mutex>

#include "tasks/Task.hpp"

#include "IntervalMap.hpp"
#include "hardware/device/Accelerator.hpp"
#include "hardware/device/AcceleratorStream.hpp"
#include "lowlevel/EnvironmentVariable.hpp"




using checker = int;

struct SymbolRepresentation;
class DeviceDirectory
{
    public:
        void print();


private:
    std::mutex device_directory_mutex;

    std::vector<Accelerator *> _accelerators;
    std::vector<std::shared_ptr<DeviceAllocation>> _symbol_allocations;

    Task *_current_task;
    IntervalMap *_dirMap;

    AcceleratorStream _taskwaitStream;
	std::atomic<bool> _stopService;
	std::atomic<bool> _finishedService;
    
    public:

    DeviceDirectory(const std::vector<Accelerator*>& accels);
    ~DeviceDirectory();
    //This function tries to register the task dependences in the directory. 
    //As a parameter it accepts the task which dependences are going to be registered, and two steps that will be used for synchronization
    //This function can fail if there is no space for allocation in the device. In this case, the user is responsible of calling the free
    //memory function.
    bool register_regions(Task *task);

    //This function registers an accelerator to the directory.
    //Each accelerator is defined by it's address space, if more than one accelerator share 
    //the same address space, they should be enclosed inside one accelerator object and
    //do the selection of which accelerator runs the task there. This is for simplicity and
    //directory performance. Each accelerator adds into the size of the internal structures
    int registerAddressSpace(Accelerator *accel);

    //This function setups a taskwait. It decides wether or not a copy must be perform and only
    //frees the dependences of the task once the data is valid on the home.
    //Right now only SMP Home is functional
   // void taskwait(DataAccessRegion taskwaitRegion, void *stream, Task* task);

    void taskwait(const DataAccessRegion& taskwaitRegion, std::function<void()> release);
    //returns the accelerator given a directory handle
    Accelerator *getAcceleratorByHandle(int handle);


    void initializeTaskwaitService()
    {
        // Spawn service function
        SpawnFunction::spawnFunction(
            [](void* t)
            {
                DeviceDirectory* devDir = ((DeviceDirectory*) t);
                while(!devDir->shouldStopService())
                {
                    while(devDir->_taskwaitStream.streamPendingExecutors())
                        devDir->_taskwaitStream.streamServiceLoop();
                    BlockingAPI::waitForUs(300);
                }
            }, this,
            [](void* t)
            {
                ((DeviceDirectory*) t)->_finishedService = true;
            }, this,
            "Taskwait service", false
            );
    }

    void shutdownTaskwaitService()
    {
        _stopService = true;
        // Wait until the service completes
        while (!_finishedService);
    }


    //This generates a setValid function for a directory range.
    AcceleratorStream::checker setValid(int handler, const std::pair<uintptr_t,uintptr_t> & entry);

private:

	inline bool shouldStopService() const
	{
		return _stopService.load(std::memory_order_relaxed);
	}


    //This function makes a copy betwen two devices, if you are implementing a new device, you must add here the 
    //interaction between the devices.
    AcceleratorStream::activatorReturnsChecker generateCopy(const DirectoryEntry &entry, int dstHandle, Task* task);

    //This function forwards to processSymbolRegions the task dependences of each type
    void processSymbol(SymbolRepresentation &symbol, std::shared_ptr<DeviceAllocation>& deviceAllocation);
    //This function checks for old allocations or the necessity of a reallocation, after forwards each region of the symbol
    //to check for validity
    void processSymbolRegions(const std::vector<DataAccessRegion> &dataAccessVector,std::shared_ptr<DeviceAllocation>& region, DataAccessType RW_TYPE);

    //generates the needed copies in need of reallocation/old allocations
    void processRegionWithOldAllocation(int handle, DirectoryEntry &entry, std::shared_ptr<DeviceAllocation>& region, DataAccessType type);


    //Updates the directory and generates the copies and steps needed for perform the copies and ensure the coherence of the directory
    void processSymbolRegions_in(int handle, DirectoryEntry &entry);
    void processSymbolRegions_out(int handle, DirectoryEntry &entry);
    void processSymbolRegions_inout(int handle, DirectoryEntry &entry);

    //This function gets the current allocation for a symbol range. In case it doesn't exists or it's not enough to
    //contain the new symbol, it tries to allocate a new device address.
    std::shared_ptr<DeviceAllocation> getDeviceAllocation(int handle, SymbolRepresentation &symbol);

    //This function allocates a new device address for a directory range.
    std::shared_ptr<DeviceAllocation> getNewAllocation(int handle, SymbolRepresentation &symbol );


    //Since two tasks could want to use the same symbol or region, we must ensure that the copy is done only one time.
    //We cannot setup the directory at the moment of generating the copies because if another task wants to use the data
    //this could mean that the data is not valid.
    //For fixing this we create a transitory step, and the task will need to wait for directory validity. 
    AcceleratorStream::checker awaitToValid(int handler, const std::pair<uintptr_t,uintptr_t> & entry);


};



namespace DeviceDirectoryInstance
{
    extern DeviceDirectory *instance;
    extern bool useDirectory;
    extern bool noflush;
};

#endif //DEVICE_DIRECTORY_HPP