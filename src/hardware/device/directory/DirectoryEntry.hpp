/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2020 Barcelona Supercomputing Center (BSC)
*/
#ifndef DIRECTORY_ENTRY_HPP
#define DIRECTORY_ENTRY_HPP

#include <utility>
#include <cstdint>
#include <cmath>
#include <vector>
#include <memory>
#include <csignal>

#include "../DeviceAllocation.hpp"

#include <dependencies/linear-regions/DataAccessRegion.hpp>

class DirectoryEntry
{

    public:
    enum  STATUS{INVALID = 1, VALID = 0, PENDING_TO_VALID = 2};

    static constexpr int NOT_MODIFIED = -1;

    using shared_allocations = std::vector<std::shared_ptr<DeviceAllocation>>;
    using location_status = std::vector<STATUS>;

    location_status     _valid_locations;
	shared_allocations  _perDeviceAllocation;
    int   _modified_location;
    std::pair<uintptr_t, uintptr_t> _range;

    int ENTRY_VAL;
 

    DirectoryEntry(size_t size):
        _valid_locations(std::vector<STATUS>(size, INVALID)),
        _perDeviceAllocation(shared_allocations(size, nullptr)),
        _modified_location(NOT_MODIFIED),
        _range({0,0})
    {
        _valid_locations[0] = STATUS::VALID;
        static int _e = 0;
        ENTRY_VAL = _e++;
    };
  

    DirectoryEntry(DirectoryEntry* itb):
        _valid_locations(itb->_valid_locations),
        _perDeviceAllocation(itb->_perDeviceAllocation),
        _modified_location(itb->_modified_location),
        _range(itb->_range),
        ENTRY_VAL(itb->ENTRY_VAL)
    {

    }


    DirectoryEntry* clone()
    {
        //printf("Cloning ENTRY_VAL: %d\n", ENTRY_VAL);
        return new DirectoryEntry(this);
    }



     //Directory Entry Utils

 	STATUS getStatus(int handler)  const   {return _valid_locations[handler];}
 	bool isValid(int handler)    const   {return _valid_locations[handler] == STATUS::VALID;}
 	bool isPending(int handler)  const   {return _valid_locations[handler] == STATUS::PENDING_TO_VALID;}
 	bool isInvalid(int handler)  const   {return _valid_locations[handler] == STATUS::INVALID;}
 	bool isModified(int handler) const   {return _modified_location == handler;}
    std::shared_ptr<DeviceAllocation>& getDeviceAllocation(int handler){return _perDeviceAllocation[handler];}



	void setValid(int handler ) {_valid_locations[handler] = STATUS::VALID;}
	void setPending(int handler){_valid_locations[handler] = STATUS::PENDING_TO_VALID;}
	void setInvalid(int handler){_valid_locations[handler] = STATUS::INVALID;}

	void setModified(int handler){_modified_location = handler;}

	void setStatus(int handler, STATUS status){_valid_locations[handler] = status;}
	void setDeviceAllocation(int handler, std::shared_ptr<DeviceAllocation>& deviceAllocation)
    {
        _perDeviceAllocation[handler] = deviceAllocation;
    }

    void clearDeviceAllocation(int handle)
    { 
        _perDeviceAllocation[handle] = nullptr;
        _valid_locations[handle] = STATUS::INVALID;
    }
    void clearValid()
    {
        std::fill(_valid_locations.begin(), _valid_locations.end(), STATUS::INVALID);
    }
   
    void clearAllocations()
    {
        for(auto& a : _perDeviceAllocation) a = nullptr;
        clearValid();
    }

    uintptr_t getTranslation(int handler, uintptr_t addr) const 
    { 
        if(handler == 0) return addr;
        assert(_perDeviceAllocation[handler] != nullptr);
        return _perDeviceAllocation[handler]->getTranslation(addr);
    }

    //get raw data
    uintptr_t getLeft() const{return _range.first;}
    uintptr_t getRight() const {return _range.second;}
    std::pair<uintptr_t, uintptr_t>   getRange() const {return _range;}

    int getModifiedLocation() const{ return _modified_location; }
    int getFirstValidLocation(int handle_ignore = -2) const
    { 
        for(size_t i = 0; i < _valid_locations.size(); ++i) 
        {
            if(handle_ignore ==  (int) i) continue;
            else if(isValid(i)) return i;
        }
        return 0;
    }

    //get processed data
    size_t getSize() const {return getRight()-getLeft();}
    DataAccessRegion getDataAccessRegion() const {return DataAccessRegion((void*) getLeft(), getSize());}
    std::pair<uintptr_t, uintptr_t> getIntersection(const std::pair<uintptr_t, uintptr_t>& other) const {return {std::max(getLeft(), other.first), std::min(getRight(), other.second)};}

    //setters
    void updateRange(const std::pair<uintptr_t, uintptr_t>& r){_range = r;}

};

#endif //DIRECTORY_ENTRY_HPP
