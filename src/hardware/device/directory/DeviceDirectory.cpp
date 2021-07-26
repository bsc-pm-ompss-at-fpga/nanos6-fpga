/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/
#include "DeviceDirectory.hpp"
#include "DirectoryEntry.hpp"
#include "DeviceDirectory.hpp"

#include "hardware/device/DeviceAllocation.hpp"
#include "hardware/device/Accelerator.hpp"
#include "hardware/device/AcceleratorEvent.hpp"

#include "api/nanos6/library-mode.h"
#include "lowlevel/FatalErrorHandler.hpp"


#include "IntervalMap.hpp"
static constexpr int SMP_HANDLER = 0;
static constexpr int NO_DEVICE = -1;
namespace DeviceDirectoryInstance {
	DeviceDirectory *instance = nullptr;
	bool useDirectory =	ConfigVariable<bool>("devices.directory");
    bool noflush = false;
};


int DeviceDirectory::computeAffininty(std::vector<SymbolRepresentation>& symbolInfo, int deviceType )
{
	static unsigned int affinityCounter = 0;

	const auto handle_to_accel_num = [&](int handle) -> int{ return _accelerators[handle]->getVendorDeviceId();};

	std::unordered_map<int, int> affinity;

	const std::vector<int>& accelerator_handles = _directory_handles_devicetype_deviceid[deviceType];
	
	if(accelerator_handles.size() == 1) 
		return handle_to_accel_num(accelerator_handles[0]);


	const auto compare_lambda = [](const decltype(affinity)::value_type &p1, const decltype(affinity)::value_type &p2)
	{
		return p1.second < p2.second;
	};

	auto affinity_fun = [&](DirectoryEntry* centry) -> bool
	{
		for(const auto& handle : accelerator_handles)
			if(centry->isValid(handle)) affinity[handle] += centry->getSize();

		return true;
	};


	for(auto& symbol : symbolInfo)
	{
		for(auto& in_region : symbol.getInputRegions())
			_dirMap->applyToRange(in_region,affinity_fun);
		for(auto& inout_region : symbol.getInputOutputRegions())
			_dirMap->applyToRange(inout_region, affinity_fun);
		//there are no copies involved in an out region
	}

	if(affinity.empty()) 
		return handle_to_accel_num(accelerator_handles[(affinityCounter++)%accelerator_handles.size()]);

	return handle_to_accel_num(std::max_element(
		std::begin(affinity), std::end(affinity), compare_lambda
	)->first);

}

DeviceDirectory::DeviceDirectory(const std::vector<Accelerator *> &accels) :
	_accelerators(accels),
	_directory_handles_devicetype_deviceid(nanos6_device_type_num),
	_dirMap(new IntervalMap(accels.size())),
	_taskwaitStream(), _stopService(false), _finishedService(false)
{
	for (size_t i = 0; i < _accelerators.size(); ++i)
	{
		_accelerators[i]->setDirectoryHandle(i);
		int deviceType = (int) _accelerators[i]->getDeviceType();
		std::vector<int> &deviceVec = _directory_handles_devicetype_deviceid[deviceType];
		deviceVec.push_back(i);
	}
	

}

AcceleratorStream::activatorReturnsChecker DeviceDirectory::generateCopy(const DirectoryEntry &entry, int dstHandle, void *copy_extra)
{
	const int srcHandle =  entry.getFirstValidLocation();
	const uintptr_t smpAddr = (uintptr_t)entry.getDataAccessRegion().getStartAddress();
	const uintptr_t srcAddr = srcHandle > 0 ? entry.getTranslation(srcHandle, smpAddr) : smpAddr;
	const uintptr_t size = entry.getDataAccessRegion().getSize();
	const uintptr_t dstAddr = dstHandle > 0 ? entry.getTranslation(dstHandle, smpAddr) : smpAddr;

	Accelerator *srcA = getAcceleratorByHandle(srcHandle);
	Accelerator *dstA = getAcceleratorByHandle(dstHandle);

	if (srcA->getDeviceType() == nanos6_host_device) {
		if (dstA->getDeviceType() == nanos6_cuda_device)
			return dstA->copy_in((void *)dstAddr, (void *)srcAddr, size, copy_extra);
		else if (dstA->getDeviceType() == nanos6_fpga_device)
			return dstA->copy_in((void *)dstAddr, (void *)srcAddr, size, copy_extra);
	}

	if (srcA->getDeviceType() == nanos6_cuda_device) {
		if (dstA->getDeviceType() == nanos6_host_device)
			return srcA->copy_out((void *)dstAddr, (void *)srcAddr, size, nullptr); //since it's from GPU->CPU, there is no cuda stream associated to the task
		else if (dstA->getDeviceType() == nanos6_cuda_device)
			return dstA->copy_between((void *)dstAddr, dstA->getDeviceHandler(), (void *)srcAddr, srcA->getDeviceHandler(), size, copy_extra);
	}

	if (srcA->getDeviceType() == nanos6_fpga_device) {
		if (dstA->getDeviceType() == nanos6_host_device)
			return srcA->copy_out((void *)dstAddr, (void *)srcAddr, size, copy_extra);
	}

	return [a = srcA->getDeviceType(), b = dstA->getDeviceType(), hand = dstHandle, vloc = entry._valid_locations, modloc = entry.getModifiedLocation()]() {  
		printf("ERROR COPY NOT SUPPORTED BETWEEN src %d dst %d destinyHandle=%d\n",a,b,hand );
		printf("MOD: %d  ", modloc);
		for(unsigned i = 0; i<vloc.size(); ++i) printf(" [%d] %d | ", i, vloc[i]);
		printf("\n");
		return [](){return true;}; 
	};
}

Accelerator *DeviceDirectory::getAcceleratorByHandle(const int handle)
{
	return _accelerators[handle];
}


bool DeviceDirectory::register_regions(std::vector<SymbolRepresentation>& symbolInfo, Accelerator* accelerator, AcceleratorStream* acceleratorStream, void* copy_extra)
{
	if (symbolInfo.size() == 0) return true;

	const int handle = accelerator->getDirectoryHandler();

	std::lock_guard<std::mutex> guard(device_directory_mutex);

	_symbol_allocations.clear();
	_symbol_allocations.resize(symbolInfo.size());

	for (size_t i = 0; i < symbolInfo.size(); ++i) {
		auto allocation = getDeviceAllocation(handle, symbolInfo[i]);
		if (allocation == nullptr)
			return false;

		_symbol_allocations[i] = allocation;
	}


	for (size_t i = 0; i < symbolInfo.size(); ++i)
		processSymbol(handle, copy_extra, accelerator, acceleratorStream, symbolInfo[i], _symbol_allocations[i]);

	_symbol_allocations.clear();

	return true;
}

bool DeviceDirectory::register_regions(Task *task)
{
	if(task->ignoreDirectory()) return true;

	if (task->getDeviceType() == nanos6_host_device)
		task->setAccelerator(getAcceleratorByHandle(0));
	return register_regions(task->getSymbolInfo(), task->getAccelerator(), task->getAcceleratorStream(), (void*) task);
}

//SYMBOL PROCESSING

void DeviceDirectory::processSymbol(const int handle, void* copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, SymbolRepresentation &symbol, std::shared_ptr<DeviceAllocation> &deviceAllocation)
{
	symbol.setSymbolTranslation(deviceAllocation);
	processSymbolRegions(handle, copy_extra,  accelerator, acceleratorStream, symbol.getInputRegions(), deviceAllocation, READ_ACCESS_TYPE);
	processSymbolRegions(handle, copy_extra,  accelerator, acceleratorStream, symbol.getOutputRegions(), deviceAllocation, WRITE_ACCESS_TYPE);
	processSymbolRegions(handle, copy_extra,  accelerator, acceleratorStream, symbol.getInputOutputRegions(), deviceAllocation, READWRITE_ACCESS_TYPE);
}

void DeviceDirectory::processSymbolRegions(const int handle, void* copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, const std::vector<DataAccessRegion> &dataAccessVector, std::shared_ptr<DeviceAllocation> &region, DataAccessType RW_TYPE)
{
	const auto in_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, copy_extra, acceleratorStream, *entry, region, RW_TYPE);
		processSymbolRegions_in(handle, copy_extra, accelerator, acceleratorStream, *entry);
		return true;
	};

	const auto out_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, copy_extra, acceleratorStream, *entry, region, RW_TYPE);
		processSymbolRegions_out(handle, *entry);
		return true;
	};
	const auto inout_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, copy_extra, acceleratorStream, *entry, region, RW_TYPE);
		processSymbolRegions_inout(handle, copy_extra, accelerator, acceleratorStream, *entry);
		return true;
	};

	for (const DataAccessRegion &dependencyRegion : dataAccessVector) {
		_dirMap->addRange(dependencyRegion);
		if (RW_TYPE == READ_ACCESS_TYPE)
			_dirMap->applyToRange(dependencyRegion, in_lambda);
		else if (RW_TYPE == WRITE_ACCESS_TYPE)
			_dirMap->applyToRange(dependencyRegion, out_lambda);
		else
			_dirMap->applyToRange(dependencyRegion, inout_lambda);
	}
}

//ALLOCATIONS

std::shared_ptr<DeviceAllocation> DeviceDirectory::getNewAllocation(const int handle, SymbolRepresentation &symbol)
{
	std::pair<std::shared_ptr<DeviceAllocation>, bool> deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

	if (!deviceAllocation.second) {
		_dirMap->freeAllocationsForHandle(handle, _symbol_allocations);
		deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

		FatalErrorHandler::failIf(!deviceAllocation.second, "Device Allocation: Out of space in device memory after already trying to free unused memory");
	}

	_dirMap->addRange(deviceAllocation.first->getHostRegion());

	_dirMap->applyToRange(deviceAllocation.first->getHostRegion(), [=, &deviceAllocation](DirectoryEntry *entry) {
		if (entry->getDeviceAllocation(handle) == nullptr)
			entry->setDeviceAllocation(handle, deviceAllocation.first);
		return true;
	});

	return deviceAllocation.first;
}

std::shared_ptr<DeviceAllocation> DeviceDirectory::getDeviceAllocation(const int handle, SymbolRepresentation &symbol)
{
	_dirMap->addRange(symbol.getHostRegion());

	auto host_region = symbol.getHostRegion();
	auto iter = _dirMap->getIterator(host_region);
	std::shared_ptr<DeviceAllocation> &deviceAllocation = iter->second->getDeviceAllocation(handle);


	if (deviceAllocation != nullptr) {
		const auto symbol_end_address = (uintptr_t)symbol.getEndAddress();
		const auto checkIfRegionIsAllocated = [=](DirectoryEntry *entry) 
		{
			const auto &allocation = entry->getDeviceAllocation(handle);
			if (allocation != deviceAllocation)
				return false;
			if (allocation->getHostEnd() < symbol_end_address)
				return false;
			return true;
		};
		const bool allocated = _dirMap->applyToRange(symbol.getHostRegion(), checkIfRegionIsAllocated);
		if (allocated)
			return deviceAllocation;
	}

	return getNewAllocation(handle, symbol);
}

//Inner Symbols

void DeviceDirectory::processSymbolRegions_out(const int handle, DirectoryEntry &entry)
{
	entry.clearValid();
	entry.setModified(handle);
	entry.setValid(handle);
}

AcceleratorStream::checker DeviceDirectory::awaitToValid(const int handler,  const std::pair<uintptr_t,uintptr_t> & entry)
{
	return [itvMap = _dirMap, left = entry.first, right = entry.second, handler = handler]() 
	{
		return itvMap->applyToRange({left, right}, 
		[=](DirectoryEntry *dirEntry) 
		{ 
			return dirEntry->isValid(handler); 
		});
	};
}

AcceleratorStream::checker DeviceDirectory::setValid(const int handler, const std::pair<uintptr_t,uintptr_t> &entry)
{
	return [itvMap = _dirMap, left = entry.first, right = entry.second,  handler = handler]
	{
		itvMap->applyToRange({left, right}, [=](DirectoryEntry *dirEntry) {dirEntry->setValid(handler); return true; });
		return true;
	};
}

void DeviceDirectory::processSymbolRegions_inout(const int handle, void* copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream, DirectoryEntry &entry)
{
	
	if (entry.isPending(handle))
		return acceleratorStream->addOperation(awaitToValid(handle, {entry.getLeft(), entry.getRight()}));

	if (entry.isValid(handle)) {
		entry.clearValid();
		entry.setModified(handle);
		entry.setValid(handle);
		return;
	}

	acceleratorStream->addOperation(generateCopy(entry, handle, copy_extra));

	entry.clearValid();
	entry.setModified(handle);
	entry.setPending(handle);

	accelerator->createEvent([left =entry.getLeft(), right = entry.getRight() , fun = setValid(handle, {entry.getLeft(), entry.getRight()}), accelerator](AcceleratorEvent *own) 
	{
		fun();
		accelerator->destroyEvent(own);
		return true;
	})->record(acceleratorStream);
}

void DeviceDirectory::processSymbolRegions_in(const int handle, void* copy_extra, Accelerator* accelerator, AcceleratorStream* acceleratorStream,  DirectoryEntry &entry)
{

	if (entry.isValid(handle))
		return;

	if (entry.isPending(handle))
		return acceleratorStream->addOperation(awaitToValid(handle, {entry.getLeft(), entry.getRight()}));


	acceleratorStream->addOperation(generateCopy(entry, handle, copy_extra));

	entry.setPending(handle);
   if (entry.getModifiedLocation() >= 0) {
	   entry.setValid(entry.getModifiedLocation());
   }
	entry.setModified(NO_DEVICE);

	accelerator->createEvent([accelerator, fun = setValid(handle,  {entry.getLeft(), entry.getRight()}) ](AcceleratorEvent *own) 
	{
		fun();
		accelerator->destroyEvent(own);
		return true;
	})->record(acceleratorStream);
}

//the control logic that comes afterwards will enqueue a copy operation from SMP to the device again
//since a stream is sequential, this is valid.
void DeviceDirectory::processRegionWithOldAllocation(const int handle, void* copy_extra, AcceleratorStream* acceleratorStream, DirectoryEntry &entry, std::shared_ptr<DeviceAllocation> &region, DataAccessType type)
{
	if (entry.getDeviceAllocation(handle) != region && handle != SMP_HANDLER) {
		if (entry.getModifiedLocation() == handle && type != WRITE_ACCESS_TYPE) {
			//std::cout<<"WARNING: RESIZING ENTRY TO FIT SYMBOL[!]!"<<std::endl;
			acceleratorStream->addOperation(generateCopy(entry, SMP_HANDLER, copy_extra));
			acceleratorStream->addOperation([keepalive = entry.getDeviceAllocation(handle)]{ std::ignore = keepalive; return true;});
			entry.setModified(SMP_HANDLER);
		}
		//std::cout<<"WARNING2: INVALIDATING OLD ENTRY TO FIT SYMBOL[!]!"<<std::endl;
		entry.setInvalid(handle);
		entry.setDeviceAllocation(handle, region);
	}
}


void DeviceDirectory::taskwait(const DataAccessRegion &taskwaitRegion, std::function<void()> release)
{

	if(DeviceDirectoryInstance::noflush)
	{
		release();
		return;
	}

	_dirMap->applyToRange(taskwaitRegion, 
	[&](DirectoryEntry *entry) 
	{
		if (!entry->isValid(entry->getHome()))
		{
			_taskwaitStream.addOperation(generateCopy(*entry, entry->getHome(), nullptr)());
		}
		return true;
	});

	(new AcceleratorEvent(
		[=](AcceleratorEvent* own) 
		{
		/*release taskwait*/

		_dirMap->applyToRange(
					taskwaitRegion,
					[&](DirectoryEntry *entry) 
					{
						//clearing allocations is not easy, we can make it so it stays allocated
						//and if we fail to allocate afterwards, try to cleanup
						entry->clearAllocations(entry->getHome());//ignore home node
						entry->clearValid();
						entry->setModified(-1); //now is not modified
						entry->setValid(entry->getHome());//it's valid on the home node
						return true;
					});

		_dirMap->remRange(taskwaitRegion, SMP_HANDLER);

		release();
		delete own;
		}
	))->record(&_taskwaitStream);
	

}


DeviceDirectory::~DeviceDirectory()
{
	for(size_t i = 1; i < _accelerators.size(); ++i)
	_dirMap->freeAllocationsForHandle(i, {});

	delete _dirMap;
}

void DeviceDirectory::print()
{


	auto printEntry = [](const std::pair<uintptr_t, DirectoryEntry*>& entry)
	{

		auto indexToString = [](auto i) -> std::string {
			if (i == DirectoryEntry::VALID)
				return "V";
			else if (i == DirectoryEntry::INVALID)
				return "I";
			else
				return "P";
		};

		
		auto left = entry.second->getRange().first;
		auto right = entry.second->getRange().second;
		printf("---------[%p,%p] \t", (void *)left, (void *)right);
		printf("%d\n", entry.second->_modified_location);

		auto& locations = entry.second->_valid_locations;
		auto& allocations = entry.second->_perDeviceAllocation;
		for(size_t i = 0; i < locations.size();++i)
		{
			printf("\t[%lu] ", i);
			printf("%s\t", indexToString(locations[i]).c_str());
			if (allocations[i] != nullptr)
				printf("[%p,%p](count: %lX) HOME: %X\t", (void *)allocations[i]->getDeviceBase(),(void *)allocations[i]->getDeviceEnd(),allocations[i].use_count(), entry.second->getHome());
			else printf("none\t");
			printf("\n");;
		}
		printf("-------------------------------\n");

	};

	printf("BEGIN OF DIR\n\n");

	std::for_each(std::begin(_dirMap->_inner_m),std::end(_dirMap->_inner_m),printEntry);

	printf("END OF DIR\n\n");

}
