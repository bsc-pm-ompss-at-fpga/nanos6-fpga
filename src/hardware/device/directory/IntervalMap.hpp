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
#include "lowlevel/FatalErrorHandler.hpp"
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

	//This will try to free all the data for a handle that is valid in a different device, this is done to try to free memory
	//instead of crashing when out-of-memory for a device. If there are tasks that are reading the DATA, they will lose the directory
	//entry with the valid data, but the inner region will not be freed until that task ends, so this function is safe to use.
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

		std::pair<uintptr_t, DirectoryEntry*> t = {itv.first, toAdd};
		return _inner_m.insert(hint, t);
	}

	inline IntervalMapIterator MODIFY_KEY(IntervalMapIterator it, std::pair<uintptr_t, uintptr_t> new_itv)
	{
		DirectoryEntry  * toAdd = it->second;
		assert(toAdd->getRight() > new_itv.second);

		toAdd->updateRange({new_itv});
		_inner_m.erase(it);
		
		uintptr_t key = toAdd->getLeft();
	
		return _inner_m.emplace(key, toAdd).first;
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

	IntervalMapIterator deleteIt(IntervalMapIterator itMapIt)
	{
		delete itMapIt->second;
		return _inner_m.erase(itMapIt);
	}

	//helper function to work with Data Access Regions
	bool applyToRange(const DataAccessRegion& dar, std::function<bool(DirectoryEntry  *)> function)
	{
		return applyToRange({(uintptr_t)dar.getStartAddress(), (uintptr_t)dar.getStartAddress() + (uintptr_t)dar.getSize()}, function);
	}

	//if function returns false, cancel execution and return false.
	bool applyToRange(std::pair<uintptr_t, uintptr_t> apply, std::function<bool(DirectoryEntry  *)> function)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);

		IntervalMapIterator it = getIterator(apply.first);
		IntervalMapIterator end_it = std::end(_inner_m);

		while(it != end_it && it->second->getLeft()<apply.first) ++it;

		while (it != end_it && it->second->getRight() <= apply.second) 
		{
			if (!function(it->second)) return false;
			++it;
		}

		return true;
	}


	void remRangeOnFlush(const DataAccessRegion& dar, const int remove_only_if_handle_equals = -1)
	{
		const std::pair<uintptr_t, uintptr_t> range((uintptr_t)dar.getStartAddress(), (uintptr_t)dar.getStartAddress() + (uintptr_t) dar.getSize());
		remRangeOnFlush(range,remove_only_if_handle_equals);

	}

	void remRangeOnFlush(const std::pair<uintptr_t, uintptr_t>& range_to_del, const int remove_only_if_handle_equals = -1)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);

		auto it = getIterator(range_to_del.first);

		while(it != std::end(_inner_m) && it->second->getRight() <= range_to_del.second)
		{
			if(it->second->getNoFlush())
			{
				it->second->setNoFlushState(false);
				++it;
			}
			else 
			{
				if(remove_only_if_handle_equals == -1 || remove_only_if_handle_equals == it->second->getHome())
					it = deleteIt(it);
				else ++it;
			}

		}

	}

	
	//helper function to add ranges
	void addRange(const DataAccessRegion& dar)
	{
		addRange({(uintptr_t)dar.getStartAddress(), (uintptr_t)dar.getStartAddress() + (uintptr_t)dar.getSize()});
	}


	void addRange(const std::pair<uintptr_t, uintptr_t>& new_range_to_add)
	{
		std::lock_guard<std::mutex> guard(_map_mtx);
		
		assert(new_range_to_add.first <= new_range_to_add.second);

		auto new_range_left = new_range_to_add.first;
		auto new_range_right = new_range_to_add.second;

		if(new_range_left == new_range_right) new_range_right += sizeof(void*);

		//If there is no elements on the map, add the access directly.
		if (_inner_m.empty()) {
			ADD(std::end(_inner_m), {new_range_left, new_range_right}, nullptr, __LINE__);
			return;
		}

		IntervalMapIterator beg = _inner_m.lower_bound(new_range_left);
		if (beg != std::begin(_inner_m)) beg--; //get the leftmost closer entry 


		//If the right of the current interval in the iterator is less than the new left range, iterate
		//until getting the first entry that is equal or greater than our range
		while (beg != std::end(_inner_m) && beg->second->getRight() < new_range_left) ++beg;


		//if there wasn't an entry greater than our new range, we can add it directly and return. this will add any -rightmost- entry
		if (beg == std::end(_inner_m)) {
			ADD(std::end(_inner_m), new_range_to_add, nullptr, __LINE__);
			return;
		}


		//here we will begin 
		while (beg != std::end(_inner_m)) 
		{
			const auto range  =  beg->second->getRange();
			const auto current_in_map_left = range.first;
			const auto current_in_map_right = range.second;

			const auto intersect = beg->second->getIntersection(new_range_to_add);
			const auto intersect_left = intersect.first;
			const auto intersect_right = intersect.second;
			const bool no_intersection = intersect_left > intersect_right;
			const bool intersection = !no_intersection;
			const bool exact_match = new_range_left == current_in_map_left && new_range_right == current_in_map_right;


			//this lambda ensures that the result is const -- c++ idiomatic
			const std::pair<bool, bool> not_map_end__region_at_left = [&]()-> std::pair<bool, bool>
			{
				IntervalMapIterator begp = beg; 
				begp++;
				if(begp == std::end(_inner_m)) return {false, false};
				else return {true, begp->first >= new_range_right};
			}();

			const bool is_not_map_end = not_map_end__region_at_left.first ;
			const bool new_region_is_at_the_left = not_map_end__region_at_left.second;

			//If there is an exact match, we can return. Else, if we have no intersection, we add the new range directly and return.
			if (new_range_left >= new_range_right || exact_match) return;
			//If the new region is at the left of the current regions, we add it. 
			//This is the equivalent of the -rightmost- entry before the while.
			else if (no_intersection && is_not_map_end && new_region_is_at_the_left) 
			{				

				ADD(beg, {new_range_left, new_range_right}, nullptr, __LINE__);
				return;
			}
			//if the  new range left and current right are equal, we need the next entry to check for partitions of matches.
			else if (new_range_left == current_in_map_right) beg++;
			//intersection rules
			else if (new_range_left < intersect_left) 
			{
				if(no_intersection)//there isn't an intersection, so we can add the entry to the map
				{
					beg = ADD(beg, {new_range_left, new_range_right}, nullptr, __LINE__);
					return;
				}
				else //there is an intersection, we add the missing partition to the left and update the current range
				{
					beg = ADD(beg, {new_range_left, intersect_left}, nullptr, __LINE__);
					new_range_left = intersect_left;
				}
			} 
			//if overlaps a region
			else if (new_range_left == current_in_map_left) 
			{
				//if overlaps partially, we partition the directory mantaining the region attributes
				//current allocation:  @@@@@@@@
				//region to add:       ##
				// --------------------
				//result:   		   ##@@@@@@ two different partitions that share attributes
				if (intersect_right < current_in_map_right) {
					beg = MODIFY_KEY(beg, {intersect_left, intersect_right});
					beg = ADD(beg, {intersect_right, current_in_map_right}, beg->second, __LINE__);
				}
				new_range_left = intersect_right;
			}
			//if it overlaps but not at the beginning of the interval, it must partitionate one more time
			//current allocation:  @@@@@@@@
			//region to add:         ##
			// --------------------
			//result:   		   @@##%%%% three different partitions that share attributes
			else if (new_range_left > current_in_map_left) 
			{
				if (intersection) 
				{
					beg = MODIFY_KEY(beg, {current_in_map_left, intersect_left});
					beg = ADD(beg, {intersect_left, intersect_right}, beg->second, __LINE__);
					beg = ADD(beg, {intersect_right, current_in_map_right}, beg->second, __LINE__);
				} else {
					//if there is no intersection with the current entry, try the next.
					//when beg results in end it will exit the loop
					beg++;
				}
			} else {
				FatalErrorHandler::fail("Device Directory Internal Structure failure\n");
			}
		}

		//once the directory has no more entries that match, if there is still a region that is not overlapping, add it.
		if (new_range_left < new_range_right)
			ADD(std::end(_inner_m), {new_range_left, new_range_right}, nullptr, __LINE__);


	}

};

#endif //DEVICE_DIRECTORY_INTERVAL_MAP_HPP
