/*
 This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

 Copyright (C) 2015-2017 Barcelona Supercomputing Center (BSC)
 */

#include <config.h>

#ifdef USE_FPGA
#include "hardware/device/fpga/FPGADeviceInfo.hpp"
#include "hardware/device/fpga/FPGAAccelerator.hpp"
#include <nanos6/fpga_device.h>
#include <libxtasks.h>

extern "C"
{

void nanos6_fpga_addArg(int index, unsigned char flags, unsigned long long value, void* taskHandle)
{
	xtasksAddArg(index, (xtasks_arg_flags) flags, (xtasks_arg_val) value, *((xtasks_task_handle*) taskHandle));
}

void nanos6_fpga_addArgs(int num, unsigned char flags, const unsigned long long* values, void* taskHandle)
{
	xtasksAddArgs(num, (xtasks_arg_flags) flags, (xtasks_arg_val*)values, *((xtasks_task_handle*) taskHandle));
}

nanos6_fpga_stat_t nanos6_fpga_malloc(uint64_t size, uint64_t* fpga_addr)
{
	FPGADeviceInfo* devInfo = (FPGADeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_fpga_device);
	FPGAAccelerator* accelerator = (FPGAAccelerator*)devInfo->getAccelerators()[0];
	std::pair<void*, bool> ret = accelerator->accel_allocate(size);
	*fpga_addr = (uint64_t)ret.first;
	return ret.second ? NANOS6_FPGA_SUCCESS : NANOS6_FPGA_ERROR;
}

nanos6_fpga_stat_t nanos6_fpga_free(uint64_t fpga_addr)
{
	FPGADeviceInfo* devInfo = (FPGADeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_fpga_device);
	FPGAAccelerator* accelerator = (FPGAAccelerator*)devInfo->getAccelerators()[0];
	bool ret = accelerator->accel_free((void*)fpga_addr);
	return ret ? NANOS6_FPGA_SUCCESS : NANOS6_FPGA_ERROR;
}

nanos6_fpga_stat_t nanos6_fpga_memcpy(void* usr_ptr, uint64_t fpga_addr, uint64_t size, nanos6_fpga_copy_t copy_type) {
	FPGADeviceInfo* devInfo = (FPGADeviceInfo*)HardwareInfo::getDeviceInfo(nanos6_fpga_device);
	FPGAAccelerator* accelerator = (FPGAAccelerator*)devInfo->getAccelerators()[0];
	std::function<std::function<bool(void)>()> ret;
	if (copy_type == NANOS6_FPGA_HOST_TO_DEV) {
		ret = accelerator->copy_in((void*)fpga_addr, usr_ptr, size, nullptr);
	}
	else {
		ret = accelerator->copy_out(usr_ptr, (void*)fpga_addr, size, nullptr);
	}

	std::function<bool(void)> finish_fun = ret();
	while (!finish_fun());
	return NANOS6_FPGA_SUCCESS;
}

nanos6_fpga_stat_t nanos6_fpga_memcpy_wideport_in(void* dst, const unsigned long long int addr, const unsigned int num_elems) {}
nanos6_fpga_stat_t nanos6_fpga_memcpy_wideport_out(void* dst, const unsigned long long int addr, const unsigned int num_elems) {}

}

#endif
