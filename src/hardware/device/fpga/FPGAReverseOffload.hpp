#ifndef FPGAREVERSEOFFLOAD_HPP
#define FPGAREVERSEOFFLOAD_HPP

#include <unordered_map>
#include <nanos6/task-instantiation.h>
#include <atomic>
#include "memory/allocator/devices/FPGAPinnedAllocator.hpp"

class FPGAReverseOffload
{

	static std::unordered_map<uint64_t, nanos6_task_info_t*> _reverseMap;
	std::atomic<bool> _stopService;
	std::atomic<bool> _finishedService;
	uint64_t _pollingPeriodUs;
	FPGAPinnedAllocator& _allocator;

public:

	FPGAReverseOffload(FPGAPinnedAllocator& allocator, uint64_t pollingPeriodUs) :
		_stopService(false), _finishedService(false), _pollingPeriodUs(pollingPeriodUs), _allocator(allocator)
	{}

	static inline void addMap(nanos6_task_info_t* task_info) {
		_reverseMap[task_info->implementations[0].device_subtype_id] = task_info;
	}

	void initializeService();
	void shutdownService();
	void serviceLoop();

	static void serviceFunction(void *data);
	static void serviceCompleted(void *data);
};

#endif // FPGAREVERSEOFFLOAD_HPP
