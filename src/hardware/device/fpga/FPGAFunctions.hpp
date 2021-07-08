/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef FPGA_FUNCTIONS_HPP
#define FPGA_FUNCTIONS_HPP

#include <libxtasks.h>
#include "support/config/ConfigVariable.hpp"
#include "FPGAMemoryAllocator.hpp"

// A helper class, providing static helper functions, specific to the device,
// to be used by DeviceInfo and other relevant classes as utilities.
class FPGAFunctions {
public:

	

	static FPGAMemoryAllocator& getAllocator()
	{
		static 	FPGAMemoryAllocator _fpgaAllocator;
		return _fpgaAllocator;
	}
	static bool initialize()
	{
		static bool only_init_one_time = xtasksInit() == XTASKS_SUCCESS;
		return only_init_one_time;
	}


	static size_t getDeviceCount()
	{
		return 1;
	}


	static size_t getPageSize()
	{
		static ConfigVariable<size_t> pageSize("devices.fpga.page_size");
		return pageSize;
	}


    static std::pair<xtasks_mem_handle, size_t> getMemHandleAndOffset(void* phys)
	{
		return FPGAFunctions::getAllocator().getMemHandleAndOffset(phys);
	}

	static void *malloc(size_t size)
	{
		return FPGAFunctions::getAllocator().malloc(size);
	}

	static void free(void *ptr)
	{
		return FPGAFunctions::getAllocator().free(ptr);
	}



	static void memcpy(void *dst, void *src, size_t count, xtasks_mem_handle handle, xtasks_memcpy_kind kind)
	{
		xtasks_stat ok;
		if(kind == XTASKS_HOST_TO_ACC) 
			ok = xtasksMemcpy(handle , (size_t) dst, count, src, kind);
		else
			ok = xtasksMemcpy(handle , (size_t) src, count, dst, kind);

		if(ok != XTASKS_SUCCESS) abort();
	}

	static xtasks_memcpy_handle* memcpyAsync(void *dst,  void *src, size_t count, xtasks_mem_handle handle,  xtasks_memcpy_kind kind)
	{
		xtasks_memcpy_handle *cpyHandle = new xtasks_memcpy_handle;
		xtasks_stat ok;
		if(kind == XTASKS_HOST_TO_ACC) 
			ok = xtasksMemcpyAsync(handle , (size_t) dst, count, src, kind, cpyHandle);
		else
			ok = xtasksMemcpyAsync(handle , (size_t) src, count, dst, kind, cpyHandle);

		if(ok != XTASKS_SUCCESS) abort();

		return cpyHandle;
	}

	static bool testAsyncDestroyOnSuccess(xtasks_memcpy_handle* handle)
	{
		if(xtasksTestCopy(handle) == XTASKS_SUCCESS) 
		{
			delete handle;
			return true;
		}
		return false;
	}
	

};

#endif // FPGA_FUNCTIONS_HPP
