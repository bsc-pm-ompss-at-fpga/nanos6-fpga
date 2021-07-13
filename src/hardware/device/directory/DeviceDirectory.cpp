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

static constexpr int SMP_HANDLER = 0;
static constexpr int NO_DEVICE = -1;
namespace DeviceDirectoryInstance {
	DeviceDirectory *instance = nullptr;
};




DeviceDirectory::DeviceDirectory(const std::vector<Accelerator *> &accels) :
	_accelerators(accels),
	_dirMap(new IntervalMap(accels.size())),
	_taskwaitStream(), _stopService(false), _finishedService(false)
{
	for (size_t i = 0; i < _accelerators.size(); ++i)
		_accelerators[i]->setDirectoryHandle(i);

}

AcceleratorStream::activatorReturnsChecker DeviceDirectory::generateCopy(const DirectoryEntry &entry, int dstHandle, Task *task)
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
			return dstA->copy_in((void *)dstAddr, (void *)srcAddr, size, task);
		else if (dstA->getDeviceType() == nanos6_fpga_device)
			return dstA->copy_in((void *)dstAddr, (void *)srcAddr, size, task);
	}

	if (srcA->getDeviceType() == nanos6_cuda_device) {
		if (dstA->getDeviceType() == nanos6_host_device)
			return srcA->copy_out((void *)dstAddr, (void *)srcAddr, size, nullptr); //since it's from GPU->CPU, there is no cuda stream associated to the task
		else if (dstA->getDeviceType() == nanos6_cuda_device)
			return dstA->copy_between((void *)dstAddr, dstA->getDeviceHandler(), (void *)srcAddr, srcA->getDeviceHandler(), size, task);
	}

	if (srcA->getDeviceType() == nanos6_fpga_device) {
		if (dstA->getDeviceType() == nanos6_host_device)
			return srcA->copy_out((void *)dstAddr, (void *)srcAddr, size, task);
	}

	return [a = srcA->getDeviceType(), b = dstA->getDeviceType(), hand = dstHandle, vloc = entry._valid_locations, modloc = entry.getModifiedLocation()]() {  
		printf("ERROR COPY NOT SUPPORTED BETWEEN src %d dst %d destinyHandle=%d\n",a,b,hand );
		printf("MOD: %d  ", modloc);
		for(unsigned i = 0; i<vloc.size(); ++i) printf(" [%d] %d | ", i, vloc[i]);
		printf("\n");
		return [](){return true;}; 
	};
}

Accelerator *DeviceDirectory::getAcceleratorByHandle(int handle)
{
	return _accelerators[handle];
}

bool DeviceDirectory::register_regions(Task *task)
{
	auto &sym = task->getSymbolInfo();
	if (sym.size() == 0)
		return true;

	std::lock_guard<std::mutex> guard(device_directory_mutex);

	_current_task = task;

	if (_current_task->getDeviceType() == nanos6_host_device)
		_current_task->setAccelerator(getAcceleratorByHandle(0));
	_symbol_allocations.clear();
	_symbol_allocations.resize(sym.size());


	const int handle = _current_task->getAccelerator()->getDirectoryHandler();

	for (size_t i = 0; i < sym.size(); ++i) {
		auto allocation = getDeviceAllocation(handle, sym[i]);
		if (allocation == nullptr)
			return false;
		_symbol_allocations[i] = allocation;
		_symbol_allocations[i]->print();
	}


	for (size_t i = 0; i < sym.size(); ++i)
		processSymbol(sym[i], _symbol_allocations[i]);

	_symbol_allocations.clear();

	return true;
}

//SYMBOL PROCESSING

void DeviceDirectory::processSymbol(SymbolRepresentation &symbol, std::shared_ptr<DeviceAllocation> &deviceAllocation)
{
	symbol.setSymbolTranslation(deviceAllocation);
	processSymbolRegions(symbol.getInputRegions(), deviceAllocation, READ_ACCESS_TYPE);
	processSymbolRegions(symbol.getOutputRegions(), deviceAllocation, WRITE_ACCESS_TYPE);
	processSymbolRegions(symbol.getInputOutputRegions(), deviceAllocation, READWRITE_ACCESS_TYPE);
}

void DeviceDirectory::processSymbolRegions(const std::vector<DataAccessRegion> &dataAccessVector, std::shared_ptr<DeviceAllocation> &region, DataAccessType RW_TYPE)
{
	const int handle = _current_task->getAccelerator()->getDirectoryHandler();

	const auto in_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, *entry, region, RW_TYPE);
		processSymbolRegions_in(handle, *entry);
		return true;
	};

	const auto out_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, *entry, region, RW_TYPE);
		processSymbolRegions_out(handle, *entry);
		return true;
	};
	const auto inout_lambda = [=, &region](DirectoryEntry *entry) {
		processRegionWithOldAllocation(handle, *entry, region, RW_TYPE);
		processSymbolRegions_inout(handle, *entry);
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

std::shared_ptr<DeviceAllocation> DeviceDirectory::getNewAllocation(int handle, SymbolRepresentation &symbol)
{
	std::pair<std::shared_ptr<DeviceAllocation>, bool> deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

	if (!deviceAllocation.second) {
		_dirMap->freeAllocationsForHandle(handle, _symbol_allocations);
		deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

		if (!deviceAllocation.second) {
			std::printf("FATAL ERROR: NO MEMORY ON DEVICE AFTER TRYING TO FREE\n");
			exit(-1);
		}
	}

	_dirMap->addRange(deviceAllocation.first->getHostRegion());

	_dirMap->applyToRange(deviceAllocation.first->getHostRegion(), [=, &deviceAllocation](DirectoryEntry *entry) {
		if (entry->getDeviceAllocation(handle) == nullptr)
			entry->setDeviceAllocation(handle, deviceAllocation.first);
		return true;
	});

	return deviceAllocation.first;
}

std::shared_ptr<DeviceAllocation> DeviceDirectory::getDeviceAllocation(int handle, SymbolRepresentation &symbol)
{
	_dirMap->addRange(symbol.getHostRegion());

	auto host_region = symbol.getHostRegion();
	auto iter = _dirMap->getIterator(host_region);
	std::shared_ptr<DeviceAllocation> &deviceAllocation = iter->second->getDeviceAllocation(handle);

	if (deviceAllocation != nullptr) {
		const auto symbol_end_address = (uintptr_t)symbol.getEndAddress();
		const bool allocated = _dirMap->applyToRange(symbol.getHostRegion(), [=](DirectoryEntry *entry) {
			const auto &allocation = entry->getDeviceAllocation(handle);
			if (allocation != deviceAllocation)
				return false;
			if (allocation->getHostEnd() < symbol_end_address)
				return false;
			return true;
		});
		if (allocated)
			return deviceAllocation;
	}

	return getNewAllocation(handle, symbol);
}

//Inner Symbols

void DeviceDirectory::processSymbolRegions_out(int handle, DirectoryEntry &entry)
{
	entry.clearValid();
	entry.setModified(handle);
	entry.setValid(handle);
}

AcceleratorStream::checker DeviceDirectory::awaitToValid(int handler,  const std::pair<uintptr_t,uintptr_t> & entry)
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

AcceleratorStream::checker DeviceDirectory::setValid(int handler, const std::pair<uintptr_t,uintptr_t> &entry)
{
	return [itvMap = _dirMap, left = entry.first, right = entry.second,  handler = handler]
	{
		itvMap->applyToRange({left, right}, [=](DirectoryEntry *dirEntry) {dirEntry->setValid(handler); return true; });
		return true;
	};
}

void DeviceDirectory::processSymbolRegions_inout(int handle, DirectoryEntry &entry)
{

	AcceleratorStream* acceleratorStream = _current_task->getAcceleratorStream();
	Accelerator* accelerator = _current_task->getAccelerator();
	
	if (entry.isPending(handle))
		return acceleratorStream->addOperation(awaitToValid(handle, {entry.getLeft(), entry.getRight()}));

	if (entry.isValid(handle)) {
		entry.clearValid();
		entry.setModified(handle);
		entry.setValid(handle);
		return;
	}

	acceleratorStream->addOperation(generateCopy(entry, handle, _current_task));

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

void DeviceDirectory::processSymbolRegions_in(int handle, DirectoryEntry &entry)
{
	AcceleratorStream* acceleratorStream = _current_task->getAcceleratorStream();
	Accelerator* accelerator = _current_task->getAccelerator();

	if (entry.isValid(handle))
		return;

	if (entry.isPending(handle))
		return acceleratorStream->addOperation(awaitToValid(handle, {entry.getLeft(), entry.getRight()}));


	acceleratorStream->addOperation(generateCopy(entry, handle, _current_task));

	entry.setPending(handle);
	entry.setValid(entry.getModifiedLocation());
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
void DeviceDirectory::processRegionWithOldAllocation(int handle, DirectoryEntry &entry, std::shared_ptr<DeviceAllocation> &region, DataAccessType type)
{
	if (entry.getDeviceAllocation(handle) != region && handle != SMP_HANDLER) {
		if (entry.getModifiedLocation() == handle && type != WRITE_ACCESS_TYPE) {
			//std::cout<<"WARNING: RESIZING ENTRY TO FIT SYMBOL[!]!"<<std::endl;
			_current_task->getAcceleratorStream()->addOperation(generateCopy(entry, SMP_HANDLER, _current_task));
			entry.setModified(SMP_HANDLER);
		}
		//std::cout<<"WARNING2: INVALIDATING OLD ENTRY TO FIT SYMBOL[!]!"<<std::endl;
		entry.setInvalid(handle);
		entry.setDeviceAllocation(handle, region);
	}
}


void DeviceDirectory::taskwait(const DataAccessRegion &taskwaitRegion, std::function<void()> release)
{

	_dirMap->applyToRange(taskwaitRegion, 
	[&](DirectoryEntry *entry) 
	{
		if (!entry->isValid(SMP_HANDLER))
		{
			_taskwaitStream.addOperation(generateCopy(*entry, SMP_HANDLER, nullptr)());
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
				//entry->clearAllocations();
				entry->clearValid();
				entry->setModified(-1); 
				entry->setValid(0);
				return true;
			});

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
/*
	auto indexToString = [](auto i) -> std::string {
		if (i == DirectoryEntry::VALID)
			return "V";
		else if (i == DirectoryEntry::INVALID)
			return "I";
		else
			return "P";
	};

	for (auto &[l, entry] : _dirMap->_inner_m) {
		std::ignore = l;
		auto &[locations, allocations, modified, range, eval] = *entry;
		auto &[left, right] = range;

		printf("---------[%p,%p] \t", (void *)left, (void *)right);
		printf("%d\n", modified);
		for (size_t i = 0; i < locations.size(); ++i) {
			printf("\t[%lu] ", i);
			printf("%s\t", indexToString(locations[i]).c_str());
			if (allocations[i] != nullptr)
				printf("[%p,%p](count: %lX)\t", 
				(void *)allocations[i]->getDeviceBase(),
				(void *)allocations[i]->getDeviceEnd(),
				allocations[i].use_count());
			else
				printf("none\t");
			printf("\n");
		}
		printf("-------------------------------\n");
	}

	printf("END OF DIR\n\n");
*/
}
