/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef BROADCASTER_DEVICE_INFO_HPP
#define BROADCASTER_DEVICE_INFO_HPP

#include "hardware/hwinfo/DeviceInfo.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"

#include "BroadcasterAccelerator.hpp"
#include <libxtasks.h>

class BroadcasterDeviceInfo : public DeviceInfo {

public:
    BroadcasterDeviceInfo(std::vector<Accelerator*>& cluster)
	{
		if (cluster.size() == 0)
		{
			_deviceCount = 0;
			_deviceInitialized = false;
			return;
		}
		_deviceCount = 1;
        _accelerators.resize(1);
        _accelerators[0] = new BroadcasterAccelerator(cluster);
		_deviceInitialized = true;
	}

    ~BroadcasterDeviceInfo()
	{
		if (_deviceInitialized)
			delete _accelerators[0];
	}

	inline void initializeDeviceServices() override
	{
		if (_deviceInitialized)
			_accelerators[0]->initializeService();
	}

	inline void shutdownDeviceServices() override
	{
		if (_deviceInitialized)
			_accelerators[0]->shutdownService();
	}

	inline size_t getComputePlaceCount() const override
	{
		return _deviceCount;
	}

    inline ComputePlace *getComputePlace([[maybe_unused]] int handler) const override
	{
        return _accelerators[0]->getComputePlace();
	}

	inline size_t getMemoryPlaceCount() const override
	{
		return _deviceCount;
	}

    inline MemoryPlace *getMemoryPlace([[maybe_unused]] int handler) const override
	{
        return _accelerators[0]->getMemoryPlace();
    }
};

#endif // BROADCASTER_DEVICE_INFO_HPP
