/*
    This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

    Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "BroadcasterAccelerator.hpp"

BroadcasterAccelerator::BroadcasterAccelerator(const std::vector<Accelerator*>& _cluster) :
    Accelerator(0,
                nanos6_fpga_device,
                1,
                ConfigVariable<size_t>("devices.fpga.polling.period_us"),
                ConfigVariable<bool>("devices.fpga.polling.pinned")),
    cluster(_cluster)
{

}

void BroadcasterAccelerator::preRunTask(Task* task)
{
    task->getAcceleratorStream()->addOperation(
        [&] () -> std::function<bool()> {
            for (int i = 0; i < (int)cluster.size(); ++i) {
                AcceleratorStream& stream = acceleratorStreams[i];
                Accelerator* dev = cluster[i];
                for (const DistributedSymbol& sym : task->getDistSymbolInfo()) {
                    std::pair<void*, bool> allocation = dev->accel_allocate(sym.size);
                    FatalErrorHandler::failIf(!allocation.second, "Device Allocation: Out of space in device memory");
                    if (sym.isBroadcast) {
                        stream.addOperation(
                            dev->copy_in(
                                allocation.first,
                                sym.startAddress,
                                sym.size, nullptr)
                            );
                    }
                    else if (sym.isScatter) {
                        const size_t sizePerDev = sym.size/cluster.size();
                        stream.addOperation(
                            dev->copy_in(
                                allocation.first,
                                (void*)((size_t)sym.startAddress + i*sizePerDev),
                                sizePerDev, nullptr)
                            );
                    }
                    else {
                        assert(false);
                    }
                }
            }
            return [&] () -> bool {
                bool anyOngoing = false;
                for (AcceleratorStream& stream : acceleratorStreams) {
                    stream.streamServiceLoop();
                    if (!anyOngoing)
                        anyOngoing = stream.streamPendingExecutors();
                }
                return !anyOngoing;
            };
        }
    );
}

void BroadcasterAccelerator::callBody(Task *task) {
    task->getAcceleratorStream()->addOperation(
        [&] () -> std::function<bool()> {
            const std::vector<DistributedSymbol>& distSymbolInfo = task->getDistSymbolInfo();
            std::vector<nanos6_address_translation_entry_t> translation_table(distSymbolInfo.size());
            std::vector<const std::vector<void*>*> device_addresses(distSymbolInfo.size());

            for (int i = 0; i < (int)distSymbolInfo.size(); ++i) {
                const DistributedSymbol& sym = distSymbolInfo[i];
                assert(translationTable.find(sym.startAddress) != translationTable.end());
                device_addresses[i] = &translationTable[sym.startAddress];
            }

            for (int i = 0; i < (int)cluster.size(); ++i) {
                Accelerator* dev = cluster[i];
                for (int j = 0; j < (int)distSymbolInfo.size(); ++j) {
                    translation_table[j].device_address = (size_t)device_addresses[j]->at(i);
                }
                dev->generateDeviceEvironment(&deviceEnvironments[i], task->getDeviceSubType());
                task->getDeviceEnvironment() = deviceEnvironments[i];
                task->body(translation_table.data());
                acceleratorStreams[i].addOperation(
                    [&]() -> std::function<bool(void)> {
                        dev->submitDevice(deviceEnvironments[i]);
                        return [&]() -> bool {
                            return dev->checkDeviceSubmissionFinished(deviceEnvironments[i]);
                        };
                    });
            }
            return [&] () -> bool {
                bool anyOngoing = false;
                for (AcceleratorStream& stream : acceleratorStreams) {
                    stream.streamServiceLoop();
                    if (!anyOngoing)
                        anyOngoing = stream.streamPendingExecutors();
                }
                return !anyOngoing;
            };
        });
}

void BroadcasterAccelerator::postRunTask(Task *task) {
    task->getAcceleratorStream()->addOperation(
        [&] () -> std::function<bool()> {
            for (int i = 0; i < (int)cluster.size(); ++i) {
                AcceleratorStream& stream = acceleratorStreams[i];
                Accelerator* dev = cluster[i];
                for (const DistributedSymbol& sym : task->getDistSymbolInfo()) {
                    void* allocation = translationTable[sym.startAddress].at(i);
                    if (sym.isReceive) {
                        if (i == sym.receiveDevice)
                            stream.addOperation(
                                dev->copy_out(
                                    sym.startAddress,
                                    allocation,
                                    sym.size, nullptr)
                                );
                    }
                    else if (sym.isGather) {
                        const size_t sizePerDev = sym.size/cluster.size();
                        stream.addOperation(
                            dev->copy_in(
                                (void*)((size_t)sym.startAddress + i*sizePerDev),
                                allocation,
                                sizePerDev, nullptr)
                            );
                    }
                    else {
                        assert(false);
                    }
                }
            }
            return [&] () -> bool {
                bool anyOngoing = false;
                for (AcceleratorStream& stream : acceleratorStreams) {
                    stream.streamServiceLoop();
                    if (!anyOngoing)
                        anyOngoing = stream.streamPendingExecutors();
                }
                if (anyOngoing)
                    return false;
                for (const std::pair<void* const, std::vector<void*>>& p : translationTable) {
                    const std::vector<void*>& deviceAllocations = p.second;
                    for (int i = 0; i < (int)p.second.size(); ++i) {
                        cluster[i]->accel_free(deviceAllocations[i]);
                    }
                }
                translationTable.clear();
                return true;
            };
        }
    );
}

//Broadcaster device is the host
std::function<std::function<bool(void)>()> BroadcasterAccelerator::copy_in(
        [[maybe_unused]] void *dst,
[[maybe_unused]] void *src,
[[maybe_unused]] size_t size,
[[maybe_unused]] void* copy_extra) const
{
    return [] { return [] { return true; }; };
}

std::function<std::function<bool(void)>()> BroadcasterAccelerator::copy_out(
        [[maybe_unused]] void *dst,
[[maybe_unused]] void *src,
[[maybe_unused]] size_t size,
[[maybe_unused]] void* copy_extra) const
{
    return [] { return [] { return true; }; };
}

std::function<std::function<bool(void)>()> BroadcasterAccelerator::copy_between(
        [[maybe_unused]] void *dst,
[[maybe_unused]] int dstDevice,
[[maybe_unused]] void *src,
[[maybe_unused]] int srcDevice,
[[maybe_unused]] size_t size,
[[maybe_unused]] void *task) const
{
    assert(false);
    return [] { return [] { return true; }; };
}
