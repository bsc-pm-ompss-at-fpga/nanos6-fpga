/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_CTF_INIT_AND_SHUTDOWN_HPP
#define INSTRUMENT_CTF_INIT_AND_SHUTDOWN_HPP

#include "executors/threads/CPUManager.hpp"
#include "instrument/api/InstrumentInitAndShutdown.hpp"


namespace Instrument {
	void initialize();
	void shutdown();
	void preinitFinished();

	int64_t getInstrumentStartTime();
}


#endif // INSTRUMENT_CTF_INIT_AND_SHUTDOWN_HPP
