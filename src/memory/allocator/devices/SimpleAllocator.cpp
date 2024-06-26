#include <iostream>
#include <cstring>
#include <utility>

#include "SimpleAllocator.hpp"

SimpleAllocator::SimpleAllocator( uint64_t baseAddress, std::size_t len ) : _baseAddress( baseAddress ), _remaining ( len ), _capacity( len )
{
   _freeChunks[ baseAddress ] = len;
}

void SimpleAllocator::init( uint64_t baseAddress, std::size_t len )
{
   _baseAddress = baseAddress;
   _freeChunks[ baseAddress ] = len;
   _remaining = len;
   _capacity = len;
}

std::pair<void *, bool> SimpleAllocator::allocate( std::size_t size )
{
   std::map<uint64_t, std::size_t>::iterator mapIter = _freeChunks.begin();
   void * retAddr = (void *) 0;
  // ensure(size != 0, "Error, can't allocate 0 bytes.");

   while( mapIter != _freeChunks.end() && mapIter->second < size )
   {
      mapIter++;
   }
   if ( mapIter != _freeChunks.end() ) {

      uint64_t targetAddr = mapIter->first;
      std::size_t chunkSize = mapIter->second;

      _freeChunks.erase( mapIter );

      //add the chunk with the new size (previous size - requested size)
      if (chunkSize > size)
         _freeChunks[ targetAddr + size ] = chunkSize - size ;
      _allocatedChunks[ targetAddr ] = size;

      retAddr = ( void * ) targetAddr;
      _remaining -= size;
   }
   else return {nullptr, false};
   

   return {retAddr, true};
}

std::pair<void *, bool>  SimpleAllocator::alignedAllocate( std::size_t const alignment, std::size_t const size )
{
   std::map<uint64_t, std::size_t>::iterator mapIter = _freeChunks.begin();
   void * retAddr = (void *) 0;

   while( mapIter != _freeChunks.end() && 
      mapIter->second < 
      ( ( mapIter->first & ~(alignment-1) ) +
        ( ( ( mapIter->first & (alignment-1) ) == 0 ) ? 0 : alignment ) +
        size ) -
       mapIter->first )
   {
      mapIter++;
   }
   if ( mapIter != _freeChunks.end() ) {
      uint64_t chunkAddr = mapIter->first;
      std::size_t chunkSize = mapIter->second;
      uint64_t targetAddr = ( mapIter->first & ~(alignment-1) ) + ( ( ( mapIter->first & (alignment-1) ) == 0 ) ? 0 : alignment ) ;

      //add the chunk with the new size (previous size - requested size)
      if (chunkSize > size) {
         if (targetAddr == chunkAddr ) {
            _freeChunks.erase( chunkAddr ); 
         } else { 
            _freeChunks[ mapIter->first ] = ( targetAddr - chunkAddr );
         }
         if ((chunkAddr + chunkSize) - (targetAddr + size) > 0)
            _freeChunks[ targetAddr + size ] = (chunkAddr + chunkSize) - (targetAddr + size) ;
      } else if ( chunkSize == size ) {
         _freeChunks.erase( chunkAddr );
      } else { fprintf(stderr, "Error, this does not make sense!\n"); }
      _allocatedChunks[ targetAddr ] = size;

      retAddr = ( void * ) targetAddr ;
      _remaining -= size;
   }
   else {
      // Could not get a chunk of 'size' bytes
      //*myThread->_file << sys.getNetwork()->getNodeNum() << ": WARNING: Allocator is full" << std::endl;
      return {nullptr, false};
   }

   return {retAddr, true};
}

std::size_t SimpleAllocator::free( void *address )
{
 //  ensure( !_allocatedChunks.empty(), "Empty _allocatedChunks!");
   //*(myThread->_file) << "SimpleAllocator::free " << (void *) address << std::endl;
   std::map<uint64_t, std::size_t>::iterator mapIter = _allocatedChunks.find( ( uint64_t ) address );

   // Unknown address, simply ignore
   if( mapIter == _allocatedChunks.end() ) {
      //ensure0( false,"Unknown address deallocation (Simple Allocator)" ); //It can happen in OpenCL
      return 0;
   }

   size_t size = mapIter->second;
   std::pair< std::map<uint64_t, std::size_t>::iterator, bool > ret;
  // ensure (size != 0, "Invalid entry in _allocatedChunks, size == 0");

   _allocatedChunks.erase( mapIter );

   if ( !_freeChunks.empty() ) {
      mapIter = _freeChunks.lower_bound( ( uint64_t ) address );

      //case where address is the highest key, check if it can be merged with the previous chunk
      if ( mapIter == _freeChunks.end() ) {
         mapIter--;
         if ( mapIter->first + mapIter->second == ( uint64_t ) address ) {
            mapIter->second += size;
         } else {
            _freeChunks[ ( uint64_t ) address ] = size;
         }
      }
      //address is not the highest key, check if it can be merged with the previous and next chunks
      else if ( _freeChunks.key_comp()( ( uint64_t ) address, mapIter->first ) ) {
         std::size_t totalSize = size;
         bool firstMerge = false;

         //check next chunk
         if ( mapIter->first == ( ( uint64_t ) address ) + size ) {
            totalSize += mapIter->second;
            _freeChunks.erase( mapIter );
            ret = _freeChunks.insert( std::map<uint64_t, std::size_t>::value_type( ( uint64_t ) address, totalSize ) );
            mapIter = ret.first;
            firstMerge = true;
         }

         //check previous chunk
         if ( _freeChunks.begin() != mapIter )
         {
            mapIter--;
            if ( mapIter->first + mapIter->second == ( uint64_t ) address ) {
               //if totalSize > size then the next chunk was merged
               if ( totalSize > size ) {
                  mapIter->second += totalSize;
                  mapIter++;
                  _freeChunks.erase( mapIter );
               }
               else {
                  mapIter->second += size;
               }
            } else if ( !firstMerge ) {
               _freeChunks[ ( uint64_t ) address ] = size;
            }
         }
         else if ( !firstMerge ) {
            _freeChunks[ ( uint64_t ) address ] = size;
         }
      }
      //duplicate key, error
      else {
       //  *(myThread->_file) << "Duplicate entry in segment map, addr " << address << ", size " << size << ". Got entry with size " << mapIter->second << ". Remaining: "<< _remaining << std::endl;
       //  printBt(*(myThread->_file));
        // printMap(*(myThread->_file));
         return 0;
      }
   }
   else {
      _freeChunks[ ( uint64_t ) address ] = size;
   }
  _remaining += size;

   return size;
}

void SimpleAllocator::printMap( std::ostream &o )
{
   std::size_t totalAlloc = 0, totalFree = 0;
   o << (void *) this <<" ALLOCATED CHUNKS" << std::endl;
   for (const auto &map_pair : _allocatedChunks)
   {
      o << "|... ";
      o << (void *) map_pair.first << " @ " << (std::size_t)map_pair.second;
      o << " ...";
      totalAlloc += map_pair.second;
   }
   o << "| total allocated bytes " << (std::size_t) totalAlloc << std::endl;

   o << (void *) this <<" FREE CHUNKS" << std::endl;
   for (const auto &map_pair : _freeChunks) {
      o << "|... ";
      o << (void *) map_pair.first << " @ " << (std::size_t) map_pair.second;
      o << " ...";
      totalFree += map_pair.second;
   }
   o << "| total free bytes "<< (std::size_t) totalFree << std::endl;
}

void SimpleAllocator::lock() 
{
   _lock.lock();
}

void SimpleAllocator::unlock() 
{
   _lock.unlock();
}

uint64_t SimpleAllocator::getBasePointer( uint64_t address, size_t size )
{
   //This is likely an error
   if (_allocatedChunks.size()==0) return 0;
   
   std::map<uint64_t, std::size_t>::iterator it = _allocatedChunks.lower_bound( address );

   // Perfect match, check size
   if ( it->first == address ) {
      if ( it->second >= size ) return it->first;
   }

   // address is lower than any other pinned address
   if ( it == _allocatedChunks.begin() ) return 0;

   // It is an intermediate region, check it fits into a pinned area
   it--;

   if ( ( it->first < address ) && ( ( ( size_t ) it->first + it->second ) >= ( ( size_t ) address + size ) ) ){
       return it->first;
   }
   
   return 0;
}

void SimpleAllocator::canAllocate( std::size_t *sizes, unsigned int numChunks, std::size_t *remainingSizes ) const {
   bool *allocated = (bool *) alloca( numChunks * sizeof(bool) );
   unsigned int allocated_chunks = 0;
   for ( unsigned int idx = 0; idx < numChunks; idx += 1 ) {
      allocated[ idx ] = false;
   }

   for(const auto &map_pair : _freeChunks)
   {
      std::size_t thisSize = map_pair.second;
      for ( unsigned int idx = 0; idx < numChunks; idx += 1 ) {
         if ( allocated[ idx ] == false && sizes[ idx ] <= thisSize ) {
            allocated[ idx ] = true;
            thisSize -= sizes[ idx ];
            allocated_chunks += 1;
         }
      }
   }
   if ( allocated_chunks < numChunks ) {
      unsigned int remaining_count = 0;
      for ( unsigned int idx = 0; idx < numChunks; idx += 1 ) {
         if ( allocated[ idx ] == false ) {
            remainingSizes[ remaining_count ] = sizes[ idx ];
            remaining_count += 1;
         }
      }
      if ( remaining_count < numChunks ) {
         remainingSizes[ remaining_count ] = 0;
      }
   } else {
      remainingSizes[ 0 ] = 0;
   }
}

void SimpleAllocator::getFreeChunksList( SimpleAllocator::ChunkList &list ) const 
{
   for(const auto&  map_pair : _freeChunks)
      list.push_back(map_pair);
}

std::size_t SimpleAllocator::getCapacity() const 
{
   return _capacity;
}

BufferManager::BufferManager( void * address, std::size_t size )
{
   init(address,size);
}

void BufferManager::init ( void * address, std::size_t size )
{
   _baseAddress = address;
   _index = 0;
   _size = size;
}

void * BufferManager::allocate ( std::size_t size )
{
   void * address = ( void * ) ( ( uint64_t ) _baseAddress + _index );
   _index = _index + size;
   return  _size >= _index ? address : NULL;
}

void BufferManager::reset ()
{
   _index = 0;
}

