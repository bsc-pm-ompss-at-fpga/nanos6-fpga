/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2022 Barcelona Supercomputing Center (BSC)
*/

#ifndef INSTRUMENT_NULL_FPGA_EVENTS_HPP
#define INSTRUMENT_NULL_FPGA_EVENTS_HPP


#include "instrument/api/InstrumentFPGAEvents.hpp"
#include <cstdint>

namespace Instrument {
	inline uint64_t getCPUTimeForFPGA() {return 0;}
	inline void startFPGAInstrumentationNewThread() {}
	inline void startFPGAInstrumentation() {}
	inline void stopFPGAInstrumentation() {}
	inline void emitFPGAEvent([[maybe_unused]] uint64_t value, 
		[[maybe_unused]] uint32_t eventId, 
		[[maybe_unused]] uint32_t eventType,
		[[maybe_unused]] uint64_t utime) {
		}
	inline void emitReverseOffloadingEvent([[maybe_unused]] uint64_t value, 
		[[maybe_unused]] uint32_t eventType) {
		}
}



#endif // INSTRUMENT_NULL_FPGA_EVENTS_HPP
