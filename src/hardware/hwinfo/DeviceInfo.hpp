/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef DEVICE_INFO_HPP
#define DEVICE_INFO_HPP

#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"

class Accelerator;

class DeviceInfo {
protected:
	//! Number of devices of the given device type
	size_t _deviceCount;

	//! Underlying mechanism initialization status, where applicable (e.g CUDA Runtime)
	bool _deviceInitialized;

	std::vector<Accelerator *> _accelerators;


public:

	inline 	std::vector<Accelerator*>& getAccelerators()
	{
		return _accelerators;
	}

	virtual ~DeviceInfo()
	{
	}

	inline size_t getDeviceCount() const
	{
		return _deviceCount;
	}

	inline bool isDeviceInitialized() const
	{
		return _deviceInitialized;
	}

	virtual void initializeDeviceServices()
	{
	}

	virtual void shutdownDeviceServices()
	{
	}

	virtual size_t getComputePlaceCount() const = 0;

	virtual ComputePlace *getComputePlace(int handler) const = 0;

	virtual size_t getMemoryPlaceCount() const = 0;

	virtual MemoryPlace *getMemoryPlace(int handler) const = 0;

	virtual size_t getNumPhysicalPackages() const
	{
		return 0;
	}
};

#endif // DEVICE_INFO_HPP
