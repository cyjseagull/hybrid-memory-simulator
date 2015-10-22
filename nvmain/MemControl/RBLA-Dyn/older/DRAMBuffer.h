/*******************************************
 *
 *
 *
 *
 * ******************************************/
#ifndef _DRAM_BUFFER_H_
#define _DRAM_BUFFER_H_

#include "include/NVMainRequest.h"
#include "MemControl/DRAMCache/AbstractDRAMCache.h"
#include "NVM/nvmain.h"
#include <list>
#include <iterator>


namespace NVM
{
	enum
	{
			INVLID = 0,
			VALID = 1,
			DIRTY = 2
	};

	enum DRAMCacheState
	{
			IDLE,
			BUSY
	};

	enum DRAMCacheOp
	{
			DRAMCache_none,
			DRAMCache_read,
			DRAMCache_write,
			DRAMCache_evict
	};


	typedef struct RAMCacheRequest
	{
		DRAMCacheOp optype;
	 	NVMAddress address;
	  	NVMAddress endAddr;
	   	NVMDataBlock data;
	    bool hit;
		NVMObject *owner;
		NVMainRequest *originalRequest;
	};

	//cache block stored in DRAM Buffer
	typedef struct DRAMCacheBlock
	{
		uint64_t flags;
		NVMAddress* addr;
		NVMDataBlock *data;
	};

	class DRAMBuffer
	{
		public:
			DRAMBuffer( uint64_t sets, uint64_t assoc , uint64_t cacheLineSize);
			virtual ~DRAMBuffer();
			void SetConfig( Config* config , bool createChildren = true );
			

			bool IsPreSent(NVMAddress &addr);
			bool Read( NVMAddress &addr);

			bool Write( NVMAddress &addr);
			
			bool Install();

		private:
			std::list<DRAMCacheBlock*> dram_cache;
			
			

	}

}


#endif
