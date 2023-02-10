/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/
#ifndef DEVICE_ALLOCATION_HPP
#define DEVICE_ALLOCATION_HPP

#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>

#include <dependencies/linear-regions/DataAccessRegion.hpp>

struct DeviceAllocation
{
	std::function<void()> free_deviceAllocation;
	DataAccessRegion _hostRegion, _deviceRegion;

	~DeviceAllocation(){
		if(_deviceRegion.getStartAddress() != nullptr)
		{
			free_deviceAllocation();
		}
	}

	DeviceAllocation(
		const DataAccessRegion& host,	 const DataAccessRegion& device,  std::function<void()> freeFun) :
			free_deviceAllocation(std::move(freeFun)), _hostRegion(host),  _deviceRegion(device)
	{
	}

	DataAccessRegion getHostRegion() {return _hostRegion;}
	DataAccessRegion getDeviceRegion() {return _deviceRegion;}

	uintptr_t getHostBase() const {return (uintptr_t) _hostRegion.getStartAddress(); }
	uintptr_t getHostEnd() const {return (uintptr_t) _hostRegion.getEndAddress();}

	uintptr_t getDeviceBase() const { return (uintptr_t) _deviceRegion.getStartAddress();}
	uintptr_t getDeviceEnd() const {return (uintptr_t) _deviceRegion.getEndAddress(); }

	uintptr_t getTranslation(uintptr_t to_translate) const{
		return (uintptr_t) _deviceRegion.getStartAddress() + to_translate - (uintptr_t) _hostRegion.getStartAddress();
	}
	uintptr_t getTranslation(const DataAccessRegion& dar) const{
		return (uintptr_t) _deviceRegion.getStartAddress() + (uintptr_t) dar.getStartAddress() - (uintptr_t) _hostRegion.getStartAddress();
	}

	void print()
	{
	}
};

#endif // DEVICE_ALLOCATION_HPP
