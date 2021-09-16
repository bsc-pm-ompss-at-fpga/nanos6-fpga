/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef HOST_ACCELERATOR_HPP
#define HOST_ACCELERATOR_HPP

#include "hardware/device/Accelerator.hpp"

class HostAccelerator : public Accelerator {
public:

	HostAccelerator() :
		Accelerator(0, nanos6_host_device, 0, 0, 0)
	{}

	void submitDevice(const DeviceEnvironment&) const override {}
	bool checkDeviceSubmissionFinished(const DeviceEnvironment&) const override {return true;}
	void generateDeviceEvironment(DeviceEnvironment&, uint64_t) override {}

	std::pair<void *, bool> accel_allocate(size_t) override {return {nullptr, false};}
	void accel_free(void *) override {}

	std::function<std::function<bool(void)>()> copy_in(void*, void*, size_t, void*) const override
	{
		return []() -> std::function<bool(void)> {
			return []() -> bool {return true;};
		};
	}
	std::function<std::function<bool(void)>()> copy_out(void*, void*, size_t, void*) const override
	{
		return []() -> std::function<bool(void)> {
			return []() -> bool {return true;};
		};
	}
	std::function<std::function<bool(void)>()> copy_between(void*, int, void*, int, size_t, void*) const override
	{
		return []() -> std::function<bool(void)> {
			return []() -> bool {return true;};
		};
	}
	
	int getVendorDeviceId() const override{return 0;}
	inline void setActiveDevice() const override {}
};

#endif // HOST_ACCELERATOR_HPP
