/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020-2021 Barcelona Supercomputing Center (BSC)
*/

#include "BroadcasterAccelerator.hpp"

BroadcasterAccelerator::BroadcasterAccelerator(const std::vector<Accelerator*>& _cluster) :
	Accelerator(0,
				nanos6_broadcaster_device,
				1,
				ConfigVariable<size_t>("devices.fpga.polling.period_us"),
				ConfigVariable<bool>("devices.fpga.polling.pinned")),
	cluster(_cluster),
	deviceEnvironments(_cluster.size()),
	acceleratorStreams(_cluster.size())
{

}

void BroadcasterAccelerator::mapSymbol(const void *symbol, size_t size)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	translationVector.resize(cluster.size());
	for (int i = 0; i < (int)cluster.size(); ++i) {
		Accelerator* dev = cluster[i];
		std::pair<void*, bool> allocation = dev->accel_allocate(size);
		FatalErrorHandler::failIf(!allocation.second,
				"Device Allocation: Out of space in device memory");
		translationVector[i] = allocation.first;
	}
}

void BroadcasterAccelerator::unmapSymbol(const void *symbol)
{
	std::unordered_map<const void*, std::vector<void*>>::iterator it = translationTable.find(symbol);
	std::vector<void*>& translationVector = it->second;
	for (int i = 0; i < (int)cluster.size(); ++i) {
		cluster[i]->accel_free(translationVector[i]);
	}
	translationTable.erase(it);
}

void BroadcasterAccelerator::memcpyToAll(const void *symbol, size_t size, size_t offset)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	for (int i = 0; i < (int)cluster.size(); ++i) {
		AcceleratorStream& stream = acceleratorStreams[i];
		Accelerator* dev = cluster[i];
		stream.addOperation(
			dev->copy_in(
				(void*)((uintptr_t)translationVector[i] + offset),
				(void*)((uintptr_t)symbol + offset),
				size, nullptr
			)
		);
	}

	bool anyOngoing;
	do {
		anyOngoing = false;
		for (AcceleratorStream& stream : acceleratorStreams) {
			stream.streamServiceLoop();
			if (!anyOngoing)
				anyOngoing = stream.streamPendingExecutors();
		}
	} while (anyOngoing);
}

void BroadcasterAccelerator::memcpyToDevice(int devId, const void *symbol, size_t size, size_t offset)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	AcceleratorStream& stream = acceleratorStreams[devId];
	Accelerator* dev = cluster[devId];
	stream.addOperation(
		dev->copy_in(
			(void*)((uintptr_t)translationVector[devId] + offset),
			(void*)((uintptr_t)symbol + offset),
			size, nullptr
		)
	);
	while (stream.streamPendingExecutors()) {
		stream.streamServiceLoop();
	}
}

void BroadcasterAccelerator::memcpyFromDevice(int devId, void *symbol, size_t size, size_t offset)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	AcceleratorStream& stream = acceleratorStreams[devId];
	Accelerator* dev = cluster[devId];
	stream.addOperation(
		dev->copy_out(
			(void*)((size_t)symbol + offset),
			(void*)((size_t)translationVector[devId] + offset),
			size, nullptr
		)
	);
	while (stream.streamPendingExecutors()) {
		stream.streamServiceLoop();
	}
}

void BroadcasterAccelerator::scatter(const void *symbol, size_t size, size_t sendOffset, size_t recvOffset)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	for (int i = 0; i < (int)cluster.size(); ++i) {
		AcceleratorStream& stream = acceleratorStreams[i];
		Accelerator* dev = cluster[i];
		stream.addOperation(
			dev->copy_out(
				(void*)((uintptr_t)translationVector[i] + recvOffset),
				(void*)((uintptr_t)symbol + sendOffset + size*i),
				size, nullptr
			)
		);
	}

	bool anyOngoing;
	do {
		anyOngoing = false;
		for (AcceleratorStream& stream : acceleratorStreams) {
			stream.streamServiceLoop();
			if (!anyOngoing)
				anyOngoing = stream.streamPendingExecutors();
		}
	} while (anyOngoing);
}

void BroadcasterAccelerator::gather(void *symbol, size_t size, size_t sendOffset, size_t recvOffset)
{
	std::vector<void*>& translationVector = translationTable[symbol];
	for (int i = 0; i < (int)cluster.size(); ++i) {
		AcceleratorStream& stream = acceleratorStreams[i];
		Accelerator* dev = cluster[i];
		stream.addOperation(
			dev->copy_in(
				(void*)((uintptr_t)symbol + recvOffset + size*i),
				(void*)((uintptr_t)translationVector[i] + sendOffset),
				size, nullptr
			)
		);
	}

	bool anyOngoing;
	do {
		anyOngoing = false;
		for (AcceleratorStream& stream : acceleratorStreams) {
			stream.streamServiceLoop();
			if (!anyOngoing)
				anyOngoing = stream.streamPendingExecutors();
		}
	} while (anyOngoing);
}

void BroadcasterAccelerator::preRunTask([[maybe_unused]] Task* task)
{
	/*task->getAcceleratorStream()->addOperation(
		[&, task] () -> std::function<bool()> {
			for (int i = 0; i < (int)cluster.size(); ++i) {
				AcceleratorStream& stream = acceleratorStreams[i];
				Accelerator* dev = cluster[i];
				for (const DistributedSymbol& sym : task->getDistSymbolInfo()) {
					std::pair<void*, bool> allocation = dev->accel_allocate(sym.size);
					FatalErrorHandler::failIf(!allocation.second,
							"Device Allocation: Out of space in device memory");
					translationTable[sym.startAddress].at(i) = allocation.first;
					if (sym.isBroadcast) {
						stream.addOperation(
							dev->copy_in(
								allocation.first,
								sym.startAddress,
								sym.size, nullptr
							)
						);
					}
					else if (sym.isScatter) {
						const size_t sizePerDev = sym.size/cluster.size();
						stream.addOperation(
							dev->copy_in(
								allocation.first,
								(void*)((size_t)sym.startAddress + i*sizePerDev),
								sizePerDev, nullptr
							)
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
	);*/
}

void BroadcasterAccelerator::callBody(Task *task) {
	task->getAcceleratorStream()->addOperation(
		[&, task] () -> std::function<bool()> {
			const std::vector<DistributedSymbol>& distSymbolInfo = task->getDistSymbolInfo();
			std::vector<nanos6_address_translation_entry_t> translation_table(distSymbolInfo.size());
			std::vector<const std::vector<void*>*> device_addresses(distSymbolInfo.size());
			char *argsBlock = new char[task->getArgsBlockSize()];

			for (int i = 0; i < (int)distSymbolInfo.size(); ++i) {
				const DistributedSymbol& sym = distSymbolInfo[i];
				assert(translationTable.find(sym.startAddress) != translationTable.end());
				device_addresses[i] = &translationTable[sym.startAddress];
				translation_table[i].local_address = (size_t)sym.startAddress;
			}

			for (int i = 0; i < (int)cluster.size(); ++i) {
				Accelerator* dev = cluster[i];
				for (int j = 0; j < (int)distSymbolInfo.size(); ++j) {
					translation_table[j].device_address = (size_t)device_addresses[j]->at(i);
				}
				dev->generateDeviceEvironment(deviceEnvironments[i], task->getDeviceSubType());
				memcpy(argsBlock, task->getArgsBlock(), task->getArgsBlockSize());
				task->getTaskInfo()->implementations[0].run(argsBlock, &deviceEnvironments[i], translation_table.data());
				acceleratorStreams[i].addOperation(
					[&, dev, i]() -> std::function<bool(void)> {
						dev->submitDevice(deviceEnvironments[i]);
						return dev->getDeviceSubmissionFinished(deviceEnvironments[i]);
					});
			}

			delete[] argsBlock;
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

void BroadcasterAccelerator::postRunTask([[maybe_unused]] Task *task) {
	/*task->getAcceleratorStream()->addOperation(
		[&, task] () -> std::function<bool()> {
			for (int i = 0; i < (int)cluster.size(); ++i) {
				AcceleratorStream& stream = acceleratorStreams[i];
				Accelerator* dev = cluster[i];
				for (const DistributedSymbol& sym : task->getDistSymbolInfo()) {
					void* allocation = translationTable[sym.startAddress].at(i);
					if (sym.isReceive) {
						if (i == sym.receiveDevice) {
							stream.addOperation(
								dev->copy_out(
									sym.startAddress,
									allocation,
									sym.size, nullptr
								)
							);
						}
					}
					else if (sym.isGather) {
						const size_t sizePerDev = sym.size/cluster.size();
						stream.addOperation(
								dev->copy_in(
								(void*)((size_t)sym.startAddress + i*sizePerDev),
								allocation,
								sizePerDev, nullptr
							)
						);
					}
					else {
						assert(false);
					}
				}
			}
			return [&, task] () -> bool {
				bool anyOngoing = false;
				for (AcceleratorStream& stream : acceleratorStreams) {
					stream.streamServiceLoop();
					if (!anyOngoing)
					anyOngoing = stream.streamPendingExecutors();
				}
				if (anyOngoing)
					return false;
				for (const DistributedSymbol& sym : task->getDistSymbolInfo()) {
					std::unordered_map<void*, std::vector<void*>>::iterator it =
							translationTable.find(sym.startAddress);
					std::vector<void*>& translationVector = it->second;
					for (int i = 0; i < (int)cluster.size(); ++i) {
						cluster[i]->accel_free(translationVector[i]);
					}
					translationTable.erase(it);
				}
				return true;
			};
		}
	);*/
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
