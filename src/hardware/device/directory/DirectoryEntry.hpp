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
    int   _home;
    bool  _no_flush;
    std::pair<uintptr_t, uintptr_t> _range;



    DirectoryEntry(size_t size):
        _valid_locations(std::vector<STATUS>(size, INVALID)),
        _perDeviceAllocation(shared_allocations(size, nullptr)),
        _modified_location(NOT_MODIFIED),
        _home(0),
        _no_flush(false),
        _range({0,0})
    {
        _valid_locations[0] = STATUS::VALID;
    };


    DirectoryEntry(const DirectoryEntry* itb):
        _valid_locations(itb->_valid_locations),
        _perDeviceAllocation(itb->_perDeviceAllocation),
        _modified_location(itb->_modified_location),
        _home(itb->_home),
        _no_flush(itb->_no_flush),
        _range(itb->_range)
    {

    }


    DirectoryEntry* clone()
    {
        return new DirectoryEntry(this);
    }



     //Directory Entry Utils

 	STATUS getStatus(int handler)  const   {return _valid_locations[handler];}
 	bool isValid(int handler)    const   {return _valid_locations[handler] == STATUS::VALID;}
 	bool isPending(int handler)  const   {return _valid_locations[handler] == STATUS::PENDING_TO_VALID;}
 	bool isInvalid(int handler)  const   {return _valid_locations[handler] == STATUS::INVALID;}
 	bool isModified(int handler) const   {return _modified_location == handler;}
    std::shared_ptr<DeviceAllocation>& getDeviceAllocation(int handler){return _perDeviceAllocation[handler];}


    void setHome(int handler){ _home = handler;}
    int  getHome() { return _home;}

    void setNoFlushState(bool flush_state ){ _no_flush = flush_state;}
    bool getNoFlush(){ return _no_flush; }
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

    void clearAllocations(size_t skip_handle = 0)
    {
        for(size_t i =  0; i < _perDeviceAllocation.size(); ++i)
            if(i != skip_handle)
                _perDeviceAllocation[i] = nullptr;
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
