/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/
#ifndef DEVICE_DIRECTORY_HPP
#define DEVICE_DIRECTORY_HPP


#include <dependencies/DataAccessType.hpp>


#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>

class Accelerator;
class AcceleratorStream;
class Task;
class IntervalMap;
class DirectoryEntry;
class DeviceAllocation;
class DataAccessRegion;

struct SymbolRepresentation;

class DeviceDirectory
{
private:
    std::mutex _device_directory_mutex;

    std::vector<Accelerator *> _accelerators;

    std::vector<std::vector<int>> _directory_handles_devicetype_deviceid;
    std::vector<std::shared_ptr<DeviceAllocation>> _symbol_allocations;

    IntervalMap *_dirMap;

    AcceleratorStream *_taskwaitStream;
	std::atomic<bool> _stopService;
	std::atomic<bool> _finishedService;
    
    public:

    DeviceDirectory(const std::vector<Accelerator*>& accels);
    ~DeviceDirectory();

    void print();

    //This function computes the affinity for a device over a region, if there is no affinity, it returns a pseudo-random one.
    //It only checks for IN and INOUT tasks, since OUT tasks only require an allocation, but not a copy.
    int computeAffininty(std::vector<SymbolRepresentation>& symbolInfo, int deviceType);

    //This function tries to register the task dependences in the directory. 
    //As a parameter it accepts the task which dependences are going to be registered, and two steps that will be used for synchronization
    //This function can fail if there is no space for allocation in the device. In this case, the user is responsible of calling the free
    //memory function.
    bool register_regions(Task *task);
    bool register_regions(std::vector<SymbolRepresentation>& symbolInfo, Accelerator* accelerator, AcceleratorStream* acceleratorStream, void* copy_extra);

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
    Accelerator *getAcceleratorByHandle(const int handle);


    void initializeTaskwaitService();

    void shutdownTaskwaitService();

    IntervalMap* getIntervalMap();

private:

	inline bool shouldStopService() const;

    //This function makes a copy betwen two devices, if you are implementing a new device, you must add here the 
    //interaction between the devices.
    void generateCopy(AcceleratorStream* acceleratorStream, const DirectoryEntry &entry, int dstHandle, void* copy_extra);

    //This function forwards to processSymbolRegions the task dependences of each type
    void processSymbol(const int handle, void *copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, SymbolRepresentation &symbol, std::shared_ptr<DeviceAllocation>& deviceAllocation);
    //This function checks for old allocations or the necessity of a reallocation, after forwards each region of the symbol
    //to check for validity
    void processSymbolRegions(const int handle, void *copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, const std::vector<DataAccessRegion> &dataAccessVector, std::shared_ptr<DeviceAllocation>& region, DataAccessType RW_TYPE);

    //generates the needed copies in need of reallocation/old allocations
    void processRegionWithOldAllocation(const int handle, void* copy_extra, AcceleratorStream* acceleratorStream, DirectoryEntry &entry, std::shared_ptr<DeviceAllocation> &region, DataAccessType type);


    //Updates the directory and generates the copies and steps needed for perform the copies and ensure the coherence of the directory
    void processSymbolRegions_in(const int handle, void* copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, DirectoryEntry &entry);
    void processSymbolRegions_out(const int handle, DirectoryEntry &entry);
    void processSymbolRegions_inout(const int handle, void* copy_extra,  Accelerator* accelerator, AcceleratorStream* acceleratorStream, DirectoryEntry &entry);

    //This function gets the current allocation for a symbol range. In case it doesn't exists or it's not enough to
    //contain the new symbol, it tries to allocate a new device address.
    std::shared_ptr<DeviceAllocation> getDeviceAllocation(const int handle, SymbolRepresentation &symbol);

    //This function allocates a new device address for a directory range.
    std::shared_ptr<DeviceAllocation> getNewAllocation(const int handle, SymbolRepresentation &symbol );


    //Since two tasks could want to use the same symbol or region, we must ensure that the copy is done only one time.
    //We cannot setup the directory at the moment of generating the copies because if another task wants to use the data
    //this could mean that the data is not valid.
    //For fixing this we create a transitory step, and the task will need to wait for directory validity. 
    void awaitToValid(AcceleratorStream* acceleratorStream, const int handler, const std::pair<uintptr_t,uintptr_t> & entry);


};



namespace DeviceDirectoryInstance
{
    extern DeviceDirectory *instance;
    extern bool useDirectory;
};

#endif //DEVICE_DIRECTORY_HPP