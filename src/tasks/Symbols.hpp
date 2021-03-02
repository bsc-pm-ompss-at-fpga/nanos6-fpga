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
struct SymbolRepresentation
{
	DataAccessRegion host_region;

	nanos6_address_translation_entry_t* translationEntry;
	std::shared_ptr<DeviceAllocation>   allocation;


	std::vector<DataAccessRegion> input_regions;
	std::vector<DataAccessRegion> output_regions;
	std::vector<DataAccessRegion> inout_regions;


	SymbolRepresentation(nanos6_address_translation_entry_t* tEntry): 
		host_region(),
		translationEntry(tEntry),
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


	inline DataAccessRegion getHostRegion(){return host_region;}
	inline const std::vector<DataAccessRegion>& getInputRegions(){return input_regions;}
	inline const std::vector<DataAccessRegion>& getOutputRegions(){return output_regions;}
	inline const std::vector<DataAccessRegion>& getInputOutputRegions(){return inout_regions;}
	inline uintptr_t getStartAddress() { return (uintptr_t) host_region.getStartAddress();}
	inline uintptr_t getEndAddress(){return (uintptr_t) host_region.getEndAddress();}
	inline uintptr_t getSize(){ return getEndAddress() - getStartAddress();}
	inline std::pair<uintptr_t, uintptr_t> getBounds(){return {getStartAddress(), getEndAddress()};};
	inline void setSymbolTranslation(std::shared_ptr<DeviceAllocation>& alloc){

		allocation = alloc;
		//std::cout<<"SET SYMBOL TRANSLATION: HOST["<<std::hex<<allocation->getHostBase()<<"]  DEVICE["<<allocation->getDeviceBase()<<"]"<<std::endl;
		*translationEntry = {allocation->getHostBase(), allocation->getDeviceBase()};
	}
};

#endif //SYMBOLS_HPP