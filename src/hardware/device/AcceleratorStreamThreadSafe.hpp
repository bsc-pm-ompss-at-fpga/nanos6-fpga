/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_STREAM_THREAD_SAFE_HPP
#define ACCELERATOR_STREAM_THREAD_SAFE_HPP

#include "AcceleratorStream.hpp"
#include <mutex>

class AcceleratorStreamThreadSafe : public AcceleratorStream {
	std::mutex _op_mtx;
public:

	void addOperation(std::function<std::function<bool(void)>(void)> operation)
	{
		std::lock_guard<std::mutex> guard(_op_mtx);
		AcceleratorStream::addOperation(operation);
	}

	void addOperation(std::function<bool(void)> operation)
	{
		std::lock_guard<std::mutex> guard(_op_mtx);
		AcceleratorStream::addOperation(operation);
	}

	void streamServiceLoop()
	{
		{
			processEvents();
		}
		{
			std::lock_guard<std::mutex> guard(_op_mtx);
			processExecutors();
		}
	}
};


#endif //ACCELERATOR_STREAM_HPP
