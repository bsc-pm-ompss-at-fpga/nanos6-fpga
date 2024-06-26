/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_STREAM_HPP
#define ACCELERATOR_STREAM_HPP

#include <functional>
#include <queue>

//This class tries to emulate the behaviour of CUDA streams, this implementation makes it easier to make the acceleratos
//share the same interface to talk with the runtime and control its execution workflow.
//The stream it's its own mini-runtime

//The functionality of the stream is as follows:
//A streamable lambda consists in two parts:
//Activation function --> This function will invoke the asynchronous behaviour we want to track.
//Async finalization check --> This function will check whether or not the asynchronous behaviour has finished.
//In order for this to work, the activation function *must* return a lambda to check its finalization.

//Free functions (Functions that don't have activation, but must be run once the stream arrives to it's queue position)
//can be enqueued with addOperation, a dummy activation function will be performed. And the behaviour of the checker
//will be the same as in an async finalization check
class AcceleratorStream {
protected:
	std::queue<std::function<std::function<bool(void)>(void)>> _queuedStreamExecutors;
	std::queue<std::function<bool(void)>> _queuedEventFinalization;
	std::function<bool(void)> _currentStreamExecutorFinished;
	bool _ongoingExecutor;
	std::function<void(void)> _activateContext;
public:

	AcceleratorStream() :
		_ongoingExecutor(false),
		_activateContext([]{})	{
	}

	virtual ~AcceleratorStream()
	{
	}

	void addContext(std::function<void(void)> activate)
	{
		_activateContext = activate;
	}

	//a free operation is an operation that doesn't require
	//an activator
	virtual void addOperation(std::function<bool()> fun)
	{
		std::function<std::function<bool(void)>(void)> operation = [f = std::move(fun)]() -> std::function<bool(void)>
		{
			return f;
		};
		AcceleratorStream::addOperation(operation);
	}

	//an operation that requires an activator (does the functionality)
	//and then, the activator function returns a checker which is used
	//to know if the activated function has finished, asynchronously.
	virtual void addOperation(std::function<std::function<bool(void)>(void)> operation)
	{
		if (!_ongoingExecutor) {
			_currentStreamExecutorFinished = operation();
			_activateContext();

			_ongoingExecutor = true;
		} else
		{
			_queuedStreamExecutors.emplace(operation);
		}
	}

	virtual bool streamPendingExecutors()
	{
		return _queuedStreamExecutors.size() || _ongoingExecutor || _queuedEventFinalization.size();
	}

	virtual void streamAddEventListener(std::function<bool(void)> eventListener)
	{
		_queuedEventFinalization.push(std::move(eventListener));
	}

	inline void processEvents()
	{
		while(!_queuedEventFinalization.empty() && _queuedEventFinalization.front()())
		{
			_activateContext();
			_queuedEventFinalization.pop();
		}
	}

	inline void processExecutors()
	{
		if (_ongoingExecutor && _currentStreamExecutorFinished()) {
			_ongoingExecutor = false;
			_activateContext();
			if (!_queuedStreamExecutors.empty()) {
				//we call the activator function of the next stream executor and put the returned checker function in our probe.
				_currentStreamExecutorFinished = _queuedStreamExecutors.front()();
				_queuedStreamExecutors.pop();
				_ongoingExecutor = true;
			}
		}
	}

	virtual void streamServiceLoop()
	{
		processEvents();
		processExecutors();
	}
};

#endif //ACCELERATOR_STREAM_HPP
