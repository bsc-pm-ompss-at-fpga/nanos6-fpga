/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_EVENT_HPP
#define ACCELERATOR_EVENT_HPP

#include "AcceleratorStream.hpp"
#include <chrono>
#include <functional>
#include <cassert>
// This event implementations is a generic implementation of CUDA events,
// It allows any device to generate events into streams, that are useful to
// check for task finalization or execution times
class AcceleratorEvent {
private:
	bool _completed;
	std::function<void(AcceleratorEvent *)> _onCompletion;
	std::chrono::steady_clock::time_point _creation_time, _fini_time;


	//checks for event-finalization. In case of vendor-implementation, checks with the vendor driver first.
	//In the base implementation or when the event is marked as finished by the vendor, we call the
	//callback specified at event-creation-time.
	bool query()
	{
		if (!vendorEventQuery())
			return false;
		_onCompletion(this);
		return true;
	}

public:
	AcceleratorEvent(std::function<void((AcceleratorEvent *))> &completion) :
		_completed(false), _onCompletion(std::move(completion))
	{
	}


	virtual ~AcceleratorEvent()
	{
	}


	//This virtual method is meant to be overriden by a device-specific implementation
	//If the vendor-driver for that device supports events. If not, returns true to
	//continue with the behaviour of the base-event implementation
	virtual bool vendorEventQuery()
	{
		return true;
	}

	//This virtual method is meant to be overriden by a device-specific implementation
	//If the vendor-driver for that device supports events.
	virtual void vendorEventRecord()
	{
	}


	virtual float getMillisBetweenEvents(AcceleratorEvent &end)
	{
		assert(_completed != false);
		assert(end._completed != false);
		return std::chrono::duration_cast<std::chrono::milliseconds>(end._fini_time - _fini_time).count();
	}


	virtual float getMillisBetweenEvents(AcceleratorEvent *end)
	{
		return getMillisBetweenEvents(*end);
	}


	virtual float eventCreationUntilExecution_ms()
	{
		assert(_completed != false);
		return std::chrono::duration_cast<std::chrono::milliseconds>(_fini_time - _creation_time).count();
	}

	bool hasFinished()
	{
		return _completed;
	}


	void record(AcceleratorStream *stream)
	{
		_completed = false;
		_creation_time = std::chrono::steady_clock::now();

		//this will enqueue into the stream the following functionality:
		//When the stream executes the activation part of this function,
		//the current time will be stored into _creation_time.
		//this marks the time when the event was created, not the time when executed.
		//if we have two events, we can infer the time between two points of executions of a stream.
		stream->addOperation(
			[&] {
				//Some devices like CUDA, have a native events interface, we invoke this if necessary.
				vendorEventRecord();
				//This is done to check for the finalization of the event. This is may be redundant, since
				//when we are on a stream, and the event executes, we only record time and notify that we arrived
				//here. However, a custom vendor event implementation could do something behind the courtains that
				//may delay the "completion" of the event. In CUDA, the ensuring of the stream order is handled by the cuda driver,
				//so the best we can do is to advance as much as we can the stream virtualization execution, meaning that probably,
				//when a cuda event arrives to this point, the real execution has not finished. We query the vendor-check for finalization.
				//If the vendor-check is not necessary, the first time we call query will just return true.

				return [&]() {
					if (query()) {
						_completed = true;
						_fini_time = std::chrono::steady_clock::now();
						return true;
					}
					return false;
				};
			});
	}


	//Record weak means that the execution won't be halted until the event has finished.
	//This is useful for intermediate-events for devices like CUDA, where we can ensure that
	//when a later event has been executed, all previous have been completed too.
	//This optimization means that we can enqueue more operations into the native streams
	//without the penalty of waiting for an operation to complete
	void recordWeak(AcceleratorStream *stream)
	{
		_completed = false;
		_creation_time = std::chrono::steady_clock::now();

		stream->addOperation(
			[&] {
				vendorEventRecord();

				return [&]() {
					_completed = true;
					_fini_time = std::chrono::steady_clock::now();
					return true;
				};
			});
	}
};

#endif // ACCELERATOR_EVENT_HPP