/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2021 Barcelona Supercomputing Center (BSC)
*/

#include <config.h>
#include <nanos6/task-instantiation.h>

#include "HardwareInfo.hpp"
#include "hwinfo/HostInfo.hpp"
#include "hardware/device/Accelerator.hpp"
#include "hardware/device/directory/DeviceDirectory.hpp"
#ifdef USE_CUDA
#include "hardware/device/cuda/CUDADeviceInfo.hpp"
#endif

#ifdef USE_OPENACC
#include "hardware/device/openacc/OpenAccDeviceInfo.hpp"
#endif

#ifdef USE_FPGA
#include "hardware/device/fpga/FPGADeviceInfo.hpp"
#endif

std::vector<DeviceInfo *> HardwareInfo::_infos;

void HardwareInfo::initialize()
{
	std::vector<Accelerator*> _accelerators;


	_infos.resize(nanos6_device_type_num);

	_infos[nanos6_host_device] = new HostInfo();
	// Prioritizing OpenACC over CUDA, as CUDA calls appear to break PGI contexts.
	// The opposite fortunately does not appear to happen.
#ifdef USE_OPENACC
	_infos[nanos6_openacc_device] = new OpenAccDeviceInfo();
#endif
#ifdef USE_CUDA
	_infos[nanos6_cuda_device] = new CUDADeviceInfo();
#endif
// Fill the rest of the devices accordingly, once implemented


	for(int i = 0; i < (int)nanos6_device_t::nanos6_device_type_num; ++i)
	{
		DeviceInfo *deviceInfo = getDeviceInfo(i);
		if(deviceInfo != nullptr && i != nanos6_device_t::nanos6_device_type_num) 
			for(Accelerator* accel: deviceInfo->getAccelerators())
			 _accelerators.push_back(accel);
	}
	DeviceDirectoryInstance::instance = new DeviceDirectory(_accelerators);
}

void HardwareInfo::initializeDeviceServices()
{
	_infos[nanos6_host_device]->initializeDeviceServices();
#ifdef USE_OPENACC
	_infos[nanos6_openacc_device]->initializeDeviceServices();
#endif
#ifdef USE_CUDA
	_infos[nanos6_cuda_device]->initializeDeviceServices();
#endif

#ifdef USE_FPGA
	_infos[nanos6_device_t::nanos6_fpga_device] = new FPGADeviceInfo();
#endif
	DeviceDirectoryInstance::instance->initializeTaskwaitService();
}

void HardwareInfo::shutdown()
{
	for (int i = 0; i < nanos6_device_type_num; ++i) {
		if (_infos[i] != nullptr) {
			delete _infos[i];
		}
	}
}

void HardwareInfo::shutdownDeviceServices()
{
	_infos[nanos6_host_device]->shutdownDeviceServices();
#ifdef USE_OPENACC
	_infos[nanos6_openacc_device]->shutdownDeviceServices();
#endif
#ifdef USE_CUDA
	_infos[nanos6_cuda_device]->shutdownDeviceServices();
#endif
	DeviceDirectoryInstance::instance->shutdownTaskwaitService();
}
