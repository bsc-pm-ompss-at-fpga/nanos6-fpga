/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "FPGAAccelerator.hpp"
#include "../directory/DeviceDirectory.hpp"
#include "hardware/places/ComputePlace.hpp"
#include "hardware/places/MemoryPlace.hpp"
#include "libxtasks.h"
#include "scheduling/Scheduler.hpp"
#include "system/BlockingAPI.hpp"
#include "InstrumentFPGAEvents.hpp"

#include <DataAccessRegistration.hpp>
#include <DataAccessRegistrationImplementation.hpp>
#include <memory>
#include "lowlevel/FatalErrorHandler.hpp"


std::unordered_map<const nanos6_task_implementation_info_t*, uint64_t> FPGAAccelerator::_device_subtype_map;

FPGAAccelerator::FPGAAccelerator(int fpgaDeviceIndex) :
	Accelerator(fpgaDeviceIndex,
		nanos6_fpga_device,
		ConfigVariable<uint32_t>("devices.fpga.streams"),
		ConfigVariable<size_t>("devices.fpga.polling.period_us"),
		ConfigVariable<bool>("devices.fpga.polling.pinned")),
		_allocator(fpgaDeviceIndex),
		_reverseOffload(_allocator, _pollingPeriodUs, _isPinnedPolling, (ConfigVariable<std::string>("version.instrument").getValue() == "ovni"))
{
	std::string memSyncString = ConfigVariable<std::string>("devices.fpga.mem_sync_type");
	if (memSyncString == "async") {
		_mem_sync_type = REAL_ASYNC;
	}
	else if (memSyncString == "forced async") {
		_mem_sync_type = FORCED_ASYNC;
	}
	else if (memSyncString == "sync") {
		_mem_sync_type = SYNC;
	}
	else {
		FatalErrorHandler::fail("Config value", memSyncString, " is not valid for devices.fpga.mem_sync_type");
	}

	size_t handlesCount=0;
	accCount = 0;

	FatalErrorHandler::failIf(
        xtasksGetNumAccs(fpgaDeviceIndex, &accCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get number of accelerators"
	);

	std::vector<xtasks_acc_info> info(accCount);
	std::vector<xtasks_acc_handle> handles(accCount);

	FatalErrorHandler::failIf(
        xtasksGetAccs(fpgaDeviceIndex, accCount, &handles[0], &handlesCount) != XTASKS_SUCCESS,
		"Xtasks: Can't get the accelerators"
	);

	for(size_t i=0; i<accCount;++i)
	{
		xtasksGetAccInfo(handles[i], &info[i]);
		_inner_accelerators[info[i].type]._accelHandle.push_back(handles[i]);
	}
	if (ConfigVariable<std::string>("version.instrument").getValue() == "ovni") {
		xtasks_ins_timestamp startTimeFpga;
		xtasksGetAccCurrentTime(handles[0], &startTimeFpga);
		uint64_t startTimeCpu = Instrument::getCPUTimeForFPGA();

		acceleratorInstrumentationServices = new FPGAAcceleratorInstrumentation[accCount];
		for(size_t i=0;i<accCount;++i) {
			FPGAAcceleratorInstrumentation::HandleWithInfo handleWithInfo = {handles[i], info[i], startTimeFpga, startTimeCpu};
			acceleratorInstrumentationServices[i].setHandle(handleWithInfo);
		}
	} else {
		acceleratorInstrumentationServices = nullptr;
	}
}

FPGAAccelerator::~FPGAAccelerator() {
	if (acceleratorInstrumentationServices != nullptr)
		delete acceleratorInstrumentationServices;
}

inline void FPGAAccelerator::generateDeviceEvironment(DeviceEnvironment& env, const nanos6_task_implementation_info_t* task_implementation) {
	uint64_t deviceSubtype = _device_subtype_map[task_implementation];
#ifndef NDEBUG
	FatalErrorHandler::failIf(
		_inner_accelerators.find(deviceSubtype) == _inner_accelerators.end(),
		"Device subtype ", deviceSubtype, " not found"
	);
#endif
	xtasks_acc_handle accelerator = _inner_accelerators[deviceSubtype].getHandle();
	xtasks_task_id parent = 0;
	xtasksCreateTask((xtasks_task_id) &env.fpga, accelerator, parent, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &env.fpga.taskHandle);
	env.fpga.taskFinished = false;
}

std::pair<void *, bool> FPGAAccelerator::accel_allocate(size_t size)
{
	return _allocator.allocate(size);
}

bool FPGAAccelerator::accel_free(void* ptr)
{
	return _allocator.free(ptr);
}

void FPGAAccelerator::postRunTask(Task *)
{
}

void FPGAAccelerator::submitDevice(const DeviceEnvironment &deviceEnvironment) const {
	FatalErrorHandler::failIf(
		xtasksSubmitTask(deviceEnvironment.fpga.taskHandle) != XTASKS_SUCCESS,
		"Xtasks: Submit Task failed"
	);
}

inline std::function<bool()> FPGAAccelerator::getDeviceSubmissionFinished(const DeviceEnvironment& deviceEnvironment) const {
	return [&] () -> bool {
		xtasks_task_handle hand;
		xtasks_task_id tid;
		xtasks_stat stat;
		while((stat = xtasksTryGetFinishedTask(&hand, &tid)) == XTASKS_SUCCESS)
		{
			xtasksDeleteTask(&hand);
			nanos6_fpga_device_environment_t* env = (nanos6_fpga_device_environment_t*) tid;
			env->taskFinished = true;
		}
		assert(stat == XTASKS_PENDING);
		return deviceEnvironment.fpga.taskFinished;
	};
}

void FPGAAccelerator::callBody(Task *task)
{
    if(DeviceDirectoryInstance::useDirectory) {
		task->getAcceleratorStream()->addOperation(
			[this, task, env = &task->getDeviceEnvironment(), handler = getDeviceHandler()]() -> std::function<bool(void)>
			{
				void *args = task->getArgsBlock();
				nanos6_task_info_t *taskInfo = task->getTaskInfo();
				const std::vector<SymbolRepresentation>& symbolInfo = task->getSymbolInfo();
				int numArgs = taskInfo->num_args;
				int numSymbols = taskInfo->num_symbols;
				const xtasks_task_handle handle = task->getDeviceEnvironment().fpga.taskHandle;
				xtasks_arg_val fpga_args[16]; //Current max supported number of arguments
				assert (numArgs <= 16);

				memset(fpga_args, 0, sizeof(fpga_args));
				for (int i = 0; i < numArgs; ++i) {
					assert (taskInfo->sizeof_table[i] <= (int)sizeof(xtasks_arg_val));
					char* p = (char*)args + taskInfo->offset_table[i];
					memcpy(fpga_args + i, p, taskInfo->sizeof_table[i]);
				}

				for (int i = 0; i < numSymbols; ++i) {
					int arg = taskInfo->arg_idx_table[i];
					uint64_t host_addr = symbolInfo[i].allocation->getHostBase();
					uint64_t fpga_addr = symbolInfo[i].allocation->getDeviceBase();
					fpga_args[arg] = fpga_args[arg] - host_addr + fpga_addr;
				}

				xtasksAddArgs(numArgs, 0xFF, fpga_args, handle);

				FatalErrorHandler::failIf(
					xtasksSubmitTask(handle) != XTASKS_SUCCESS,
					"Xtasks: Submit Task failed"
				);

				return getDeviceSubmissionFinished(*env);
			}
		);
    }
    else FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");
}

void FPGAAccelerator::preRunTask(Task *task)
{
	if (DeviceDirectoryInstance::useDirectory)
		DeviceDirectoryInstance::instance->register_regions(task);
	else
		FatalErrorHandler::fail("Can't use FPGA Tasks without the directory");
}

std::function<std::function<bool(void)>()> FPGAAccelerator::copy_in(void *dst, void *src, size_t size, [[maybe_unused]]  void *task) const
{
	if(_mem_sync_type == REAL_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_HOST_TO_ACC);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
	}
	else if (_mem_sync_type == FORCED_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
		{

			bool* copy_finished_flag = new bool;
			*copy_finished_flag=false;
			auto do_copy = [](void* t)
			{
				std::function<void()>* fn = (std::function<void()>*) t;
				(*fn)();
				delete fn;
			};

			auto copy  = new std::function<void()>([=](){_allocator.memcpy(dst,src,size,XTASKS_HOST_TO_ACC);});

			auto finish_copy = [](void* flag){ bool* ff = (bool*) flag; *ff=true;};

			SpawnFunction::spawnFunction(do_copy, (void*) copy, finish_copy, (void*) copy_finished_flag,"CopyInFPGA", false);
			return [=]() -> bool
			{
				if(*copy_finished_flag)
				{
					delete copy_finished_flag;
					return true;
				}
				return false;
				//IF FINISHED FLAG -> CONTINUE
			};
		};
	}
	else
	{
		return [&, dst, src, size]() -> std::function<bool(void)> {
			return [&, dst, src, size]() -> bool {
				_allocator.memcpy(dst, src, size, XTASKS_HOST_TO_ACC);
				return true;
			};
		};
	}
}

//this functions performs a copy from the accelerator address space to host memory
std::function<std::function<bool(void)>()> FPGAAccelerator::copy_out(void *dst, void *src, size_t size, [[maybe_unused]] void *task) const
{
	if(_mem_sync_type == REAL_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
			{
				auto checkFinalization= _allocator.memcpyAsync(dst,src,size,XTASKS_ACC_TO_HOST);
				return [this, checkFinalization]() -> bool
				{
					return _allocator.testAsyncDestroyOnSuccess(checkFinalization);
				};
			};
	}
	else if (_mem_sync_type == FORCED_ASYNC)
	{
		return [=]() -> std::function<bool(void)>
		{

			bool* copy_finished_flag = new bool;
			*copy_finished_flag=false;
			auto do_copy = [](void* t)
			{
				std::function<void()>* fn = (std::function<void()>*) t;
				(*fn)();
				delete fn;
			};

			auto copy  = new std::function<void()>([=](){_allocator.memcpy(dst,src,size,XTASKS_ACC_TO_HOST);});

			auto finish_copy = [](void* flag){ bool* ff = (bool*) flag; *ff=true;};

			SpawnFunction::spawnFunction(do_copy, (void*) copy, finish_copy, (void*) copy_finished_flag,"CopyInFPGA", false);
			return [=]() -> bool
			{
				if(*copy_finished_flag)
				{
					delete copy_finished_flag;
					return true;
				}
				return false;
				//IF FINISHED FLAG -> CONTINUE
			};
		};
	}
	else
	{
		return [&, dst, src, size]() -> std::function<bool(void)> {
			return [&, dst, src, size]() -> bool {
				_allocator.memcpy(dst, src, size, XTASKS_ACC_TO_HOST);
				return true;
			};
		};
	}
}

//this functions performs a copy from two accelerators that can share their data without the host intervention
std::function<std::function<bool(void)>()> FPGAAccelerator::copy_between(
	void *dst,
	int dstDevice,
	void *src,
	int srcDevice,
	size_t size,
	[[maybe_unused]] void *task) const
{
	xtasks_acc_handle sendHandle = (xtasks_acc_handle)(((uintptr_t)_inner_accelerators.find(4294967299)->second.getHandle(0) & 0xFFFFFFFF00000000l) | srcDevice);
	xtasks_acc_handle recvHandle = (xtasks_acc_handle)(((uintptr_t)_inner_accelerators.find(4294967300)->second.getHandle(0) & 0xFFFFFFFF00000000l) | dstDevice);
	return [=]() -> std::function<bool(void)>
	{
		nanos6_fpga_device_environment_t* env = new nanos6_fpga_device_environment_t[2];
		env[0].taskFinished = false;
		env[1].taskFinished = false;

		xtasksCreateTask((xtasks_task_id)&env[0], sendHandle, 0, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &env[0].taskHandle);
		xtasksCreateTask((xtasks_task_id)&env[1], recvHandle, 0, XTASKS_COMPUTE_ENABLE, (xtasks_task_handle*) &env[1].taskHandle);

		uint64_t argsSend[2];
		uint64_t argsRecv[2];

		argsSend[0] = ((dstDevice+1) << 16) | ((uint64_t)src << 32);
		argsSend[1] = size;

		argsRecv[0] = ((srcDevice+1) << 16) | ((uint64_t)dst << 32);
		argsRecv[1] = size;

		xtasksAddArgs(2, 0xFF, argsSend, env[0].taskHandle);
		xtasksAddArgs(2, 0xFF, argsRecv, env[1].taskHandle);

		FatalErrorHandler::failIf(
			xtasksSubmitTask(env[0].taskHandle) != XTASKS_SUCCESS,
			"Xtasks: Submit Task failed"
		);
		FatalErrorHandler::failIf(
			xtasksSubmitTask(env[1].taskHandle) != XTASKS_SUCCESS,
			"Xtasks: Submit Task failed"
		);

		return [env]() -> bool {
			xtasks_task_handle hand;
			xtasks_task_id tid;
			xtasks_stat stat;
			while((stat = xtasksTryGetFinishedTask(&hand, &tid)) == XTASKS_SUCCESS)
			{
				xtasksDeleteTask(&hand);
				nanos6_fpga_device_environment_t* finishedEnv = (nanos6_fpga_device_environment_t*) tid;
				finishedEnv->taskFinished = true;
			}
			assert(stat == XTASKS_PENDING);
			bool finished = env[0].taskFinished && env[1].taskFinished;
			if (finished) {
				delete[] env;
			}
			return finished;
		};
	};
}
