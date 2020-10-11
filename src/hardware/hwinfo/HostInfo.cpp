/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#include <cassert>
#include <hwloc.h>
#include <unistd.h>

#include "HostInfo.hpp"
#include "hardware/places/L2Cache.hpp"
#include "hardware/places/L3Cache.hpp"
#include "hardware/places/NUMAPlace.hpp"
#include "executors/threads/CPU.hpp"
#include "hardware/places/NUMAPlace.hpp"
#include "lowlevel/FatalErrorHandler.hpp"
#include "lowlevel/Padding.hpp"

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
//! Check that hwloc headers match with runtime.
#if HWLOC_API_VERSION >= 0x00020000
	FatalErrorHandler::failIf(hwloc_get_api_version() < 0x20000, "hwloc headers are more recent than runtime library.");
#else
	FatalErrorHandler::failIf(hwloc_get_api_version() >= 0x20000, "hwloc headers are older than runtime library.");
#endif

	//! Hardware discovery
	hwloc_topology_t topology;
	hwloc_topology_init(&topology);  // initialization

#if HWLOC_API_VERSION >= 0x00020100
	// Do not omit empty NUMA nodes. This option is only supported from hwloc 2.1.0
	// It will mimic the behaviour of hwloc 1.x with disallowed resources.
	hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_INCLUDE_DISALLOWED);
#endif

	hwloc_topology_load(topology);   // actual detection

	//! Create NUMA addressSpace
	AddressSpace *NUMAAddressSpace = new AddressSpace();

	//! Get the number of physical packages in the machine
	int depthPhysicalPackages = hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE);
	if (depthPhysicalPackages != HWLOC_TYPE_DEPTH_UNKNOWN) {
		_numPhysicalPackages = hwloc_get_nbobjs_by_depth(topology, depthPhysicalPackages);
	} else {
		_numPhysicalPackages = 0;
	}

	//! Get NUMA nodes of the machine.
	//! NUMA node means: A set of processors around memory which the processors can directly access. (Extracted from hwloc documentation)
	size_t memNodesCount = hwloc_get_nbobjs_by_type(topology, HWLOC_NUMA_ALIAS);

	//! Check if HWLOC has found any NUMA node.
	if (memNodesCount != 0) {
		_memoryPlaces.resize(memNodesCount);
	} else {
		memNodesCount = 1;
		_memoryPlaces.resize(1);

		//! There is no NUMA info. We assume we have a single MemoryPlace.
		//! Create a MemoryPlace.
		//! TODO: Index is 0 arbitrarily. Maybe a special index should be set.
		//! Create the MemoryPlace representing the NUMA node with its index and AddressSpace.
		NUMAPlace *node = new NUMAPlace(/* Index */ 0, NUMAAddressSpace);

		//! Add the MemoryPlace to the list of memory nodes of the HardwareInfo.
		_memoryPlaces[node->getIndex()] = node;
		_validMemoryPlaces = 1;
	}

	//! Get (logical) CPUs of the machine
	size_t cpuCount = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
	_computePlaces.resize(cpuCount);

	//! Get physical core count
	size_t coreCount = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
	for (size_t i = 0; i < cpuCount; i++) {
		hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, i);
		assert(obj != nullptr);

#if HWLOC_API_VERSION >= 0x00020000
		hwloc_obj_t ancestor = nullptr;
		hwloc_obj_t nodeNUMA = nullptr;

		//! NUMA node can be found in different depths of ancestors (ordered from deeper to narrower):
		//! 1. A L3CACHE object / A GROUP object.
		//! 2. The most common is a PACKAGE object.
		//! 3. The MACHINE object.
		//! ref: https://www.open-mpi.org/projects/hwloc/doc/v2.0.0/a00327.php
		ancestor = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_L3CACHE, obj);
		if (ancestor == nullptr || ancestor->memory_arity != 1) {
			ancestor = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_GROUP, obj);
			if (ancestor == nullptr || ancestor->memory_arity != 1) {
				ancestor = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_PACKAGE, obj);
				if (ancestor == nullptr || ancestor->memory_arity != 1) {
					ancestor = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_MACHINE, obj);
				}
			}
		}
		assert(ancestor != nullptr);
		assert(ancestor->memory_arity == 1);

		nodeNUMA = ancestor->memory_first_child;
		assert(nodeNUMA != nullptr);
		assert(hwloc_obj_type_is_memory(nodeNUMA->type));
		hwloc_obj_t L3CacheObj = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_L3CACHE, obj);
		hwloc_obj_t L2CacheObj = hwloc_get_ancestor_obj_by_type(topology, HWLOC_OBJ_L2CACHE, obj);
#else
		hwloc_obj_t nodeNUMA = hwloc_get_ancestor_obj_by_type(topology, HWLOC_NUMA_ALIAS, obj);
		hwloc_obj_t tmpCache = obj->parent;
		assert(tmpCache != nullptr);
		//! The first unified cache object found going up from the tmp should be its cache.
		while(!(tmpCache->type == HWLOC_OBJ_CACHE && tmpCache->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED && tmpCache->attr->cache.depth == 2)) {
			tmpCache = tmpCache->parent;
			assert(tmpCache != nullptr);
		}
		hwloc_obj_t L2CacheObj = tmpCache;
		while(!(tmpCache->type == HWLOC_OBJ_CACHE && tmpCache->attr->cache.type == HWLOC_OBJ_CACHE_UNIFIED && tmpCache->attr->cache.depth == 3)) {
			tmpCache = tmpCache->parent;
			assert(tmpCache != nullptr);
		}
		hwloc_obj_t L3CacheObj = tmpCache;
#endif

		//! Check if L3 cache object is already created.
		bool alreadyExists = false;
		L3Cache * l3cache = nullptr;
		for(L3Cache * cache : _L3Caches) {
			if(cache->getId() == L3CacheObj->logical_index) {
				l3cache = cache;
				alreadyExists = true;
			}
		}
		//! Create the L3 cache object if needed.
		if(!alreadyExists) {
			const char * inclusiveness = hwloc_obj_get_info_by_name(L3CacheObj, "Inclusive");
			bool isInclusive = ((inclusiveness != nullptr) && strcmp(inclusiveness, "1") == 0);
			l3cache = new L3Cache(L3CacheObj->logical_index, L3CacheObj->attr->cache.size, L3CacheObj->attr->cache.linesize, isInclusive);
			_L3Caches.push_back(l3cache);
		}
		assert(l3cache != nullptr);

		L2Cache * l2cache = nullptr;
		if(_L2Caches.size() >= L2CacheObj->logical_index+1)
			l2cache = _L2Caches[L2CacheObj->logical_index];
		else {
			l2cache = new L2Cache(L2CacheObj->logical_index, l3cache->getId(), L2CacheObj->attr->cache.size, L2CacheObj->attr->cache.linesize);
			_L2Caches.push_back(l2cache);
		}
		assert(l2cache != nullptr);

		size_t NUMANodeId = nodeNUMA == NULL ? 0 : nodeNUMA->logical_index;
		assert(nodeNUMA == NULL || _memoryPlaces.size() >= nodeNUMA->logical_index);
		if (_memoryPlaces[NUMANodeId] == nullptr) {
			//! Create the MemoryPlace representing the NUMA node with its index and AddressSpace
			NUMAPlace *node = new NUMAPlace(NUMANodeId, NUMAAddressSpace);

			//! Add the MemoryPlace to the list of memory nodes of the HardwareInfo
			_memoryPlaces[node->getIndex()] = node;
			_validMemoryPlaces++;
		}

		// Intertwine CPU IDs so that threads from different physical cores are
		// registered one after another (T0 from CPU0 to ID0, T1 from CPU0 to
		// ID"coreCount", etc.

		assert(obj->parent != NULL);
		assert(obj->parent->type == HWLOC_OBJ_CORE);
		size_t cpuLogicalIndex = (coreCount * obj->sibling_rank) + obj->parent->logical_index;
		assert(cpuLogicalIndex < cpuCount);

		CPU *cpu = new CPU(
			/* systemCPUID */ obj->os_index,
			/* virtualCPUID */ cpuLogicalIndex,
			NUMANodeId,
			l2cache,
			l3cache
		);

		// Set L3 penalty for data tracking only first time
		if (i == coreCount - 1) {
			double penalty = (double) (l3cache->getCacheSize() * getNumL3Cache()) / (double) (l2cache->getCacheSize() * getNumL2Cache());
			L3Cache::setPenalty(penalty);
		}

		_computePlaces[cpuLogicalIndex] = cpu;
	}

	assert(_validMemoryPlaces <= memNodesCount);

	if (_validMemoryPlaces < memNodesCount) {
		//! Create the MemoryPlaces representing the NUMA nodes containing no CPUs.
		for (size_t i = 0; i < memNodesCount; i++) {
			if (_memoryPlaces[i] == nullptr) {
				NUMAPlace *node = new NUMAPlace(i, NUMAAddressSpace);
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

		// Emit a warning if the runtime was configured with a wrong cacheline size for this machine.
		FatalErrorHandler::warnIf(_cacheLineSize != CACHELINE_SIZE,
			"Cacheline size of host (", _cacheLineSize, ") does not match ",
			"the configured size (", CACHELINE_SIZE, "). Performance may be sub-optimal.");
	} else {
		// In some machines, such as HCA-Merlin or Dibona,
		// hwloc cannot obtain cache information or just returns 0
		// If so, fall back to compile-time detected cacheline size.
		_cacheLineSize = CACHELINE_SIZE;
	}

	//! Attributes of system's memory
	_pageSize = sysconf(_SC_PAGESIZE);

	//! This is not so portable, but it works for more Unix-like stuff
	size_t nrPhysicalPages = sysconf(_SC_PHYS_PAGES);
	_physicalMemorySize = nrPhysicalPages * _pageSize;

	//! Associate CPUs with NUMA nodes
	for (MemoryPlace *memoryPlace : _memoryPlaces) {
		for (ComputePlace *computePlace : _computePlaces) {
			NUMAPlace *numaNode = (NUMAPlace *) memoryPlace;
			numaNode->addComputePlace(computePlace);
			computePlace->addMemoryPlace(numaNode);
		}
	}

	//! Get matrix of NUMA distances
	hwloc_distances_s *distances;
	//! nr points to the number of distance matrices that may be stored in distances
	unsigned nr = 1;
	//! These distances were obtained from the operating system or hardware.
	unsigned long kind = HWLOC_DISTANCES_KIND_FROM_OS;
	//! flags is currently unused, should be 0.
	unsigned long flags = 0;
	hwloc_distances_get(topology, &nr, &distances, kind, flags);
	unsigned nbobjs = distances->nbobjs;
	_NUMADistances = std::vector<uint64_t>(nbobjs*nbobjs, 0);
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
	//std::cout << "---------- DISTANCE MATRIX ----------" << std::endl;
	//for (unsigned i = 0; i < nbobjs; i++) {
	//	for (unsigned j = 0; j < nbobjs; j++) {
	//		std::cout << _NUMADistances[i*nbobjs+j] << " ";
	//	}
	//	std::cout << std::endl;
	//}
	//! Each distance matrix returned in the distances array should be released by the caller using hwloc_distances_release().
	hwloc_distances_release(topology, distances);

	// Other work
	// Release resources
	hwloc_topology_destroy(topology);
	_deviceInitialized = true;
}

HostInfo::~HostInfo()
{
	assert(!_memoryPlaces.empty());

	AddressSpace *NUMAAddressSpace = _memoryPlaces[0]->getAddressSpace();;
	for (size_t i = 0; i < _memoryPlaces.size(); ++i) {
		delete _memoryPlaces[i];
	}

	//! There is a single AddressSpace
	assert(NUMAAddressSpace != nullptr);

	delete NUMAAddressSpace;

	for (size_t i = 0; i < _computePlaces.size(); ++i) {
		delete _computePlaces[i];
	}
}
