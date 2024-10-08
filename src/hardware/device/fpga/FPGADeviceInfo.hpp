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
#include "instrument/api/InstrumentFPGAEvents.hpp"
#include <libxtasks.h>

class FPGADeviceInfo : public DeviceInfo {
public:
	FPGADeviceInfo()
	{
		_deviceInitialized = false;
		_deviceCount = 0;
		// There are no FPGA tasks in the application
		if (FPGAAccelerator::_device_subtype_map.size() == 0)
			return;

		FatalErrorHandler::failIf(
			xtasksInit() != XTASKS_SUCCESS,
			"Xtasks: Can't init"
		);
		xtasks_stat stat = xtasksInitHWIns(128);
		FatalErrorHandler::failIf(
			stat != XTASKS_SUCCESS && stat != XTASKS_ENOAV,
			"Xtasks: Can't init HW instrumentation"
		);
		FatalErrorHandler::failIf(
			ConfigVariable<std::string>("version.instrument").getValue() == "ovni" &&
			stat == XTASKS_ENOAV,
			"Xtasks: Running with ovni but bitstream does not have instrumentation support"
		);
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
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			delete (FPGAAccelerator *)accelerator;
		}
		xtasksFiniHWIns();
		xtasksFini();
	}

	inline void initializeDeviceServices() override
	{
		if (ConfigVariable<bool>("devices.fpga.enable_services")) {
			for (Accelerator *accelerator : _accelerators) {
				assert(accelerator != nullptr);
				((FPGAAccelerator *)accelerator)->initializeService();
			}
		}
	}

	inline void shutdownDeviceServices() override
	{
		if (ConfigVariable<bool>("devices.fpga.enable_services")) {
			for (Accelerator *accelerator : _accelerators) {
				assert(accelerator != nullptr);
				((FPGAAccelerator *)accelerator)->shutdownService();
			}
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
