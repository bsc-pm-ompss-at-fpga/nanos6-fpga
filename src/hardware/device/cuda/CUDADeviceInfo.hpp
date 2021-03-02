/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef CUDA_DEVICE_INFO_HPP
#define CUDA_DEVICE_INFO_HPP

#include "CUDAAccelerator.hpp"
#include "CUDAFunctions.hpp"

#include "hardware/hwinfo/DeviceInfo.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"

// This class provides the interface to be used by the runtime's Hardware Info;
// Not to be confused with the device properties (see CUDAFunctions class)

class CUDADeviceInfo : public DeviceInfo {

public:
	CUDADeviceInfo()
	{
		_deviceCount = 0;
		if (!CUDAFunctions::initialize())
			return;

		_deviceCount = CUDAFunctions::getDeviceCount();
		_accelerators.reserve(_deviceCount);

		if (_deviceCount > 0) {
			// Create an Accelerator instance for each physical device
			for (size_t i = 0; i < _deviceCount; ++i) {
				CUDAAccelerator *accelerator = new CUDAAccelerator(i);
				assert(accelerator != nullptr);
				_accelerators.push_back((Accelerator*)accelerator);
			}

			_deviceInitialized = true;
		}
	}

	~CUDADeviceInfo()
	{
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			delete (CUDAAccelerator *)accelerator;
		}
	}

	inline void initializeDeviceServices() override
	{
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			((CUDAAccelerator *)accelerator)->initializeService();
		}
	}

	inline void shutdownDeviceServices() override
	{
		for (Accelerator *accelerator : _accelerators) {
			assert(accelerator != nullptr);
			((CUDAAccelerator *)accelerator)->shutdownService();
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

#endif // CUDA_DEVICE_INFO_HPP
