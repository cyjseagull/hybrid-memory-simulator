/*********************************************************************************
* Copyright (c) 2010-2011, 
* Jim Stevens, Paul Tschirhart, Ishwar Singh Bhati, Mu-Tien Chang, Peter Enns, 
* Elliott Cooper-Balis, Paul Rosenfeld, Bruce Jacob
* University of Maryland
* Contact: jims [at] cs [dot] umd [dot] edu
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

#ifndef HYBRIDSYSTEM_CONFIG_H
#define HYBRIDSYSTEM_CONFIG_H

// Temporary prefetch flags.
#define ENABLE_PERFECT_PREFETCHING 0
#define PREFETCH_FILE "traces/prefetch_data.txt"

#define SEQUENTIAL_PREFETCHING_WINDOW 0

// Stream Buffer Setup.
#define ENABLE_STREAM_BUFFER 1
#define ONE_MISS_TABLE_SIZE 10
#define NUM_STREAM_BUFFERS 10
#define STREAM_BUFFER_LENGTH 4
#define DEBUG_STREAM_BUFFER 0
#define DEBUG_STREAM_BUFFER_HIT 0 // This generates a lot of stuff.


// Debug flags.

// Lots of output during cache operations. Goes to stdout.
#define DEBUG_CACHE 0

// Lots of output for debugging logging operations. Goes to debug.log.
#define DEBUG_LOGGER 0		

// Outputs the victim selection process during each eviction. Goes to debug_victim.log.
#define DEBUG_VICTIM 0		

// Outputs the full trace of accesses sent to NVDIMM. Goes to nvdimm_trace.log.
#define DEBUG_NVDIMM_TRACE 0

// Outputs the full trace of accesses received by HybridSim. Goes to full_trace.log.
#define DEBUG_FULL_TRACE 0


// Map the first CACHE_PAGES of the NVDIMM address space.
// This is the initial state of the hybrid memory on boot.
// This will be overridden if ENABLE_RESTORE is on in the ini file, so leaving this
// on all of the time is harmless. Valid options are 0 or 1.
#define PREFILL_CACHE 1

// If PREFILL_CACHE is on, specify whether the initialized pages should be
// clean or dirty. Valid options are 0 or 1.
#define PREFILL_CACHE_DIRTY 0


// In Intel processors, the 3.5-4.0 GB range of addresses is reserved for MMIO. If the
// input stream is not filtered and remapped, then memory accesses that should not be
// simulated will come into HybridSim. This option drops all accesses in that range
// and subtracts 0.5 GB from all addresses above 4.0 GB.
// With MARSSx86 or traces generated from MARSSx86, this option should be on.
#define REMAP_MMIO 1

// Constants for REMAP_MMIO
const uint64_t HALFGB = 536870912; // 1024^3 / 2
const uint64_t THREEPOINTFIVEGB = 3758096384; // 1024^3 * 3.5
const uint64_t FOURGB = 4294967296; // 1024^3 * 4


// SINGLE_WORD only sends one transaction to the memories per page instead of PAGE_SIZE/BURST_SIZE
// This is an old feature that may not work.
#define SINGLE_WORD 0

// OVERRIDE_DRAM_SIZE is used to make the dram_size parameter passed to DRAM different than the computed
// size needed by HybridSim (CACHE_PAGES * PAGE_SIZE) >> 20. 
// If it is 0, it is ignored.
#define OVERRIDE_DRAM_SIZE 0


// RESTORE_CLEAN is used to simulate a checkpoint of the memory system upon restoring the state.
// Essentially, all pages in the DRAM cache are reverted to clean.
#define RESTORE_CLEAN 0


// TLB parameters

// All size parameters in bytes (keep to powers of 2)
// Setting TLB_SIZE to 0 disables TLB simulation.
#define TAG_SIZE 2
//#define TLB_SIZE 16384
#define TLB_SIZE 0 

// TLB miss delay is in memory clock cycles
#define TLB_MISS_DELAY 30


// C standard library and C++ STL includes.
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <ctime>
#include <stdint.h>
#include <vector>
#include <utility>
#include <assert.h>

// Include external interface for DRAMSim.
#include <DRAMSim.h>

// Additional things I reuse from DRAMSim repo (for internal use only).
//#include <Transaction.h>
#include <SimulatorObject.h>
using DRAMSim::SimulatorObject;
//using DRAMSim::TransactionType;
//using DRAMSim::DATA_READ;
//using DRAMSim::DATA_WRITE;

// Include external interface for NVDIMM.
#include <NVDIMMSim.h>


// Include the Transaction type (which is needed below).
#include "Transaction.h"

// Declare error printout (used to be brought in from DRAMSim).
#define ERROR(str) std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;


using namespace std;

namespace HybridSim
{



// Declare externs for Ini file settings.

extern uint64_t CONTROLLER_DELAY;

extern uint64_t ENABLE_LOGGER;
extern uint64_t EPOCH_LENGTH;
extern uint64_t HISTOGRAM_BIN;
extern uint64_t HISTOGRAM_MAX;

extern uint64_t PAGE_SIZE; // in bytes, so divide this by 64 to get the number of DDR3 transfers per page
extern uint64_t SET_SIZE; // associativity of cache
extern uint64_t BURST_SIZE; // number of bytes in a single transaction, this means with PAGE_SIZE=1024, 16 transactions are needed
extern uint64_t FLASH_BURST_SIZE; // number of bytes in a single flash transaction

// Number of pages total and number of pages in the cache
extern uint64_t TOTAL_PAGES; // 2 GB
extern uint64_t CACHE_PAGES; // 1 GB


// Defined in marss memoryHierachy.cpp.
// Need to confirm this and make it more flexible later.
extern uint64_t CYCLES_PER_SECOND;

// INI files
extern string dram_ini;
extern string flash_ini;
extern string sys_ini;

// Save/Restore options
extern uint64_t ENABLE_RESTORE;
extern uint64_t ENABLE_SAVE;
extern string HYBRIDSIM_RESTORE_FILE;
extern string NVDIMM_RESTORE_FILE;
extern string HYBRIDSIM_SAVE_FILE;
extern string NVDIMM_SAVE_FILE;





// Macros derived from Ini settings.

#define NUM_SETS (CACHE_PAGES / SET_SIZE)
#define PAGE_NUMBER(addr) (addr / PAGE_SIZE)
#define PAGE_ADDRESS(addr) ((addr / PAGE_SIZE) * PAGE_SIZE)
#define PAGE_OFFSET(addr) (addr % PAGE_SIZE)
#define SET_INDEX(addr) (PAGE_NUMBER(addr) % NUM_SETS)
#define TAG(addr) (PAGE_NUMBER(addr) / NUM_SETS)
#define FLASH_ADDRESS(tag, set) ((tag * NUM_SETS + set) * PAGE_SIZE)
#define ALIGN(addr) (((addr / BURST_SIZE) * BURST_SIZE) % (TOTAL_PAGES * PAGE_SIZE))

// TLB derived parameters
#define BYTES_PER_READ 64
#define TLB_MAX_ENTRIES (TLB_SIZE / BYTES_PER_READ)
#define TAGS_PER_ENTRY (BYTES_PER_READ / TAG_SIZE)
#define TLB_ENTRY_SPAN (PAGE_SIZE * TAGS_PER_ENTRY)
#define TLB_BASE_ADDRESS(addr) ((addr * TLB_ENTRY_SPAN) / TLB_ENTRY_SPAN)

// Declare the cache_line class, which is the table entry used for each line in the cache tag store.
class cache_line
{
        public:
        bool valid;
        bool dirty;
		bool locked;
		uint64_t lock_count;
        uint64_t tag;
        uint64_t data;
        uint64_t ts;
		bool prefetched; // Set to 1 if a cache_line is brought into DRAM as a prefetch.
		bool used; // Like dirty, but also set to 1 for reads. Used for tracking prefetch hits vs. misses.

        cache_line() : valid(false), dirty(false), locked(false), lock_count(0), tag(0), data(0), ts(0), prefetched(0), used(0) {}
        string str() 
		{ 
			stringstream out; 
			out << "tag=" << tag << " data=" << data << " valid=" << valid << " dirty=" << dirty << " locked=" << locked 
				<< " lock_count=" << lock_count << " ts=" << ts << " prefetched=" << prefetched << " used=" << used;
			return out.str(); 
		}
};

enum PendingOperation
{
	VICTIM_READ, // Read victim line from DRAM
	VICTIM_WRITE, // Write victim line to Flash
	LINE_READ, // Read new line from Flash
	CACHE_READ, // Perform a DRAM read and return the final result.
	CACHE_WRITE // Perform a DRAM read and return the final result.
};

// Entries in the pending table.
class Pending
{
	public:
	PendingOperation op; // What operation is being performed.
	uint64_t orig_addr;
	uint64_t flash_addr;
	uint64_t cache_addr;
	uint64_t victim_tag;
	bool victim_valid;
	bool callback_sent;
	TransactionType type; // DATA_READ or DATA_WRITE

	Pending() : op(VICTIM_READ), flash_addr(0), cache_addr(0), victim_tag(0), type(DATA_READ) {};
        string str() { stringstream out; out << "O=" << op << " F=" << flash_addr << " C=" << cache_addr << " V=" << victim_tag 
		<< " T=" << type; return out.str(); }
};

} // namespace HybridSim

#endif
