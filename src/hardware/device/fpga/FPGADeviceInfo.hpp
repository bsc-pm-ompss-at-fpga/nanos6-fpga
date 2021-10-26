/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_DEVICE_INFO_HPP
#define FPGA_DEVICE_INFO_HPP

#include "hardware/hwinfo/DeviceInfo.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"

#include "FPGAAccelerator.hpp"
#include <libxtasks.h>

class FPGADeviceInfo : public DeviceInfo {

public:
	FPGADeviceInfo()
	{
		_deviceInitialized = false;
		_deviceCount = 0;
		if (xtasksInit() != XTASKS_SUCCESS)
			return;

        FatalErrorHandler::failIf(
            xtasksGetNumDevices((int*)&_deviceCount) != XTASKS_SUCCESS,
            "Xtasks: Can't get number of devices"
        );
        _accelerators.resize(_deviceCount);
        for (size_t i = 0; i < _deviceCount; ++i) {
            _accelerators[i] = new FPGAAccelerator(i);
        }
        _deviceInitialized = true;
	}

	~FPGADeviceInfo()
	{
		if (!_deviceInitialized)
			return;
		xtasksFini();
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			delete (FPGAAccelerator *)accelerator;
		}
	}

	inline void initializeDeviceServices() override
	{
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			((FPGAAccelerator *)accelerator)->initializeService();
		}
	}

	inline void shutdownDeviceServices() override
	{
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			((FPGAAccelerator *)accelerator)->shutdownService();
		}
	}

	inline size_t getComputePlaceCount() const override
	{
		return _deviceCount;
	}

	inline ComputePlace *getComputePlace(int handler) const override
	{
		return _accelerators[handler]->getComputePlace();
	}

	inline size_t getMemoryPlaceCount() const override
	{
		return _deviceCount;
	}

	inline MemoryPlace *getMemoryPlace(int handler) const override
	{
		return _accelerators[handler]->getMemoryPlace();
	}
};

#endif // FPGA_DEVICE_INFO_HPP
