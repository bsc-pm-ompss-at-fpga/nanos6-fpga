#pragma once

#include "SimpleAllocator.hpp"
#include <libxtasks.h>

   class FPGAPinnedAllocator: public SimpleAllocator 
   {

   private: 
   xtasks_mem_handle _handle; //!< Memory chunk handler for xTasks library
   public: 
   
   FPGAPinnedAllocator() 
   {
      const size_t userRequestedSize =512*1024*1024;// FPGAConfig::getAllocatorPoolSize();
      size_t size = userRequestedSize > 0 ? userRequestedSize :512*1024*1024; //FPGAConfig::getDefaultAllocatorPoolSize();
      xtasks_stat status;
      status = xtasksMalloc(0x1000, & _handle);
      printf("FIRST HANDLE IS: %X\n", _handle);
      status = xtasksMalloc(size, & _handle);
      if (status != XTASKS_SUCCESS) {
         // Before fail, try to allocate less memory
         do {
         size = size / 2;
         status = xtasksMalloc(size, & _handle);
         } while (status != XTASKS_SUCCESS && size > 32768 /* 32KB */ );

         // Emit a warning with the allocation result
         if (status == XTASKS_SUCCESS && userRequestedSize > 0) 
         {
         std::cout<< "Could not allocate requested amount of FPGA device memory (" << userRequestedSize <<" bytes). Only " << size << " bytes have been allocated."<<std::endl;
      } else if (status != XTASKS_SUCCESS) {
         std::cout<<"Could not allocate FPGA device memory for the FPGAPinnedAllocator"<<std::endl;
         size = 0;
      }
   }
    std::cout<<"Allocated " << size << " bytes of FPGA device memory for the FPGAPinnedAllocator status is"<<status<<std::endl;
   printf("The handle is: %X\n", _handle);
   uint64_t addr = 0;
   if (status == XTASKS_SUCCESS) {
      status = xtasksGetAccAddress(_handle, & addr);
      //ensure( status == XTASKS_SUCCESS, " Error getting the FPGA device address for the FPGAPinnedAllocator" );
   }
   init(addr, size);

   // debug0( "New FPGAPinnedAllocator created with size: " << size/1024 << "KB, base_addr: 0x" <<    std::hex << addr << std::dec );
   }
   
   ~FPGAPinnedAllocator() 
   {
      xtasksFree(_handle);
   }

   void * allocate(size_t size) 
   {
      static const std::size_t align = 0x1000;
      lock();
      //Force the allocated sizes to be multiples of align
      //This prevents allocation of unaligned chunks
      void * ret = SimpleAllocator::allocate((size + align - 1) & (~(align - 1)));
      unlock();
      return ret;
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
      size_t fpga_addr = (size_t)(kind==XTASKS_HOST_TO_ACC? dst : src);
      void* host_addr = (void*) (kind==XTASKS_HOST_TO_ACC? src : dst);
      printf("MEMCPY normal FPGA: %lX HOST: %lX COUNT: %lX handle %X \n", fpga_addr, host_addr, count, _handle);

		if(xtasksMemcpy(_handle ,fpga_addr, count, host_addr, kind) != XTASKS_SUCCESS) abort();
	}

	xtasks_memcpy_handle* memcpyAsync(void *dst,  void *src, size_t count, xtasks_memcpy_kind kind)
	{
      xtasks_memcpy_handle *cpyHandle = new xtasks_memcpy_handle;		

      
      size_t fpga_addr = (size_t)(kind==XTASKS_HOST_TO_ACC? dst : src);
      void* host_addr = (void*) (kind==XTASKS_HOST_TO_ACC? src : dst);

      printf("MEMCPY async FPGA: %lX HOST: %lX COUNT: %lX\n", fpga_addr, host_addr, count);
		if(xtasksMemcpyAsync(_handle ,fpga_addr, count, host_addr, kind, cpyHandle) != XTASKS_SUCCESS) abort();

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
