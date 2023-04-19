/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2022 Barcelona Supercomputing Center (BSC)
*/
#ifndef INSTRUMENT_OVNI_FPGA_EVENTS_HPP
#define INSTRUMENT_OVNI_FPGA_EVENTS_HPP
#include "instrument/api/InstrumentFPGAEvents.hpp"

#include "OvniTrace.hpp"

namespace Instrument {
	inline void startFPGAInstrumentation() {
		Ovni::threadInit();

	}
	inline void stopFPGAInstrumentation() {
		Ovni::threadEnd();
		ovni_thread_free();
	}
	inline void emitFPGAEvent(uint64_t value, uint32_t eventId, uint32_t eventType, uint64_t utime) {
		std::cout << "event [Type: " << eventType << ", ID: " << eventId << ", Value: " << value << ", time: " << double(utime)/1'000'000 << "ms]" << std::endl;
		Ovni::fpgaEvent(value, eventId, eventType, utime);
	}
}
#endif // INSTRUMENT_OVNI_FPGA_EVENTS_HPP
