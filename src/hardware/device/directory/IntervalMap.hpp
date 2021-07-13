/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/
#ifndef DEVICE_DIRECTORY_INTERVAL_MAP_HPP
#define DEVICE_DIRECTORY_INTERVAL_MAP_HPP

#include <algorithm>
#include <assert.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include "DirectoryEntry.hpp"

using IntervalMapType = std::map<uintptr_t, DirectoryEntry *>;
using IntervalMapKeyValue = std::pair<uintptr_t, DirectoryEntry *>;
using IntervalMapIterator = typename IntervalMapType::iterator;

class IntervalMap {
private:
	const int _numberOfAccelerators;
	std::mutex _map_mtx;

public:
	IntervalMapType _inner_m;

	IntervalMap():_numberOfAccelerators(0){}
	IntervalMap(int s):_numberOfAccelerators(s){}
	~IntervalMap()
	{
		for(auto& val: _inner_m) delete val.second;
	}

	inline void freeAllocationsForHandle(int handle, const std::vector<std::shared_ptr<DeviceAllocation>>& untouchableAllocations)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);
		const auto isTouchable = [&](auto& t){
			for(auto& a : untouchableAllocations) if(a == t) return false;
			return true;
		};

		for(auto & ent : _inner_m)
		{
			auto& v = *ent.second;
			if(isTouchable(v.getDeviceAllocation(handle)) && !v.isModified(handle))
			{
				const int fvl = v.getFirstValidLocation(handle);
				const bool isNotValidAnywhere = !v.isValid(0) && !v.isValid(handle);
				const bool isValidElsewhere = v.isValid(0) || fvl!=0;
				if(isNotValidAnywhere || isValidElsewhere)
				{
					v.clearDeviceAllocation(handle);
				}
			}
		}
	}

	inline IntervalMapIterator ADD(IntervalMapIterator hint, std::pair<uintptr_t, uintptr_t> itv, DirectoryEntry  *toClone,__attribute__((unused)) int line = 0)
	{
		DirectoryEntry  *toAdd;
		if (itv.first == itv.second)
			return hint;

		if (toClone == nullptr) toAdd = new DirectoryEntry (_numberOfAccelerators);
		else toAdd = toClone->clone();

		toAdd->updateRange({itv.first, itv.second});

		assert(itv.first < itv.second);

		std::pair<uintptr_t, DirectoryEntry  *> t = {itv.first, toAdd};
		return _inner_m.insert(hint, t);
	}

	inline IntervalMapIterator MODIFY_KEY(IntervalMapIterator it, std::pair<uintptr_t, uintptr_t> new_itv)
	{
		DirectoryEntry  * toAdd = it->second;
		assert(toAdd->getRight() > new_itv.second);

		toAdd->updateRange({new_itv});
		_inner_m.erase(it);
		
		uintptr_t key = toAdd->getLeft();
	
		std::pair<uintptr_t, DirectoryEntry  *> t = {key, toAdd};
		return _inner_m.insert(std::begin(_inner_m), t);
	}


	IntervalMapIterator getIterator(uintptr_t left)
	{
		assert(_inner_m.size() != 0);
		return _inner_m.find(left);
	}
	IntervalMapIterator getIterator(DataAccessRegion dar)
	{
		return getIterator((uintptr_t)dar.getStartAddress());
	}

	IntervalMapIterator getEnd()
	{
		return std::end(_inner_m);
	}

	bool applyToRange(const DataAccessRegion& dar, std::function<bool(DirectoryEntry  *)> function)
	{
		return applyToRange({(uintptr_t)dar.getStartAddress(), (uintptr_t)dar.getStartAddress() + (uintptr_t)dar.getSize()}, function);
	}

	//if function returns false, cancel execution and return false.
	bool applyToRange(std::pair<uintptr_t, uintptr_t> apply, std::function<bool(DirectoryEntry  *)> function)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);
		IntervalMapIterator it = getIterator(apply.first);
		while(it != getEnd() && it->second->getLeft()<apply.first) ++it;
		while (it != getEnd() && it->second->getRight() <= apply.second) {
			if (!function(it->second)) {
				return false;
			}

			++it;
		}
		return true;
	}

	void addRange(const DataAccessRegion& dar)
	{
		addRange({(uintptr_t)dar.getStartAddress(), (uintptr_t)dar.getStartAddress() + (uintptr_t)dar.getSize()});
	}


	void addRange(std::pair<uintptr_t, uintptr_t> new_range_to_add)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);
		
		assert(new_range_to_add.first <= new_range_to_add.second);
		if(new_range_to_add.first == new_range_to_add.second) new_range_to_add.second+=sizeof(void*);
		if (_inner_m.size() == 0) {
			ADD(std::end(_inner_m), new_range_to_add, nullptr, __LINE__);
			return;
		}

		IntervalMapIterator beg = _inner_m.lower_bound(new_range_to_add.first);
		if (beg != std::begin(_inner_m))
			beg--; //if not exact match

		auto &new_range_left = new_range_to_add.first;
		auto &new_range_right = new_range_to_add.second;

		while (beg != std::end(_inner_m) && beg->second->getRight() < new_range_to_add.first)
			++beg;
		if (beg == std::end(_inner_m)) {
			ADD(std::end(_inner_m), new_range_to_add, nullptr, __LINE__);
			return;
		}

		while (beg != std::end(_inner_m)) 
		{
			auto range  =  beg->second->getRange();
			auto current_in_map_left = range.first;
			auto current_in_map_right = range.second;

			auto intersect = beg->second->getIntersection(new_range_to_add);
			auto intersect_left = intersect.first;
			auto intersect_right = intersect.second;

			bool no_intersection = intersect_left > intersect_right;
			IntervalMapIterator begp = beg;
			begp++;
			if (no_intersection && begp != std::end(_inner_m) && begp->first >= new_range_right) {
				ADD(beg, {new_range_left, new_range_right}, nullptr, __LINE__);
				return;
			}
			bool intersection = !no_intersection;
			if (new_range_left >= new_range_right)
			{
				return;

			}

			bool exact_match = new_range_left == current_in_map_left && new_range_right == current_in_map_right;

			if (exact_match) return;
			else if (new_range_left == current_in_map_right) beg++;
			else if (new_range_left < intersect_left) 
			{
				if(no_intersection)
				{
					beg = ADD(beg, {new_range_left, new_range_right}, nullptr, __LINE__);
					return;
				}
				else
				{
					beg = ADD(beg, {new_range_left, intersect_left}, nullptr, __LINE__);
					new_range_left = intersect_left;
				}
			} 
			else if (new_range_left == current_in_map_left) {
				if (intersect_right < current_in_map_right) {
					beg = MODIFY_KEY(beg, {intersect_left, intersect_right});
					beg = ADD(beg, {intersect_right, current_in_map_right}, beg->second, __LINE__);
				}
				new_range_left = intersect_right;
			} else if (new_range_left > current_in_map_left) {
				if (intersection) 
				{
					beg = MODIFY_KEY(beg, {current_in_map_left, intersect_left});
					beg = ADD(beg, {intersect_left, intersect_right}, beg->second, __LINE__);
					beg = ADD(beg, {intersect_right, current_in_map_right}, beg->second, __LINE__);
				} else {
					beg++;
				}
			} else {
				printf("Device Directory Internal Structure failure\n");
				abort();
			}
		}


		if (new_range_left < new_range_right)
			ADD(std::end(_inner_m), {new_range_left, new_range_right}, nullptr, __LINE__);


	}

};

#endif //DEVICE_DIRECTORY_INTERVAL_MAP_HPP