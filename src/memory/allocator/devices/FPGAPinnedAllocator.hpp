#pragma once

#include "support/config/ConfigCentral.hpp"
#include "SimpleAllocator.hpp"
#include <libxtasks.h>
class FPGAPinnedAllocator: public SimpleAllocator 
{

   private: 
   xtasks_mem_handle _handle; //!< Memory chunk handler for xTasks library
   size_t _allocated_memory;
   uint64_t _phys_base_addr;

   public: 
   
   FPGAPinnedAllocator() 
   {
      const size_t userRequestedSize = ConfigVariable<size_t>("devices.fpga.requested_fpga_memory").getValue();
      size_t size = userRequestedSize > 0 ? userRequestedSize :512*1024*1024; 
      xtasks_stat status;
      status = xtasksMalloc(size, & _handle);
      if (status != XTASKS_SUCCESS) {
         // Before fail, try to allocate less memory
         do {
         size = size / 2;
         status = xtasksMalloc(size, & _handle);
         } while (status != XTASKS_SUCCESS && size > 32768 /* 32KB */ );
         _allocated_memory = size;
         // Emit a warning with the allocation result
         if (status == XTASKS_SUCCESS && userRequestedSize > 0) 
         {
         std::cerr<< "Could not allocate requested amount of FPGA device memory (" << userRequestedSize <<" bytes). Only " << size << " bytes have been allocated."<<std::endl;
         }
         else if (status != XTASKS_SUCCESS) 
         {
            std::cerr<<"Could not allocate FPGA device memory for the FPGAPinnedAllocator"<<std::endl;
            abort();
         }
      }

      status = xtasksGetAccAddress(_handle, &_phys_base_addr);
      if (status != XTASKS_SUCCESS)
      {
         std::cerr << "Error getting the FPGA device address for the FPGAPinnedAllocator" << std::endl;
         abort();
      }

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
      if(size > _allocated_memory) 
      {
         printf("Can't allocate this much memory for FPGA");
         abort();
      }
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
      //printf(" FPGA_ADDR: [0x%lX (offset %lu)]  HOST_ADDR:  [%p]  SIZE:  [%lX] %s\n", fpga_addr, fpga_addr-_phys_base_addr,host_addr,count,kind==XTASKS_HOST_TO_ACC?"HOST->FPGA":"FPGA->HOST");
		if(xtasksMemcpy(_handle ,fpga_addr-_phys_base_addr, count, host_addr, kind) != XTASKS_SUCCESS) abort();
	}

	xtasks_memcpy_handle* memcpyAsync(void *dst,  void *src, size_t count, xtasks_memcpy_kind kind)
	{
      xtasks_memcpy_handle *cpyHandle = new xtasks_memcpy_handle;		

      
      size_t fpga_addr = (size_t) (kind==XTASKS_HOST_TO_ACC? dst : src);
      void* host_addr = (void*) (kind==XTASKS_HOST_TO_ACC? src : dst);
		if(xtasksMemcpyAsync(_handle ,fpga_addr-_phys_base_addr, count, host_addr, kind, cpyHandle) != XTASKS_SUCCESS) abort();
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
