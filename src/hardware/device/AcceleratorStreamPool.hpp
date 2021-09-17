/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef ACCELERATOR_STREAM_POOL_HPP
#define ACCELERATOR_STREAM_POOL_HPP

#include "AcceleratorStream.hpp"

//This class controls the execution of all the streams of an specific accelerator.
//Limiting the number of streams per time and executing its logic.
class AcceleratorStreamPool {
private:
	std::vector<AcceleratorStream> _streams;
	std::queue<AcceleratorStream *> _streams_avail;

public:
	AcceleratorStreamPool(int numStreams, std::function<void(void)> activate) : _streams(numStreams)
	{
		for (auto &stream : _streams)
		{
			stream.addContext(activate);
			_streams_avail.push(&stream);
		}
	}

	bool streamAvailable()
	{
		return !_streams_avail.empty();
	}

	bool ongoingStreams()
	{
		return _streams.size() != _streams_avail.size();
	}

	void processStreams()
	{
		for (auto &stream : _streams)
			stream.streamServiceLoop();
	}

	AcceleratorStream *getStream()
	{
		auto stream = _streams_avail.front();
		_streams_avail.pop();
		return stream;
	}

	void releaseStream(AcceleratorStream *stream)
	{
		_streams_avail.push(stream);
	}
};

#endif // ACCELERATOR_STREAM_POOL_HPP
