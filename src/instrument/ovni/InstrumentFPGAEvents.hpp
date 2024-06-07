/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2022 Barcelona Supercomputing Center (BSC)
*/
#ifndef INSTRUMENT_OVNI_FPGA_EVENTS_HPP
#define INSTRUMENT_OVNI_FPGA_EVENTS_HPP
#include "instrument/api/InstrumentFPGAEvents.hpp"

#include "OvniTrace.hpp"
#include "ovni.h"

namespace Instrument {
	inline uint64_t getCPUTimeForFPGA() {return ovni_clock_now();}
	inline void startFPGAInstrumentation() {
		Ovni::threadInit();
	}
	inline void stopFPGAInstrumentation() {
		ovni_flush();
		ovni_thread_free();
	}
	inline void emitFPGAEvent(uint64_t value, uint32_t eventId, uint32_t eventType, uint64_t utime) {
		Ovni::fpgaEvent(value, eventId, eventType, utime);
	}

	inline void emitReverseOffloadingEvent(uint64_t value, uint32_t eventType) {
		Ovni::reverseOffloadingEvent(value, eventType);
	}
}
#endif // INSTRUMENT_OVNI_FPGA_EVENTS_HPP
