/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_FPGA_EVENTS_HPP
#define INSTRUMENT_FPGA_EVENTS_HPP


#include <cstdint>
#include <nanos6.h>

#include <InstrumentInstrumentationContext.hpp>
#include <InstrumentThreadInstrumentationContext.hpp>

namespace Instrument {
	//! This function is called at startup when an FPGA is known to have to start emiting events 
	void startFPGAInstrumentation();
	//! This function is called at exit, if startFPGAInstrumentation has been called.
	void stopFPGAInstrumentation();
	//! This function is called upon recieving an event
	void emitFPGAEvent(uint64_t value, uint32_t eventId, uint32_t eventType, uint64_t utime);
}


#endif // INSTRUMENT_FPGA_EVENTS_HPP
