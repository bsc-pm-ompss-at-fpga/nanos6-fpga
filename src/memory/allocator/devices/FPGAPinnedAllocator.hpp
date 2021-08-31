#pragma once

#include "support/config/ConfigCentral.hpp"
#include "support/config/ConfigVariable.hpp"
#include "SimpleAllocator.hpp"
#include <libxtasks.h>
class FPGAPinnedAllocator: public SimpleAllocator 
{

   private: 
   xtasks_mem_handle _handle; //!< Memory chunk handler for xTasks library
   size_t _allocated_memory;
   uint64_t _phys_base_addr;

   public: 
   
   FPGAPinnedAllocator(int deviceId)
   {
      const size_t userRequestedSize = ConfigVariable<size_t>("devices.fpga.requested_fpga_memory").getValue();
      size_t size = userRequestedSize > 0 ? userRequestedSize :512*1024*1024; 
      
      xtasks_stat status = xtasksMalloc(deviceId, size, &_handle);
      if (status != XTASKS_SUCCESS) 
      {
         // Before fail, try to allocate less memory
         do {
            size = size / 2;
            status = xtasksMalloc(deviceId, size, & _handle);
         } while (status != XTASKS_SUCCESS && size > 32768 /* 32KB */ );
         _allocated_memory = size;
         // Emit a warning with the allocation result

         FatalErrorHandler::failIf(status != XTASKS_SUCCESS, "Xtasks: failed to allocate memory");

         FatalErrorHandler::warnIf(userRequestedSize > 0,
            "Could not allocate requested amount of FPGA device memory (",userRequestedSize," bytes). Only " , size , " bytes have been allocated.");
      }


      FatalErrorHandler::failIf(
         xtasksGetAccAddress(_handle, &_phys_base_addr) != XTASKS_SUCCESS,
         "Error getting the FPGA device address for the FPGAPinnedAllocator"
      );

      init(_phys_base_addr, size);

      //std::cerr << "New FPGAPinnedAllocator created with size: " << size/1024 << "KB, base_addr: 0x" << std::hex << _phys_base_addr << std::dec << std::endl;
   }
   
   ~FPGAPinnedAllocator() 
   {
      xtasksFree(_handle);
   }

 

   std::pair<void *, bool> allocate(size_t size) 
   {
      static const std::size_t align = 16;
      FatalErrorHandler::failIf(
		   size > _allocated_memory,
		   "FPGA Pinned Allocator: Can't allocate this much memory for FPGA"
	   );
      lock();
      //Force the allocated sizes to be multiples of align
      //This prevents allocation of unaligned chunks
      //auto size_to_allocate = (size + align - 1) & (~(align - 1));
      auto pair_ptr_success = SimpleAllocator::allocate( ( size + align - 1 ) & ( ~( align - 1 ) ) );
     // printf("SimpleAllocator allocate. align: 0x%lX size: 0x%lX   size_to_call_allocator: 0x%lX \n", align, size, ( size + align - 1 ) & ( ~( align - 1 ) ));
     // printf("return allocator: 0x%lX SUCCESS: %s\n", (uint64_t) pair_ptr_success.first, pair_ptr_success.second?"YES":"NO");

      unlock();
      return pair_ptr_success;
   }

   size_t free(void * address) 
   {
      size_t ret;
      lock();
      ret = SimpleAllocator::free(address);
      unlock();
      return ret;
   }


	void memcpy(void *dst, void *src, size_t count, xtasks_memcpy_kind kind)
	{
      size_t fpga_addr = (size_t) (kind==XTASKS_HOST_TO_ACC? dst : src);
      void* host_addr = (void*) (kind==XTASKS_HOST_TO_ACC? src : dst);
	   FatalErrorHandler::failIf(
		   xtasksMemcpy(_handle ,fpga_addr-_phys_base_addr, count, host_addr, kind) != XTASKS_SUCCESS,
		   "Xtasks: failed to perform a memcpy sync"
	   );
   }

	xtasks_memcpy_handle* memcpyAsync(void *dst,  void *src, size_t count, xtasks_memcpy_kind kind)
	{
      xtasks_memcpy_handle *cpyHandle = new xtasks_memcpy_handle;		

      
      size_t fpga_addr = (size_t) (kind==XTASKS_HOST_TO_ACC? dst : src);
      void* host_addr = (void*) (kind==XTASKS_HOST_TO_ACC? src : dst);
      FatalErrorHandler::failIf(
		   xtasksMemcpyAsync(_handle ,fpga_addr-_phys_base_addr, count, host_addr, kind, cpyHandle) != XTASKS_SUCCESS,
		   "Xtasks: failed to perform a memcpy async"
	   );
		return cpyHandle;
	}

	static bool testAsyncDestroyOnSuccess(xtasks_memcpy_handle* handle)
	{
		if(xtasksTestCopy(handle) == XTASKS_SUCCESS) 
		{
			delete handle;
			return true;
		}
		return false;
	}

   /* \brief Returns the xTasks library handle for the memory region that is being managed
   */
   xtasks_mem_handle getBufferHandle() 
   {
      return _handle;
   }

};
