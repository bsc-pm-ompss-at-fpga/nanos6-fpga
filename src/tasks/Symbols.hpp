/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <vector>
#include <memory>

#include "DataAccessRegion.hpp"
#include <hardware/device/DeviceAllocation.hpp>
#include <api/nanos6/task-instantiation.h>
#include <dependencies/DataAccessType.hpp>

struct DistributedSymbol
{
	void* startAddress;
	size_t size;
	bool isBroadcast;
	bool isScatter;
	bool isGather;
	bool isReceive;
	int receiveDevice;
};

//A symbol representation may have multiple data accesses, which can be adjacent or not
struct SymbolRepresentation
{
	//host_region is a region containing all accesses to this symbol. This is necessary to do
	//correct allocations even if the data accesses are not adjacent. All reads/writes
	//to this symbol use the same pointer in the user code.
	DataAccessRegion host_region;

	std::shared_ptr<DeviceAllocation>   allocation;

	std::vector<DataAccessRegion> input_regions;
	std::vector<DataAccessRegion> output_regions;
	std::vector<DataAccessRegion> inout_regions;


	SymbolRepresentation():
		host_region(),
		allocation(nullptr)
		{

		}

	void addDataAccess(DataAccessRegion region, DataAccessType type)
	{
		void* minimum = (void*) std::min((uintptr_t) host_region.getStartAddress(),(uintptr_t) region.getStartAddress());
		if(minimum == nullptr) minimum = region.getStartAddress();
		void* maximum = (void*) std::max((uintptr_t) host_region.getEndAddress(),  (uintptr_t) region.getEndAddress());
		host_region = DataAccessRegion(minimum,maximum);

		switch (type)
		{
			case READ_ACCESS_TYPE:      input_regions.push_back(region); break;
			case READWRITE_ACCESS_TYPE: inout_regions.push_back(region); break;
			default:                    output_regions.push_back(region); break;
		}

	}


	inline DataAccessRegion getHostRegion() const {return host_region;}
	inline const std::vector<DataAccessRegion>& getInputRegions() const {return input_regions;}
	inline const std::vector<DataAccessRegion>& getOutputRegions() const {return output_regions;}
	inline const std::vector<DataAccessRegion>& getInputOutputRegions() const {return inout_regions;}
	inline uintptr_t getStartAddress() const { return (uintptr_t) host_region.getStartAddress();}
	inline uintptr_t getEndAddress() const {return (uintptr_t) host_region.getEndAddress();}
	inline uintptr_t getSize() const { return getEndAddress() - getStartAddress();}
	inline std::pair<uintptr_t, uintptr_t> getBounds() const {return {getStartAddress(), getEndAddress()};};
	inline void setSymbolTranslation(std::shared_ptr<DeviceAllocation>& alloc){
		allocation = alloc;
		//std::cout<<"SET SYMBOL TRANSLATION: HOST["<<std::hex<<allocation->getHostBase()<<"]  DEVICE["<<allocation->getDeviceBase()<<"]"<<std::endl;
	}
};

#endif //SYMBOLS_HPP
