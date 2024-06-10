#include "FPGAAcceleratorInstrumentation.hpp"
#include "InstrumentFPGAEvents.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include <stack>

void FPGAAcceleratorInstrumentation::serviceFunction(void *data)
{
    FPGAAcceleratorInstrumentation *acceleratorInstrumentation = (FPGAAcceleratorInstrumentation*)data;
    assert(acceleratorInstrumentation != nullptr);

    // Execute the service loop
    acceleratorInstrumentation->serviceLoop();
}

void FPGAAcceleratorInstrumentation::serviceCompleted(void *data)
{
    FPGAAcceleratorInstrumentation *acceleratorInstrumentation = (FPGAAcceleratorInstrumentation*)data;
    assert(acceleratorInstrumentation != nullptr);
    assert(acceleratorInstrumentation->_stopService);

    // Mark the service as completed
    acceleratorInstrumentation->_finishedService = true;
}

void FPGAAcceleratorInstrumentation::initializeService() {
    // Spawn service function
    // The spawn function creates a task and sends it to the nanos6 scheduler.
    // This means we can't control which thread executes the service and may be difficult to 
    // see in the trace. It may even migrate from one thread (worker) ot another.
    // Thus, for the moment we create a separate thread not controlled by nanos.
    // However, this generates oversubscription because the nanos scheduler doesn't know the cpu
    // assigned to the service thread.
/*    SpawnFunction::spawnFunction(
                serviceFunction, this,
                serviceCompleted, this,
                "FPGA accelerator instrumentation service", false
                );*/
    serviceThread = std::thread(serviceFunction, this);
}

void FPGAAcceleratorInstrumentation::shutdownService() {
    _stopService = true;
    while (!_finishedService);
    serviceThread.join();
}

void FPGAAcceleratorInstrumentation::serviceLoop() {
    constexpr int nEvents = 128;
    Instrument::startFPGAInstrumentation();
    bool getLastEvents = false;
    while (!_stopService || getLastEvents) {
        bool eventsFound = false;
        xtasks_ins_event events[nEvents];
        xtasks_stat res = xtasksGetInstrumentData(handle.handle, events, nEvents);
        FatalErrorHandler::failIf(
            res != XTASKS_SUCCESS,
            "Error while retrieving instrumentation events from handle with id: ", handle.info.id, ". xtasksGetInstrumentData returns: ", res
        );
        for (int i = 0; i < nEvents && events[i].eventType != XTASKS_EVENT_TYPE_INVALID; ++i) {
            eventsFound = true;
            switch(events[i].eventType) {
            case XTASKS_EVENT_TYPE_BURST_OPEN:
            case XTASKS_EVENT_TYPE_BURST_CLOSE:
                Instrument::emitFPGAEvent(events[i].eventType, events[i].eventId, events[i].value, ((events[i].timestamp-handle.startTimeFpga)*1'000'000)/(handle.info.freq) + handle.startTimeCpu);
                break;
            case XTASKS_EVENT_TYPE_POINT:
                // Events has been lost
                if (events[i].eventId == 82)
                    std::cout << "Warning: Event Lost : " << events[i].eventType << " Type: " << events[i].value << std::endl;
                break;
            default:
                std::cout << "Ignoring unkown fpga event type: " << events[i].eventType << " Type: " << events[i].value << std::endl;
            }
        }
        if (_stopService && !getLastEvents) getLastEvents = true;
        else if (!eventsFound) getLastEvents = false;
    }
    Instrument::stopFPGAInstrumentation();
    _finishedService = true;
}

