#ifndef FPGAREVERSEOFFLOAD_HPP
#define FPGAREVERSEOFFLOAD_HPP

#include <unordered_map>
#include <nanos6/task-instantiation.h>
#include <atomic>
#include "memory/allocator/devices/FPGAPinnedAllocator.hpp"

class FPGAReverseOffload
{
	std::atomic<bool> _stopService;
	std::atomic<bool> _finishedService;
	uint64_t _pollingPeriodUs;
	FPGAPinnedAllocator& _allocator;
	bool _isPinnedPolling;
	bool _isInstrumented;

public:

	static std::unordered_map<uint64_t, const nanos6_task_info_t*> _reverseMap;

	FPGAReverseOffload(FPGAPinnedAllocator& allocator, uint64_t pollingPeriodUs, bool isPinnedPolling, bool isInstrumented) :
		_stopService(false), _finishedService(false), _pollingPeriodUs(pollingPeriodUs), _allocator(allocator), _isPinnedPolling(isPinnedPolling), _isInstrumented(isInstrumented)
	{}

	void initializeService();
	void shutdownService();
	void serviceLoop();

	static void serviceFunction(void *data);
	static void serviceCompleted(void *data);
};

#endif // FPGAREVERSEOFFLOAD_HPP
