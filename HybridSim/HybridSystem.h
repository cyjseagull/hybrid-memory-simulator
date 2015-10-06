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

#ifndef HYBRIDSIM_HYBRIDSYSTEM_H
#define HYBRIDSIM_HYBRIDSYSTEM_H

#include <iostream>
#include <fstream>
#include <string>

#include "config.h"
#include "util.h"
#include "CallbackHybrid.h"
#include "Logger.h"
#include "IniReader.h"

using std::string;
typedef unsigned int uint;

namespace HybridSim
{
	class HybridSystem: public SimulatorObject
	{
		public:
		HybridSystem(uint id, string ini);
		~HybridSystem();
		void update();
		bool addTransaction(bool isWrite, uint64_t addr);
		bool addTransaction(Transaction &trans);
		void addPrefetch(uint64_t prefetch_addr);
		void addFlush(uint64_t flush_addr);
		bool WillAcceptTransaction();
		void RegisterCallbacks(
				TransactionCompleteCB *readDone,
				TransactionCompleteCB *writeDone);
		void mmio(uint64_t operation, uint64_t address);
		void syncAll();
		void DRAMReadCallback(uint id, uint64_t addr, uint64_t cycle);
		void DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle);
		void DRAMPowerCallback(double a, double b, double c, double d);
		void FlashReadCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped);
		void FlashCriticalLineCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped);
		void FlashWriteCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped);

		// Functions to run the callbacks to the module using HybridSim.
		void ReadDoneCallback(uint systemID, uint64_t orig_addr, uint64_t cycle);
		void WriteDoneCallback(uint sysID, uint64_t orig_addr, uint64_t cycle);

		void reportPower();
		string SetOutputFileName(string tracefilename);

		// Print out the logging data for HybridSim only.
		void printLogfile();

		// Save/Restore cache table functions
		void restoreCacheTable();
		void saveCacheTable();


		// Helper functions
		void ProcessTransaction(Transaction &trans);

		void VictimRead(Pending p);
		void VictimReadFinish(uint64_t addr, Pending p);

		void VictimWrite(Pending p);

		void LineRead(Pending p);
		void LineReadFinish(uint64_t addr, Pending p);

		void LineWrite(Pending p);

		void CacheRead(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);
		void CacheReadFinish(uint64_t addr, Pending p);

		void CacheWrite(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr);
		void CacheWriteFinish(Pending p);

		void Flush(uint64_t cache_addr);


		// Page Contention Functions
		void contention_lock(uint64_t flash_addr);
		void contention_page_lock(uint64_t flash_addr);
		void contention_unlock(uint64_t flash_addr, uint64_t orig_addr, string operation, bool victim_valid, uint64_t victim_page, 
				bool cache_line_valid, uint64_t cache_addr);
		bool contention_is_unlocked(uint64_t flash_addr);
		void contention_increment(uint64_t flash_addr);
		void contention_decrement(uint64_t flash_addr);
		void contention_victim_lock(uint64_t page_addr);
		void contention_victim_unlock(uint64_t page_addr);
		void contention_cache_line_lock(uint64_t cache_addr);
		void contention_cache_line_unlock(uint64_t cache_addr);


		// Prefetch Functions
		void issue_sequential_prefetches(uint64_t page_addr);

		// Sync functions
		void sync(uint64_t addr, uint64_t cache_address, Transaction trans);
		void syncAllCounter(uint64_t addr, Transaction trans);
		void addSync(uint64_t addr);
		void addSyncCounter(uint64_t addr, bool initial);

		// TLB functions
		void check_tlb(uint64_t page_addr);

		// Stream Buffer Functions
		void stream_buffer_miss_handler(uint64_t miss_page);
		void stream_buffer_hit_handler(uint64_t hit_page);
		

		// State
		string hybridsim_ini;
		IniReader iniReader;

		TransactionCompleteCB *ReadDone;
		TransactionCompleteCB *WriteDone;
		uint systemID;
		
		//action with DRAMSim2 and NVDIMMSim
		DRAMSim::MultiChannelMemorySystem *dram;

		NVDSim::NVDIMM *flash;

		unordered_map<uint64_t, cache_line> cache;

		unordered_map<uint64_t, Pending> dram_pending;
		unordered_map<uint64_t, Pending> flash_pending;

		// Per page wait sets for the VICTIM_READ and LINE_READ operations.
		unordered_map<uint64_t, unordered_set<uint64_t>> dram_pending_wait;
		unordered_map<uint64_t, unordered_set<uint64_t>> flash_pending_wait;

		
		unordered_map<uint64_t, uint64_t> pending_flash_addr; // If a page is in the pending_flash_addr , then skip subsequent transactions to the flash address.
		unordered_map<uint64_t, uint64_t> pending_pages; // If a page is in the pending_pages, then skip subsequent transactions to the page.
		unordered_map<uint64_t, uint64_t> set_counter; // Counts the number of outstanding transactions to each set.

		bool check_queue; // If there is nothing to do, don't check the queue until the next event occurs that will make new work.

		uint64_t delay_counter; // Used to stall the controller while it is "doing work".
		Transaction active_transaction; // Used to hold the transaction waiting for SRAM.
		bool active_transaction_flag; // Indicates that a transaction is waiting for SRAM.

		int64_t pending_count;
		set<uint64_t> dram_pending_set;
		list<uint64_t> dram_bad_address;
		uint64_t max_dram_pending;
		uint64_t pending_pages_max;
		uint64_t trans_queue_max;
		uint64_t trans_queue_size;

		list<Transaction> trans_queue; // Entry queue for the cache controller.
		list<Transaction> dram_queue; // Buffer to wait for DRAM
		list<Transaction> flash_queue; // Buffer to wait for Flash

		// Logger is used to store HybridSim-specific logging events.
		Logger log;

		// Prefetch data stores the prefetch sets from the prefetch file.
		// This is stored as a map of lists. It could be stored more compactly as an array of pointers to pointers,
		// but I chose not to since random access is not needed and this makes the code simpler.
		// If space becomes a problem, I'm just going to switch this to loading the data from a file per set at runtime.
		unordered_map<uint64_t, list<uint64_t>> prefetch_access_number;
		unordered_map<uint64_t, list<uint64_t>> prefetch_flush_addr;
		unordered_map<uint64_t, list<uint64_t>> prefetch_new_addr;
		unordered_map<uint64_t, uint64_t> prefetch_counter;

		ofstream debug_victim;
		ofstream debug_nvdimm_trace;
		ofstream debug_full_trace;

		// TLB state
		unordered_map<uint64_t, uint64_t> tlb_base_set; 
		uint64_t tlb_misses;
		uint64_t tlb_hits;

		// Prefetch tracking.
		uint64_t total_prefetches;
		uint64_t unused_prefetches; // Count of unused prefetched pages in the DRAM cache.
		uint64_t unused_prefetch_victims; // Count of unused prefetched pages that were never used before being evicted.
		uint64_t prefetch_hit_nops; // Count the number of prefetch hits that are nops.
		uint64_t prefetch_cheat_count; // Count the number of cheat prefetches.

		// Stream buffer state.
		list<pair<uint64_t, uint64_t> > one_miss_table; // pair is (address, cycle)
		unordered_map<uint64_t, uint64_t> stream_buffers; // address -> cycle

		// Stream buffer tracking.
		uint64_t unique_one_misses;
		uint64_t unique_stream_buffers;
		uint64_t stream_buffer_hits;


		unordered_map<uint64_t, uint64_t> prefetch_cheat_map;

	};

	HybridSystem *getMemorySystemInstance(uint id, string ini);

}

#endif
