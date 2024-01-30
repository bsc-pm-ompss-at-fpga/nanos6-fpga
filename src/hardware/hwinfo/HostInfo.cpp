/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2023 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <hwloc.h>
#include <unistd.h>

#include "HostInfo.hpp"
#include "dependencies/DataTrackingSupport.hpp"
#include "executors/threads/CPU.hpp"
#include "hardware/places/NUMAPlace.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "lowlevel/Padding.hpp"
#include "hardware/device/host/HostAccelerator.hpp"
// Workaround to deal with changes in different HWLOC versions
#if HWLOC_API_VERSION < 0x00010b00
#define HWLOC_NUMA_ALIAS HWLOC_OBJ_NODE
#else
#define HWLOC_NUMA_ALIAS HWLOC_OBJ_NUMANODE
#endif

#ifndef HWLOC_OBJ_PACKAGE
#define HWLOC_OBJ_PACKAGE HWLOC_OBJ_SOCKET
#endif


HostInfo::HostInfo() :
	_validMemoryPlaces(0)
{
	_accelerators.resize(1);
	_accelerators[0] = new HostAccelerator();

// Check that hwloc headers match with runtime
#if HWLOC_API_VERSION >= 0x00020000
	FatalErrorHandler::failIf(hwloc_get_api_version() < 0x20000, "hwloc headers are more recent than runtime library");
#else
	FatalErrorHandler::failIf(hwloc_get_api_version() >= 0x20000, "hwloc headers are older than runtime library");
#endif

	// Hardware discovery
	hwloc_topology_t topology;
	hwloc_topology_init(&topology);  // initialization

#if HWLOC_API_VERSION >= 0x00020100
	// Do not omit empty NUMA nodes. This option is only supported from hwloc 2.1.0
	// It will mimic the behaviour of hwloc 1.x with disallowed resources
	hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_INCLUDE_DISALLOWED);
#endif

	hwloc_topology_load(topology);   // actual detection

	// Create NUMA addressSpace
	AddressSpace *NUMAAddressSpace = new AddressSpace();

	// Get the number of physical packages in the machine
	int depthPhysicalPackages = hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE);
	if (depthPhysicalPackages != HWLOC_TYPE_DEPTH_UNKNOWN) {
		_numPhysicalPackages = hwloc_get_nbobjs_by_depth(topology, depthPhysicalPackages);
	} else {
		_numPhysicalPackages = 0;
	}

	// Get the NUMA nodes. Extracted from hwloc documentation, a NUMA node is a
	// set of processors around memory which the processors can directly access
	size_t memNodesCount = hwloc_get_nbobjs_by_type(topology, HWLOC_NUMA_ALIAS);

	// Check if HWLOC has found any NUMA node
	if (memNodesCount != 0) {
		_memoryPlaces.resize(memNodesCount);
	} else {
		memNodesCount = 1;
		_memoryPlaces.resize(1);

		// There is no NUMA info. We assume we have a single MemoryPlace. Create
		// a MemoryPlace representing the NUMA node with its index and AddressSpace
		// TODO: Index is 0 arbitrarily; a special index should be set
		NUMAPlace *node = new NUMAPlace(/* Index */ 0, /* osIndex */ 0, NUMAAddressSpace);

		// Add the MemoryPlace to the list of memory nodes
		_memoryPlaces[node->getIndex()] = node;
		_validMemoryPlaces = 1;
	}

	// Get (logical) CPUs of the machine
	size_t cpuCount = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
	_computePlaces.resize(cpuCount);

	// Get physical core count
	size_t coreCount = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
	for (size_t i = 0; i < cpuCount; i++) {
		hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, i);
		assert(obj != nullptr);

#if HWLOC_API_VERSION >= 0x00020000
		hwloc_obj_t ancestor = nullptr;
		hwloc_obj_t nodeNUMA = nullptr;

		ancestor = obj->parent;
		while (ancestor != nullptr && ancestor->memory_arity == 0) {
			ancestor = ancestor->parent;
		}

		assert(ancestor != nullptr);
		bool found = false;
		for (size_t memChild = 0; memChild < ancestor->memory_arity; memChild++) {
			if (memChild == 0) {
				nodeNUMA = ancestor->memory_first_child;
			} else {
				assert(nodeNUMA != nullptr);
				nodeNUMA = nodeNUMA->next_sibling;
			}

			// Check that the object is actually a NUMA node
			// Check that the NUMA node is local to the PU
			if (hwloc_obj_type_is_memory(nodeNUMA->type) &&
					hwloc_bitmap_isset(obj->nodeset, nodeNUMA->os_index))
			{
				found = true;
				break;
			}
		}
		FatalErrorHandler::warnIf(!found, "Cannot find NUMA node of PU with OS index ", obj->os_index);

		// Some machines, particularly ARM-based, do not always provide cache
		// info. However, L3 may not exist, as in KNL in flat mode
		hwloc_obj_t L3CacheObj = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_L3CACHE, obj);
		hwloc_obj_t L2CacheObj = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_L2CACHE, obj);
#else
		hwloc_obj_t nodeNUMA = hwloc_get_ancestor_obj_by_type(topology, HWLOC_NUMA_ALIAS, obj);

		// Some machines, particularly ARM-based, do not always provide cache
		// info. The first unified cache object found going up from the tmp
		// should be its cache
		hwloc_obj_t tmpCache = obj->parent;
		// Some machines, particularly ARM-based, do not always provide cache info
		while (tmpCache != nullptr && tmpCache->attr != nullptr &&
				!(tmpCache->type == HWLOC_OBJ_CACHE &&
					tmpCache->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED &&
					tmpCache->attr->cache.depth == 2))
		{
			tmpCache = tmpCache->parent;

			// Topmost obj, no L2 found
			if (tmpCache == nullptr || tmpCache->type == HWLOC_OBJ_MACHINE)
				break;
		}

		hwloc_obj_t L2CacheObj = nullptr;
		hwloc_obj_t L3CacheObj = nullptr;
		if (tmpCache != nullptr) {
			L2CacheObj = tmpCache;
			// Some machines, particularly ARM-based, do not always provide cache info
			while (tmpCache != nullptr && tmpCache->attr != nullptr &&
					!(tmpCache->type == HWLOC_OBJ_CACHE &&
						tmpCache->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED &&
						tmpCache->attr->cache.depth == 3))
			{
				tmpCache = tmpCache->parent;

				// Topmost obj, no L3 found
				if (tmpCache == nullptr || tmpCache->type == HWLOC_OBJ_MACHINE)
					break;
			}
			L3CacheObj = tmpCache;
		}
#endif
		L3Cache *l3Cache = nullptr;
		if (L3CacheObj != nullptr) {

			// Check that L3 cache object is actually an L3. If there is no L3,
			// it will be another obj type
#if HWLOC_API_VERSION >= 0x00020000
			if (L3CacheObj->type == HWLOC_OBJ_L3CACHE
#else
			if (L3CacheObj->type == HWLOC_OBJ_CACHE
#endif
				&& L3CacheObj->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED
				&& L3CacheObj->attr->cache.depth == 3)
			{
				// Check if L3 cache object is already created
				if (_l3Caches.size() > L3CacheObj->logical_index) {
					l3Cache = _l3Caches[L3CacheObj->logical_index];
				} else {
					const char * inclusiveness = hwloc_obj_get_info_by_name(L3CacheObj, "Inclusive");
					bool isInclusive = ((inclusiveness != nullptr) && strcmp(inclusiveness, "1") == 0);
					l3Cache = new L3Cache(L3CacheObj->logical_index,
							L3CacheObj->attr->cache.size,
							L3CacheObj->attr->cache.linesize,
							isInclusive);
					assert(_l3Caches.size() == (size_t) L3CacheObj->logical_index);
					_l3Caches.push_back(l3Cache);
				}
				assert(l3Cache != nullptr);
			}
		}

		L2Cache *l2Cache = nullptr;
		if (L2CacheObj != nullptr) {
			if (_l2Caches.size() > L2CacheObj->logical_index) {
				l2Cache = _l2Caches[L2CacheObj->logical_index];
			} else {
				l2Cache = new L2Cache(L2CacheObj->logical_index,
						l3Cache,
						L2CacheObj->attr->cache.size,
						L2CacheObj->attr->cache.linesize);
				assert(_l2Caches.size() == (size_t) L2CacheObj->logical_index);
				_l2Caches.push_back(l2Cache);
			}
			assert(l2Cache != nullptr);
			assert(l2Cache->getId() == (int) L2CacheObj->logical_index);
		}

		// Set shouldEnableIS to a factor of L3 cache size
		size_t l3CacheSize = (l3Cache != nullptr) ? l3Cache->getCacheSize() : L3_DEFAULT_CACHE_SIZE;
		double factor = 0.1;
		DataTrackingSupport::setShouldEnableIS(l3CacheSize * factor);

		size_t NUMANodeId = nodeNUMA == NULL ? 0 : nodeNUMA->logical_index;
		assert(nodeNUMA == NULL || _memoryPlaces.size() >= nodeNUMA->logical_index);
		if (_memoryPlaces[NUMANodeId] == nullptr) {
			// Create the MemoryPlace representing the NUMA node with its index and AddressSpace
			int NUMANodeOsId = nodeNUMA == NULL ? -1 : nodeNUMA->os_index;
			NUMAPlace *node = new NUMAPlace(NUMANodeId, NUMANodeOsId, NUMAAddressSpace);

			// Add the MemoryPlace to the list of memory nodes of the HardwareInfo
			_memoryPlaces[node->getIndex()] = node;
			_validMemoryPlaces++;
		}

		assert(obj->parent != NULL);

		if (obj->parent->type != HWLOC_OBJ_CORE) {
			FatalErrorHandler::fail("The parent HWLOC object of a PU should be a CORE.\n"
				"This issue could be related to a missing system directory (e.g. /sys)");
		}

		// Intertwine CPU IDs so that threads from different physical cores are
		// registered one after another (T0 from CPU0 to ID0, T1 from CPU0 to
		// ID"coreCount", etc
		size_t cpuLogicalIndex = (coreCount * obj->sibling_rank) + obj->parent->logical_index;
		assert(cpuLogicalIndex < cpuCount);

		CPU *cpu = new CPU(
			/* systemCPUID */ obj->os_index,
			/* virtualCPUID */ cpuLogicalIndex,
			NUMANodeId,
			l2Cache,
			l3Cache
		);
		((NUMAPlace *)_memoryPlaces[NUMANodeId])->increaseNumLocalCores();

		_computePlaces[cpuLogicalIndex] = cpu;
	}

	assert(_validMemoryPlaces <= memNodesCount);

	if (_validMemoryPlaces < memNodesCount) {
		// Create the MemoryPlaces representing the NUMA nodes containing no CPUs
		for (size_t i = 0; i < memNodesCount; i++) {
			if (_memoryPlaces[i] == nullptr) {
				NUMAPlace *node = new NUMAPlace(i, /* osIndex */ -1, NUMAAddressSpace);
				_memoryPlaces[node->getIndex()] = node;
			}
		}
	}

	hwloc_obj_t cache = nullptr;
#if HWLOC_API_VERSION >= 0x00020000
	cache = hwloc_get_obj_by_type(topology, HWLOC_OBJ_L3CACHE, 0);

	// If we find no L3 cache, fall back to L1 to get the cache line size
	if (cache == nullptr)
		cache = hwloc_get_obj_by_type(topology, HWLOC_OBJ_L1CACHE, 0);
#else
	// Get L3 Cache
	int cacheDepth = hwloc_get_cache_type_depth(topology, 3, HWLOC_OBJ_CACHE_DATA);
	if (cacheDepth == HWLOC_TYPE_DEPTH_MULTIPLE || cacheDepth == HWLOC_TYPE_DEPTH_UNKNOWN) {
		// No matches for L3, try to get L1 instead
		cacheDepth = hwloc_get_cache_type_depth(topology, 1, HWLOC_OBJ_CACHE_DATA);
	}

	if (cacheDepth != HWLOC_TYPE_DEPTH_MULTIPLE && cacheDepth != HWLOC_TYPE_DEPTH_UNKNOWN) {
		cache = hwloc_get_obj_by_depth(topology, cacheDepth, 0);
	}
#endif

	if ((cache != nullptr) && (cache->attr->cache.linesize != 0)) {
		_cacheLineSize = cache->attr->cache.linesize;

		// Emit a warning if the runtime was configured with a wrong cacheline
		// size for this machine
		FatalErrorHandler::warnIf(_cacheLineSize != CACHELINE_SIZE,
			"Cacheline size of host (", _cacheLineSize, ") does not match ",
			"the configured size (", CACHELINE_SIZE, "). Performance may be sub-optimal");
	} else {
		// In some machines, such as HCA-Merlin or Dibona,
		// hwloc cannot obtain cache information or just returns 0
		// If so, fall back to compile-time detected cacheline size
		_cacheLineSize = CACHELINE_SIZE;
	}

	// Attributes of system's memory
	_pageSize = sysconf(_SC_PAGESIZE);

	// This is not so portable, but it works for more Unix-like stuff
	size_t nrPhysicalPages = sysconf(_SC_PHYS_PAGES);
	_physicalMemorySize = nrPhysicalPages * _pageSize;

	// Associate CPUs with NUMA nodes
	for (MemoryPlace *memoryPlace : _memoryPlaces) {
		for (ComputePlace *computePlace : _computePlaces) {
			NUMAPlace *numaNode = (NUMAPlace *) memoryPlace;
			numaNode->addComputePlace(computePlace);
			computePlace->addMemoryPlace(numaNode);
			computePlace->getDependencyData().initBytesInNUMA(memNodesCount);
		}
	}

	_NUMADistances.resize(memNodesCount * memNodesCount, 0);

	if (memNodesCount > 1) {
#if HWLOC_API_VERSION >= 0x00020000
		// Get matrix of NUMA distances
		hwloc_distances_s *distances;
		// The number of distance matrices that may be stored in distances
		unsigned nr = 1;
		// These distances were obtained from the operating system or hardware
		unsigned long kind = HWLOC_DISTANCES_KIND_FROM_OS;
		// The flags are currently unused, should be 0
		unsigned long flags = 0;
		hwloc_distances_get(topology, &nr, &distances, kind, flags);
		unsigned nbobjs = distances->nbobjs;
		assert(nbobjs == memNodesCount);
		for (unsigned i = 0; i < nbobjs; i++) {
			hwloc_obj_t obj = distances->objs[i];
			assert(obj->type == HWLOC_NUMA_ALIAS);
			for (unsigned j = i; j < nbobjs; j++) {
				hwloc_obj_t obj2 = distances->objs[j];
				assert(obj2->type == HWLOC_NUMA_ALIAS);
				hwloc_uint64_t distance1to2, distance2to1;
				hwloc_distances_obj_pair_values(distances, obj, obj2, &distance1to2, &distance2to1);
				_NUMADistances[i*nbobjs+j] = distance1to2;
				_NUMADistances[j*nbobjs+i] = distance2to1;
			}
		}
		hwloc_distances_release(topology, distances);
#else
		unsigned nbobjs = memNodesCount;
		for (unsigned i = 0; i < nbobjs; i++) {
			hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_NUMA_ALIAS, i);
			assert(obj != nullptr && obj->type == HWLOC_NUMA_ALIAS);
			for (unsigned j = i; j < nbobjs; j++) {
				hwloc_obj_t obj2 = hwloc_get_obj_by_type(topology, HWLOC_NUMA_ALIAS, j);
				assert(obj2->type == HWLOC_NUMA_ALIAS);
				float distance1to2, distance2to1;
				hwloc_get_latency(topology, obj, obj2, &distance1to2, &distance2to1);
				_NUMADistances[i*nbobjs+j] = distance1to2 * 10;
				_NUMADistances[j*nbobjs+i] = distance2to1 * 10;
			}
		}
#endif
	}

	// Release resources
	hwloc_topology_destroy(topology);
	_deviceInitialized = true;
}

HostInfo::~HostInfo()
{
	assert(!_memoryPlaces.empty());

	// Get the address space to later be deleted
	AddressSpace *NUMAAddressSpace = _memoryPlaces[0]->getAddressSpace();
	assert(NUMAAddressSpace != nullptr);

	for (MemoryPlace *memoryPlace : _memoryPlaces)
		delete memoryPlace;

	delete NUMAAddressSpace;

	for (Accelerator *accelerator : _accelerators) {
		assert(accelerator != nullptr);
		delete accelerator;
	}

	for (L2Cache *cache : _l2Caches)
		delete cache;

	for (L3Cache *cache : _l3Caches)
		delete cache;

	for (ComputePlace *computePlace : _computePlaces)
		delete computePlace;
}
