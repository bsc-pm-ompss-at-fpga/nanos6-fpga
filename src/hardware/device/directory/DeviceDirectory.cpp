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
	dirMap(accels.size()),
	_stopService(false), _finishedService(false)
{
	for (size_t i = 0; i < _accelerators.size(); ++i)
		_accelerators[i]->setDirectoryHandle(i);

}

AcceleratorStream::activatorReturnsChecker DeviceDirectory::generateCopy(const DirectoryEntry &entry, int dstHandle, Task *task)
{
	const int srcHandle = entry.getFirstValidLocation();
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

	// printf("Getting device allocations...\n");
	for (size_t i = 0; i < sym.size(); ++i) {
		auto allocation = getDeviceAllocation(handle, sym[i]);
		if (allocation == nullptr)
			return false;
		_symbol_allocations[i] = allocation;
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
		dirMap.addRange(dependencyRegion);
		if (RW_TYPE == READ_ACCESS_TYPE)
			dirMap.applyToRange(dependencyRegion, in_lambda);
		else if (RW_TYPE == WRITE_ACCESS_TYPE)
			dirMap.applyToRange(dependencyRegion, out_lambda);
		else
			dirMap.applyToRange(dependencyRegion, inout_lambda);
	}
}

//ALLOCATIONS

std::shared_ptr<DeviceAllocation> DeviceDirectory::getNewAllocation(int handle, SymbolRepresentation &symbol)
{
	std::shared_ptr<DeviceAllocation> deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

	if (deviceAllocation.get() == nullptr) {
		dirMap.freeAllocationsForHandle(handle, _symbol_allocations);
		deviceAllocation = _accelerators[handle]->createNewDeviceAllocation(symbol.getHostRegion());

		if (deviceAllocation.get() == nullptr) {
			std::printf("FATAL ERROR: NO MEMORY ON DEVICE AFTER TRYING TO FREE\n");
			exit(-1);
		}
	}

	dirMap.addRange(deviceAllocation->getHostRegion());

	dirMap.applyToRange(deviceAllocation->getHostRegion(), [=, &deviceAllocation](DirectoryEntry *entry) {
		if (entry->getDeviceAllocation(handle) == nullptr)
			entry->setDeviceAllocation(handle, deviceAllocation);
		return true;
	});

	return deviceAllocation;
}

std::shared_ptr<DeviceAllocation> DeviceDirectory::getDeviceAllocation(int handle, SymbolRepresentation &symbol)
{
	dirMap.addRange(symbol.getHostRegion());

	auto host_region = symbol.getHostRegion();
	auto iter = dirMap.getIterator(host_region);
	std::shared_ptr<DeviceAllocation> &deviceAllocation = iter->second->getDeviceAllocation(handle);

	if (deviceAllocation != nullptr) {
		const auto symbol_end_address = (uintptr_t)symbol.getEndAddress();
		const bool allocated = dirMap.applyToRange(symbol.getHostRegion(), [=](DirectoryEntry *entry) {
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

AcceleratorStream::checker DeviceDirectory::awaitToValid(int handler, DirectoryEntry &entry)
{
	return [left = entry.getLeft(), right = entry.getRight(), itvMap = &dirMap, handler = handler]() {
		return itvMap->applyToRange({left, right}, [=](DirectoryEntry *dirEntry) { return dirEntry->isValid(handler); });
	};
}

AcceleratorStream::checker DeviceDirectory::setValid(int handler, DirectoryEntry &entry)
{
	return [left = entry.getLeft(), right = entry.getRight(), itvMap = &dirMap, handler = handler]() 
	{
		itvMap->applyToRange(
			{left, right}, 
			[=](DirectoryEntry *dirEntry) 
			{
				dirEntry->setValid(handler);
				 return true; 
			});
		return true;
	};
}

void DeviceDirectory::processSymbolRegions_inout(int handle, DirectoryEntry &entry)
{

	AcceleratorStream* acceleratorStream = _current_task->getAcceleratorStream();
	Accelerator* accelerator = _current_task->getAccelerator();
	
	if (entry.isPending(handle))
		return acceleratorStream->addOperation(awaitToValid(handle, entry));

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

	accelerator->createEvent([this,accelerator, setToValid = setValid(handle, entry)](AcceleratorEvent *own) 
	{
		accelerator->destroyEvent(own);
		setToValid();
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
		return acceleratorStream->addOperation(awaitToValid(handle, entry));


	acceleratorStream->addOperation(generateCopy(entry, handle, _current_task));

	entry.setPending(handle);
	entry.setValid(entry.getModifiedLocation());
	entry.setModified(NO_DEVICE);

	accelerator->createEvent([this,accelerator, setToValid = setValid(handle, entry)](AcceleratorEvent *own) 
	{
		accelerator->destroyEvent(own);
		setToValid();
		return true;
	})->record(acceleratorStream);
}

//the control logic that comes afterwards will enqueue a copy operation from SMP to the device again
//since a stream is sequential, this is valid.
void DeviceDirectory::processRegionWithOldAllocation(int handle, DirectoryEntry &entry, std::shared_ptr<DeviceAllocation> &region, DataAccessType type)
{
	if (entry.getDeviceAllocation(handle) != region && handle != SMP_HANDLER) {
		if (entry.getModifiedLocation() == handle && type != WRITE_ACCESS_TYPE) {
			_current_task->getAcceleratorStream()->addOperation(generateCopy(entry, SMP_HANDLER, _current_task));
			entry.setModified(SMP_HANDLER);
		}
		entry.setInvalid(handle);
		entry.setDeviceAllocation(handle, region);
	}
}


void DeviceDirectory::taskwait(const DataAccessRegion &taskwaitRegion, std::function<void()> release)
{
		std::cout<<" @@@@@@@@setup event for taskwait"<<std::endl;


	dirMap.applyToRange(taskwaitRegion, 
	[&](DirectoryEntry *entry) 
	{
		if (!entry->isValid(SMP_HANDLER)) 
			_taskwaitStream.addOperation(generateCopy(*entry, SMP_HANDLER, nullptr)());
		return true;
	});

	AcceleratorEvent * event = new AcceleratorEvent([=](AcceleratorEvent* own) {
		/*release taskwait*/
		dirMap.applyToRange(taskwaitRegion, [](DirectoryEntry *entry) {entry->clearAllocations(); return true; });
		std::cout<<"releasing taskwait copyback"<<std::endl;
		release();
		delete own;
	});
	event->record(&_taskwaitStream);
	

}