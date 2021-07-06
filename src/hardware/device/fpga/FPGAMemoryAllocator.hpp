#pragma once

#include <libxtasks.h>
#include <utility>
#include <map>
class FPGAMemoryAllocator
{
    std::map<uintptr_t, xtasks_mem_handle> _allocationMap;

    public:
    FPGAMemoryAllocator(){

    }
    void* malloc(size_t size, unsigned=0)
    {
        xtasks_mem_handle memHandle;
        xtasksMalloc(size, &memHandle);
        uintptr_t phys;
        xtasksGetAccAddress(memHandle, &phys);
        setMemHandle(phys,memHandle);
        return (void*) phys;
    }

    void free(void* ptr)
    {
        xtasksFree(ptr);
    }

    std::pair<xtasks_mem_handle, size_t> getMemHandleAndOffset(void* phys)
    {
        auto it = _allocationMap.lower_bound((uintptr_t) phys);
        return {it->second, it->first-(uintptr_t) phys};
    }
    private: 

    void setMemHandle(uintptr_t phys, xtasks_mem_handle handle)
    {
        _allocationMap[phys] = handle;
    }

};