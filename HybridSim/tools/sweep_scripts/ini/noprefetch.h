#ifndef HYBRIDSYSTEM_CONFIG_H
#define HYBRIDSYSTEM_CONFIG_H

// Temporary prefetch flags.
#define ENABLE_PREFETCHING 0
#define PREFETCH_FILE "traces/prefetch_data.txt"

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


// SINGLE_WORD only sends one transaction to the memories per page instead of PAGE_SIZE/BURST_SIZE
// This is an old feature that may not work.
#define SINGLE_WORD 0


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


// Declare the cache_line class, which is the table entry used for each line in the cache tag store.
class cache_line
{
        public:
        bool valid;
        bool dirty;
        uint64_t tag;
        uint64_t data;
        uint64_t ts;

        cache_line() : valid(false), dirty(false), tag(0), data(0), ts(0) {}
        string str() { stringstream out; out << "tag=" << tag << " data=" << data << " valid=" << valid << " dirty=" << dirty << " ts=" << ts; return out.str(); }

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
	bool callback_sent;
	TransactionType type; // DATA_READ or DATA_WRITE
	unordered_set<uint64_t> *wait;

	Pending() : op(VICTIM_READ), flash_addr(0), cache_addr(0), victim_tag(0), type(DATA_READ), wait(0) {};
        string str() { stringstream out; out << "O=" << op << " F=" << flash_addr << " C=" << cache_addr << " V=" << victim_tag 
		<< " T=" << type; return out.str(); }

	void init_wait()
	{
		wait = new unordered_set<uint64_t>;

	}

	void insert_wait(uint64_t n)
	{
		wait->insert(n);
	}

	void delete_wait()
	{
		delete wait;
		wait = NULL;
	}
};

} // namespace HybridSim

#endif
