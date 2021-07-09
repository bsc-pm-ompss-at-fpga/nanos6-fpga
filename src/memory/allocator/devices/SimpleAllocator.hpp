#pragma once
#include <stdint.h>
#include <map>
#include <list>
#include <ostream>
#include <mutex>


class SimpleAllocator
{
   private:

      std::map<uint64_t, std::size_t> _allocatedChunks;
      std::map<uint64_t, std::size_t> _freeChunks;

      uint64_t _baseAddress;
      std::mutex  _lock;
      std::size_t _remaining;
      std::size_t _capacity;

   public:
      typedef std::list< std::pair< uint64_t, std::size_t > > ChunkList;

      SimpleAllocator( uint64_t baseAddress, std::size_t len );

      // WARNING: Calling this constructor requires calling init() at some time
      // before any allocate() or free() methods are called
      SimpleAllocator() : _baseAddress( 0 ), _remaining( 0 )  { }

      void init( uint64_t baseAddress, std::size_t len );
      uint64_t getBaseAddress (){return _baseAddress;}
      void lock();
      void unlock();

      void * allocate( std::size_t len );
      void * alignedAllocate( std::size_t const alignment, std::size_t const len );
      std::size_t free( void *address );

      void canAllocate( std::size_t *sizes, unsigned int numChunks, std::size_t *remainingSizes ) const;
      void getFreeChunksList( ChunkList &list ) const;

      void printMap( std::ostream &o );
      std::size_t getCapacity() const;
      uint64_t getBasePointer( uint64_t address, size_t size );

};

class BufferManager
{
   private:
      void *      _baseAddress;
      std::size_t _index;
      std::size_t _size;

   public:
      BufferManager( void * address, std::size_t size );
      BufferManager() : _baseAddress(0),_index(0),_size(0) {} 

      ~BufferManager() {}

      void init ( void * address, std::size_t size );

      void * getBaseAddress () {return _baseAddress;}


      void * allocate ( std::size_t size );

      void reset ();
};

