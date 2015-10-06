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

#include "HybridSystem.h"

using namespace std;

namespace HybridSim {

	HybridSystem::HybridSystem(uint id, string ini)
	{
		//set configuration file path
		if (ini == "")
		{
			hybridsim_ini = "";
			//get environment value(HYBRIDSIM_BASE) content
			char *base_path = getenv("HYBRIDSIM_BASE");
			if (base_path != NULL)
			{
				hybridsim_ini.append(base_path);
				hybridsim_ini.append("/");
			}
			//default configuration file
			hybridsim_ini.append("../HybridSim/ini/hybridsim.ini");
		}
		else
		{
			hybridsim_ini = ini;
		}

		string pattern = "/ini/";
		string inipathPrefix;

		unsigned found = hybridsim_ini.rfind(pattern); // Find the last occurrence of "/ini/"
		assert (found != string::npos);
		inipathPrefix = hybridsim_ini.substr(0,found);
		inipathPrefix.append("/");
		//read hybridsim file
		iniReader.read(hybridsim_ini);
		if (ENABLE_LOGGER)
			log.init();

		// Make sure that there are more cache pages than pages per set. 
		assert(CACHE_PAGES >= SET_SIZE);

		systemID = id;
		cerr << "Creating DRAM with " << dram_ini << "\n";
		uint64_t dram_size = (CACHE_PAGES * PAGE_SIZE) >> 20;
		dram_size = (dram_size == 0) ? 1 : dram_size; // DRAMSim requires a minimum of 1 MB, even if HybridSim isn't going to use it.
		dram_size = (OVERRIDE_DRAM_SIZE == 0) ? dram_size : OVERRIDE_DRAM_SIZE; // If OVERRIDE_DRAM_SIZE is non-zero, then use it.
		dram = DRAMSim::getMemorySystemInstance(dram_ini, sys_ini, inipathPrefix, "resultsfilename", dram_size);

		cerr << "Creating Flash with " << flash_ini << "\n";
		flash = NVDSim::getNVDIMMInstance(1,flash_ini,"ini/def_system.ini",inipathPrefix,"");
		cerr << "Done with creating memories" << endl;

		// Set up the callbacks for DRAM.
		typedef DRAMSim::Callback <HybridSystem, void, uint, uint64_t, uint64_t> dramsim_callback_t;
		DRAMSim::TransactionCompleteCB *read_cb = new dramsim_callback_t(this, &HybridSystem::DRAMReadCallback);
		DRAMSim::TransactionCompleteCB *write_cb = new dramsim_callback_t(this, &HybridSystem::DRAMWriteCallback);
		dram->RegisterCallbacks(read_cb, write_cb, NULL);

		// Set up the callbacks for NVDIMM.
		typedef NVDSim::Callback <HybridSystem, void, uint64_t, uint64_t, uint64_t, bool> nvdsim_callback_t;
		NVDSim::Callback_t *nv_read_cb = new nvdsim_callback_t(this, &HybridSystem::FlashReadCallback);
		NVDSim::Callback_t *nv_write_cb = new nvdsim_callback_t(this, &HybridSystem::FlashWriteCallback);
		NVDSim::Callback_t *nv_crit_cb = new nvdsim_callback_t(this, &HybridSystem::FlashCriticalLineCallback);
		flash->RegisterCallbacks(nv_read_cb, nv_crit_cb, nv_write_cb, NULL);

		// Need to check the queue when we start.
		check_queue = true;

		// No delay to start with.
		delay_counter = 0;

		// No active transaction to start with.
		active_transaction_flag = false;

		// Call the restore cache state function.
		// If ENABLE_RESTORE is set, then this will fill the cache table.
		restoreCacheTable();

		// Load prefetch data.
		if (ENABLE_PERFECT_PREFETCHING)
		{
			// MOVE THIS TO A FUNCTION.

			// Open prefetch file.
			ifstream prefetch_file;
			prefetch_file.open(PREFETCH_FILE, ifstream::in);
			if (!prefetch_file.is_open())
			{
				cerr << "ERROR: Failed to load prefetch file: " << PREFETCH_FILE << "\n";
				abort();
			}

			// Declare variables for parsing string and uint64_t.
			string parse_string;
			uint64_t num_sets, cur_set, set_size, set_counter, tmp_num;

			// Parse prefetch data.
			prefetch_file >> parse_string;
			if (parse_string != "NUM_SETS")
			{
				cerr << "ERROR: Invalid prefetch file format. NUM_SETS does not appear at beginning.\n";
				abort();
			}
			
			prefetch_file >> num_sets;

			for (cur_set = 0; cur_set < num_sets; cur_set++)
			{
				prefetch_file >> parse_string;
				if (parse_string != "SET")
				{
					cerr << "ERROR: Invalid prefetch file format. SET does not appear at beginning of set " << cur_set << ".\n";
					abort();
				}

				prefetch_file >> tmp_num;
				if (tmp_num != cur_set)
				{
					cerr << "ERROR: Invalid prefetch file format. Sets not given in order. (" << cur_set << ")\n";
					abort();
				}
				
				// Read the size fo this set.
				prefetch_file >> set_size;

				// Create a new entry in the maps.
				prefetch_access_number[cur_set] = list<uint64_t>();
				prefetch_flush_addr[cur_set] = list<uint64_t>();
				prefetch_new_addr[cur_set] = list<uint64_t>();
				prefetch_counter[cur_set] = 0;

				// Process each prefetch.
				for (set_counter = 0; set_counter < set_size; set_counter++)
				{
					// Read and store the access counter.
					prefetch_file >> tmp_num;
					prefetch_access_number[cur_set].push_back(tmp_num);

					// Read and store the flush address.
					prefetch_file >> tmp_num;
					prefetch_flush_addr[cur_set].push_back(tmp_num);

					// Read and store the new address.
					prefetch_file >> tmp_num;
					prefetch_new_addr[cur_set].push_back(tmp_num);
				}
			}

			prefetch_file.close();
		}

		// Initialize size/max counters.
		// Note: Some of this is just debug info, but I'm keeping it around because it is useful.
		pending_count = 0; // This is used by TraceBasedSim for MAX_PENDING.
		max_dram_pending = 0;
		pending_pages_max = 0;
		trans_queue_max = 0;
		trans_queue_size = 0; // This is not debugging info.

		tlb_misses = 0;
		tlb_hits = 0;

		total_prefetches = 0;
		unused_prefetches = 0;
		unused_prefetch_victims = 0;
		prefetch_hit_nops = 0;
		prefetch_cheat_count = 0;

		unique_one_misses = 0;
		unique_stream_buffers = 0;
		stream_buffer_hits = 0;

		// Create file descriptors for debugging output (if needed).
		if (DEBUG_VICTIM) 
		{
			debug_victim.open("debug_victim.log", ios_base::out | ios_base::trunc);
			if (!debug_victim.is_open())
			{
				cerr << "ERROR: HybridSim debug_victim file failed to open.\n";
				abort();
			}
		}

		if (DEBUG_NVDIMM_TRACE) 
		{
			debug_nvdimm_trace.open("nvdimm_trace.log", ios_base::out | ios_base::trunc);
			if (!debug_nvdimm_trace.is_open())
			{
				cerr << "ERROR: HybridSim debug_nvdimm_trace file failed to open.\n";
				abort();
			}
		}

		if (DEBUG_FULL_TRACE) 
		{
			debug_full_trace.open("full_trace.log", ios_base::out | ios_base::trunc);
			if (!debug_full_trace.is_open())
			{
				cerr << "ERROR: HybridSim debug_full_trace file failed to open.\n";
				abort();
			}
		}
	}

	HybridSystem::~HybridSystem()
	{
		if (DEBUG_VICTIM)
			debug_victim.close();

		if (DEBUG_NVDIMM_TRACE)
			debug_nvdimm_trace.close();

		if (DEBUG_FULL_TRACE)
			debug_full_trace.close();
	}

	// static allocator for the library interface
	HybridSystem *getMemorySystemInstance(uint id, string ini)
	{
		return new HybridSystem(id, ini);
	}


	void HybridSystem::update()
	{
		// Process the transaction queue.
		// This will fill the dram_queue and flash_queue.

		if (dram_pending.size() > max_dram_pending)
			max_dram_pending = dram_pending.size();
		if (pending_pages.size() > pending_pages_max)
			pending_pages_max = pending_pages.size();
		if (trans_queue_size > trans_queue_max)
			trans_queue_max = trans_queue_size;

		// Log the queue length.
		bool idle = (trans_queue.empty()) && (pending_pages.empty());
		bool flash_idle = (flash_queue.empty()) && (flash_pending.empty());
		bool dram_idle = (dram_queue.empty()) && (dram_pending.empty());
		if (ENABLE_LOGGER)
			log.access_update(trans_queue_size, idle, flash_idle, dram_idle);


		// See if there are any transactions ready to be processed.
		if ((active_transaction_flag) && (delay_counter == 0))
		{
				ProcessTransaction(active_transaction);
				active_transaction_flag = false;
		}
		


		// Used to see if any work is done on this cycle.
		bool sent_transaction = false;


		list<Transaction>::iterator it = trans_queue.begin();
		while((it != trans_queue.end()) && (pending_pages.size() < NUM_SETS) && (check_queue) && (delay_counter == 0))
		{
			// Compute the page address.
			uint64_t flash_addr = ALIGN((*it).address);
			uint64_t page_addr = PAGE_ADDRESS(flash_addr);


			// Check to see if this page is open under contention rules.
			if (contention_is_unlocked(flash_addr))
			{
				// Lock the page.
				contention_lock(flash_addr);

				// Log the page access.
				if (ENABLE_LOGGER)
					log.access_page(page_addr);

				// Set this transaction as active and start the delay counter, which
				// simulates the SRAM cache tag lookup time.
				active_transaction = *it;
				active_transaction_flag = true;
				delay_counter = CONTROLLER_DELAY;
				sent_transaction = true;

				// Check that this page is in the TLB.
				// Do not do this for SYNC_ALL_COUNTER transactions because the page address refers
				// to the cache line, not the flash page address, so the TLB isn't needed.
				if ((*it).transactionType != SYNC_ALL_COUNTER)
					check_tlb(page_addr);

				// Delete this item and skip to the next.
				it = trans_queue.erase(it);
				trans_queue_size--;


				if ((active_transaction.transactionType == PREFETCH) && 
					(prefetch_cheat_map.count(PAGE_ADDRESS(ALIGN(active_transaction.address)))))
				{
					// If this is a cheat prefetch, then just do it now and continue processing.
					// Cheat prefetches are effectively "free" in terms of clock cycles.
					//cout << "Issuing cheat prefetch to address " << PAGE_ADDRESS(ALIGN(active_transaction.address))
					//	<< " on cycle " << currentClockCycle << "\n";
					ProcessTransaction(active_transaction);
					active_transaction_flag = false;
					delay_counter = 0;
					sent_transaction = false;
					continue;
				}
				else
				{
					// We've found the transaction we want.
					break;
				}
			}
			else
			{
				// Log the set conflict.
				if (ENABLE_LOGGER)
					log.access_set_conflict(SET_INDEX(page_addr));

				// Skip to the next and do nothing else.
				++it;
			}
		}

		// If there is nothing to do, wait until a new transaction arrives or a pending set is released.
		// Only set check_queue to false if the delay counter is 0. Otherwise, a transaction that arrives
		// while delay_counter is running might get missed and stuck in the queue.
		if ((sent_transaction == false) && (delay_counter == 0))
		{
			this->check_queue = false;
		}


		// Process DRAM transaction queue until it is empty or addTransaction returns false.
		// Note: This used to be a while, but was changed ot an if to only allow one
		// transaction to be sent to the DRAM per cycle.
		bool not_full = true;
		if (not_full && !dram_queue.empty())
		{
			Transaction tmp = dram_queue.front();
			bool isWrite;
			if (tmp.transactionType == DATA_WRITE)
				isWrite = true;
			else
				isWrite = false;
			not_full = dram->addTransaction(isWrite, tmp.address);
			if (not_full)
			{
				dram_queue.pop_front();
				dram_pending_set.insert(tmp.address);
			}
		}

		// Process Flash transaction queue until it is empty or addTransaction returns false.
		// Note: This used to be a while, but was changed ot an if to only allow one
		// transaction to be sent to the flash per cycle.
		not_full = true;
		if (not_full && !flash_queue.empty())
		{
			bool isWrite;

			Transaction tmp = flash_queue.front();
			if (tmp.transactionType == DATA_WRITE)
				isWrite = true;
			else
				isWrite = false;
			not_full = flash->addTransaction(isWrite, tmp.address);

			if (not_full)
			{
				flash_queue.pop_front();

				if (DEBUG_NVDIMM_TRACE)
				{
					debug_nvdimm_trace << currentClockCycle << " " << (isWrite ? 1 : 0) << " " << tmp.address << "\n";
					debug_nvdimm_trace.flush();
				}
			}
		}

		// Decrement the delay counter.
		if (delay_counter > 0)
		{
			delay_counter--;
		}


		// Update the logger.
		if (ENABLE_LOGGER)
			log.update();

		// Update the memories.
		dram->update();
		flash->update();

		// Increment the cycle count.
		step();
	}

	bool HybridSystem::addTransaction(bool isWrite, uint64_t addr)
	{
		if (DEBUG_CACHE)
			cerr << "\n" << currentClockCycle << ": " << "Adding transaction for address=" << addr << " isWrite=" << isWrite << endl;

		TransactionType type;
		if (isWrite)
		{
			type = DATA_WRITE;
		}
		else
		{
			type = DATA_READ;
		}
		Transaction t = Transaction(type, addr, NULL);
		return addTransaction(t);
	}

	bool HybridSystem::addTransaction(Transaction &trans)
	{

		if (REMAP_MMIO)
		{
			if ((trans.address >= THREEPOINTFIVEGB) && (trans.address < FOURGB))
			{
				// Do not add this transaction to the queue because it is in the MMIO range.
				// Just issue the callback and return.
				if (trans.transactionType == DATA_READ)
				{
					if (ReadDone != NULL)
						(*ReadDone)(systemID, trans.address, currentClockCycle);
				}
				else if (trans.transactionType == DATA_WRITE)
				{
					if (WriteDone != NULL)
						(*WriteDone)(systemID, trans.address, currentClockCycle);
				}
				else
					assert(0);

				log.mmio_dropped();

				return true;
			}
			else if (trans.address >= FOURGB)
			{
				// Subtract 0.5 GB from the address to adjust for MMIO.
				trans.address -= HALFGB;

				log.mmio_remapped();
			}
		}

		pending_count += 1;

		trans_queue.push_back(trans);
		trans_queue_size++;

		if ((trans.transactionType == PREFETCH) || (trans.transactionType == FLUSH))
		{
			ERROR("PREFETCH/FLUSH not allowed in addTransaction()");
			abort();
		}

		// Start the logging for this access.
		if (ENABLE_LOGGER)
			log.access_start(trans.address);

		if (DEBUG_FULL_TRACE)
		{
			debug_full_trace << currentClockCycle << " " << ((trans.transactionType == DATA_WRITE) ? 1 : 0) << " " << trans.address << "\n";
			debug_full_trace.flush();
		}

		// Restart queue checking.
		this->check_queue = true;

		return true; // TODO: Figure out when this could be false.
	}

	void HybridSystem::addPrefetch(uint64_t prefetch_addr)
	{
		// Create prefetch transaction.
		Transaction prefetch_transaction = Transaction(PREFETCH, prefetch_addr, NULL);

		// Push the operation onto the front of the transaction queue (so it executes immediately).
		trans_queue.push_front(prefetch_transaction);
		trans_queue_size += 1;

		pending_count += 1;

		// Restart queue checking.
		this->check_queue = true;
	}

	void HybridSystem::addFlush(uint64_t flush_addr)
	{
		// Create flush transaction.
		Transaction flush_transaction = Transaction(FLUSH, flush_addr, NULL);

		// Push the operation onto the front of the transaction queue (so it executes immediately).
		trans_queue.push_front(flush_transaction);
		trans_queue_size += 1;

		pending_count += 1;

		// Restart queue checking.
		this->check_queue = true;
	}

	bool HybridSystem::WillAcceptTransaction()
	{
		// Always true for now since MARSS expects this.
		// Might change later.
		return true;
	}

	void HybridSystem::ProcessTransaction(Transaction &trans)
	{
		// trans.address is the original address that we must use to callback.
		// But for our processing, we must use an aligned address (which is aligned to a page in the NV address space).
		uint64_t addr = ALIGN(trans.address);


		if (trans.transactionType == SYNC_ALL_COUNTER)
		{
			// SYNC_ALL_COUNTER transactions are handled elsewhere.
			syncAllCounter(addr, trans);
			return;
		}


		if (DEBUG_CACHE)
			cerr << "\n" << currentClockCycle << ": " << "Starting transaction for address " << addr << endl;


		if (addr >= (TOTAL_PAGES * PAGE_SIZE))
		{
			// Note: This should be technically impossible due to the modulo in ALIGN. But this is just a sanity check.
			cerr << "ERROR: Address out of bounds - orig:" << trans.address << " aligned:" << addr << "\n";
			abort();
		}

		// Compute the set number and tag
		uint64_t set_index = SET_INDEX(addr);
		uint64_t tag = TAG(addr);

		list<uint64_t> set_address_list;
		for (uint64_t i=0; i<SET_SIZE; i++)
		{
			uint64_t next_address = (i * NUM_SETS + set_index) * PAGE_SIZE;
			set_address_list.push_back(next_address);
		}

		bool hit = false;
		uint64_t cache_address = *(set_address_list.begin());
		uint64_t cur_address;
		cache_line cur_line;
		for (list<uint64_t>::iterator it = set_address_list.begin(); it != set_address_list.end(); ++it)
		{
			cur_address = *it;
			if (cache.count(cur_address) == 0)
			{
				// If i is not allocated yet, allocate it.
				cache[cur_address] = cache_line();
			}

			cur_line = cache[cur_address];

			if (cur_line.valid && (cur_line.tag == tag))
			{
				hit = true;
				cache_address = cur_address;

				if (DEBUG_CACHE)
				{
					cerr << currentClockCycle << ": " << "HIT: " << cur_address << " " << " " << cur_line.str() << 
						" (set: " << set_index << ")" << endl;
				}

				break;
			}

		}

		// Place access_process here and combine it with access_cache.
		// Tell the logger when the access is processed (used for timing the time in queue).
		// Only do this for DATA_READ and DATA_WRITE.
		if ((ENABLE_LOGGER) && ((trans.transactionType == DATA_READ) || (trans.transactionType == DATA_WRITE)))
			log.access_process(trans.address, trans.transactionType == DATA_READ, hit);

		// Handle prefetching operations.
		if (ENABLE_PERFECT_PREFETCHING && ((trans.transactionType == DATA_READ) || (trans.transactionType == DATA_WRITE)))
		{
			// Increment prefetch counter for this set.
			prefetch_counter[set_index]++;

			// If there are any prefetches left in this set prefetch list.
			if (!prefetch_access_number[set_index].empty())
			{
				// If the counter exceeds the front of the access list, 
				// then issue a prefetch and pop the front of the prefetch lists for this set.
				// This must be a > because the prefetch must only happen AFTER the access.
				if (prefetch_counter[set_index] > prefetch_access_number[set_index].front())
				{

					// Add prefetch, then add flush (this makes flush run first).
					addPrefetch(prefetch_new_addr[set_index].front());
					addFlush(prefetch_flush_addr[set_index].front());

					// Go to the next prefetch in this set.
					prefetch_access_number[set_index].pop_front();
					prefetch_flush_addr[set_index].pop_front();
					prefetch_new_addr[set_index].pop_front();
				}
			}
		}


		if (hit)
		{
			// Lock the line that was hit (so it cannot be selected as a victim while being processed).
			contention_cache_line_lock(cache_address);

			if ((ENABLE_STREAM_BUFFER) && 
					((trans.transactionType == DATA_READ) || (trans.transactionType == DATA_WRITE)))
			{
				stream_buffer_hit_handler(PAGE_ADDRESS(addr));
			}

			// Issue operation to the DRAM.
			if (trans.transactionType == DATA_READ)
				CacheRead(trans.address, addr, cache_address);
			else if(trans.transactionType == DATA_WRITE)
				CacheWrite(trans.address, addr, cache_address);
			else if(trans.transactionType == FLUSH)
			{
				Flush(cache_address);
			}
			else if(trans.transactionType == PREFETCH)
			{
				// Prefetch hits are just NOPs.
				uint64_t flash_address = addr;
				contention_unlock(flash_address, flash_address, "PREFETCH (hit)", false, 0, true, cache_address);

				prefetch_hit_nops++;

				return; 
			}
			else if(trans.transactionType == SYNC)
			{
				sync(addr, cache_address, trans);
				//contention_unlock(addr, addr, "SYNC (hit)", false, 0, true, cache_address);
			}
			else
			{
				assert(0);
			}
		}

		if (!hit)
		{
			// Lock the whole page.
			contention_page_lock(addr);

			// Make sure this isn't a FLUSH before proceeding.
			if(trans.transactionType == FLUSH)
			{
				// We allow FLUSH to miss the cache because of non-determinism from marss.
				uint64_t flash_address = addr;
				contention_unlock(flash_address, flash_address, "FLUSH (miss)", false, 0, false, 0);

				// TODO: Add some logging for this event.

				return;
			}

			assert(trans.transactionType != SYNC);

			if ((SEQUENTIAL_PREFETCHING_WINDOW > 0) && (trans.transactionType != PREFETCH))
			{
				issue_sequential_prefetches(addr);
			}

			if ((ENABLE_STREAM_BUFFER) && (trans.transactionType != PREFETCH))
			{
				stream_buffer_miss_handler(PAGE_ADDRESS(addr));
			}

			// Select a victim offset within the set (LRU)
			uint64_t victim = *(set_address_list.begin());
			uint64_t min_ts = (uint64_t) 18446744073709551615U; // Max uint64_t
			bool min_init = false;

			if (DEBUG_VICTIM)
			{
				debug_victim << "--------------------------------------------------------------------\n";
				debug_victim << currentClockCycle << ": new miss. time to pick the unlucky line.\n";
				debug_victim << "set: " << set_index << "\n";
				debug_victim << "new flash addr: 0x" << hex << addr << dec << "\n";
				debug_victim << "new tag: " << TAG(addr)<< "\n";
				debug_victim << "scanning set address list...\n\n";
			}

			uint64_t victim_counter = 0;
			uint64_t victim_set_offset = 0;
			for (list<uint64_t>::iterator it=set_address_list.begin(); it != set_address_list.end(); it++)
			{
				cur_address = *it;
				cur_line = cache[cur_address];

				if (DEBUG_VICTIM)
				{
					debug_victim << "cur_address= 0x" << hex << cur_address << dec << "\n";
					debug_victim << "cur_tag= " << cur_line.tag << "\n";
					debug_victim << "dirty= " << cur_line.dirty << "\n";
					debug_victim << "valid= " << cur_line.valid << "\n";
					debug_victim << "ts= " << cur_line.ts << "\n";
					debug_victim << "min_ts= " << min_ts << "\n\n";
				}

				// If the current line is the least recent we've seen so far, then select it.
				// But do not select it if the line is locked.
				if (((cur_line.ts < min_ts) || (!min_init)) && (!cur_line.locked))
				{
					victim = cur_address;	
					min_ts = cur_line.ts;
					min_init = true;

					if (DEBUG_VICTIM)
					{
						victim_set_offset = victim_counter;
						debug_victim << "FOUND NEW MINIMUM!\n\n";
					}
				}

				victim_counter++;
				
			}

			if (DEBUG_VICTIM)
			{
				debug_victim << "Victim in set_offset: " << victim_set_offset << "\n\n";
			}


			cache_address = victim;
			cur_line = cache[cache_address];
			uint64_t victim_flash_addr = FLASH_ADDRESS(cur_line.tag, set_index);

			if ((cur_line.prefetched) && (cur_line.used == false))
			{
				// An unused prefetch is being removed from the cache, so transfer the count
				// to the unused_prefetch_victims counter.
				unused_prefetches--;
				unused_prefetch_victims++;
			}

			// If this is a cheating PREFETCH operation, then we don't actually want to do any work.
			// Just set the cache tags as if the operation completed and then bail.
			if (trans.transactionType == PREFETCH)
			{
				uint64_t page_address = PAGE_ADDRESS(addr);
				if (prefetch_cheat_map.count(page_address) != 0)
				{
					// Remove the page_address from the prefetch_cheat_map.
					prefetch_cheat_map[page_address] -= 1;
					if (prefetch_cheat_map[page_address] == 0)
					{
						size_t retval = prefetch_cheat_map.erase(page_address);
						assert(retval == 1);
					}

					// Set the cache lines as if the transaction was already done.
					cur_line.tag = TAG(page_address);
					cur_line.dirty = false;
					cur_line.valid = true;
					cur_line.ts = currentClockCycle;
					cur_line.used = false;

					// Since this is a prefetch, also keep track of that.
					cur_line.prefetched = true;
					total_prefetches++;
					unused_prefetches++;
					prefetch_cheat_count++;

					cache[cache_address] = cur_line;

					// Unlock the address.
					contention_unlock(addr, addr, "PREFETCH (cheat)", false, 0, false, 0);

					// Note: No callback is necessary since this is a prefetch operation.
					return;
				}

				// TODO: Figure out if the logger or anything else is going to break.

			}

			// Log the victim, set, etc.
			// THIS MUST HAPPEN AFTER THE CUR_LINE IS SET TO THE VICTIM LINE.
			if ((ENABLE_LOGGER) && ((trans.transactionType == DATA_READ) || (trans.transactionType == DATA_WRITE)))
				log.access_miss(PAGE_ADDRESS(addr), victim_flash_addr, set_index, victim, cur_line.dirty, cur_line.valid);

			// Lock the victim page so it will not be selected for eviction again during the processing of this
			// transaction's miss and so further transactions to this page cannot happen.
			// Only lock it if the cur_line is valid.
			if (cur_line.valid)
				contention_victim_lock(victim_flash_addr);

			// Lock the cache line so no one else tries to use it while this miss is being serviced.
			contention_cache_line_lock(cache_address);


			if (DEBUG_CACHE)
			{
				cerr << currentClockCycle << ": " << "MISS: victim is cache_address " << cache_address <<
						" (set: " << set_index << ")" << endl;
				cerr << cur_line.str() << endl;
				cerr << currentClockCycle << ": " << "The victim is dirty? " << cur_line.dirty << endl;
			}


			Pending p;
			p.orig_addr = trans.address;
			p.flash_addr = addr;
			p.cache_addr = cache_address;
			p.victim_tag = cur_line.tag;
			p.victim_valid = cur_line.valid;
			p.callback_sent = false;
			p.type = trans.transactionType;

			// Read the line that missed from the NVRAM.
			// This is started immediately to minimize the latency of the waiting user of HybridSim.
			LineRead(p);

			// If the cur_line is dirty, then do a victim writeback process (starting with VictimRead).
			if (cur_line.dirty)
			{
				VictimRead(p);
			}
		}
	}

	void HybridSystem::VictimRead(Pending p)
	{
		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "Performing VICTIM_READ for (" << p.flash_addr << ", " << p.cache_addr << ")\n";

		// flash_addr is the original Flash address requested from the top level Transaction.
		// victim is the base address of the DRAM page to read.
		// victim_tag is the cache tag for the victim page (used to compute the victim's flash address).

		// Increment the pending set/page counter (this is used to ensure that the pending set/page entry isn't removed until both LineRead
		// and VictimRead (if needed) are completely done.
		contention_increment(p.flash_addr);

#if SINGLE_WORD
		// Schedule a read from DRAM to get the line being evicted.
		Transaction t = Transaction(DATA_READ, p.cache_addr, NULL);
		dram_queue.push_back(t);
#else
		// Schedule reads for the entire page.
		dram_pending_wait[p.cache_addr] = unordered_set<uint64_t>();
		for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
		{
			uint64_t addr = p.cache_addr + i*BURST_SIZE;
			dram_pending_wait[p.cache_addr].insert(addr);
			Transaction t = Transaction(DATA_READ, addr, NULL);
			dram_queue.push_back(t);
		}
#endif

		// Add a record in the DRAM's pending table.
		p.op = VICTIM_READ;
		assert(dram_pending.count(p.cache_addr) == 0);
		dram_pending[p.cache_addr] = p;
	}

	void HybridSystem::VictimReadFinish(uint64_t addr, Pending p)
	{
		if (DEBUG_CACHE)
		{
			cerr << currentClockCycle << ": " << "VICTIM_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ") offset="
				<< PAGE_OFFSET(addr);
		}

#if SINGLE_WORD
		if (DEBUG_CACHE)
			cerr << " num_left=0 (SINGLE_WORD)\n";
#else
		uint64_t cache_page_addr = p.cache_addr;

		if (DEBUG_CACHE)
			cerr << " num_left=" << dram_pending_wait[cache_page_addr].size() << "\n"; 

		// Remove the read that just finished from the wait set.
		dram_pending_wait[cache_page_addr].erase(addr);

		if (!dram_pending_wait[cache_page_addr].empty())
		{
			// If not done with this line, then re-enter pending map.
			dram_pending[cache_page_addr] = p;
			dram_pending_set.erase(addr);
			return;
		}

		// The line has completed. Delete the wait set object and move on.
		dram_pending_wait.erase(cache_page_addr);
#endif


		// Decrement the pending set counter (this is used to ensure that the pending set entry isn't removed until both LineRead
		// and VictimRead (if needed) are completely done.
		contention_decrement(p.flash_addr);

		if (DEBUG_CACHE)
		{
			cerr << "The victim read to DRAM line " << PAGE_ADDRESS(addr) << " has completed.\n";
			cerr << "pending_pages[" << PAGE_ADDRESS(p.flash_addr) << "] = " << pending_pages[PAGE_ADDRESS(p.flash_addr)] << "\n";
		}

		// contention_unlock will only unlock if the pending_page counter is 0.
		// This means that LINE_READ finished first and that the pending set was not removed
		// in the CacheReadFinish or CacheWriteFinish functions (or LineReadFinish for PREFETCH).
		uint64_t victim_address = FLASH_ADDRESS(p.victim_tag, SET_INDEX(p.cache_addr));
		contention_unlock(p.flash_addr, p.orig_addr, "VICTIM_READ", p.victim_valid, victim_address, true, p.cache_addr);

		// Schedule a write to the flash to simulate the transfer
		VictimWrite(p);
	}

	void HybridSystem::VictimWrite(Pending p)
	{
		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "Performing VICTIM_WRITE for (" << p.flash_addr << ", " << p.cache_addr << ")\n";

		// Compute victim flash address.
		// This is where the victim line is stored in the Flash address space.
		uint64_t victim_flash_addr = (p.victim_tag * NUM_SETS + SET_INDEX(p.flash_addr)) * PAGE_SIZE; 

#if SINGLE_WORD
		// Schedule a write to Flash to save the evicted line.
		Transaction t = Transaction(DATA_WRITE, victim_flash_addr, NULL);
		flash_queue.push_back(t);
#else
		// Schedule writes for the entire page.
		for(uint64_t i=0; i<PAGE_SIZE/FLASH_BURST_SIZE; i++)
		{
			Transaction t = Transaction(DATA_WRITE, victim_flash_addr + i*FLASH_BURST_SIZE, NULL);
			flash_queue.push_back(t);
		}
#endif

		// No pending event schedule necessary (might add later for debugging though).
	}

	void HybridSystem::LineRead(Pending p)
	{
		if (DEBUG_CACHE)
		{
			cerr << currentClockCycle << ": " << "Performing LINE_READ for (" << p.flash_addr << ", " << p.cache_addr << ")\n";
			cerr << "the page address was " << PAGE_ADDRESS(p.flash_addr) << endl;
		}

		uint64_t page_addr = PAGE_ADDRESS(p.flash_addr);


		// Increment the pending set counter (this is used to ensure that the pending set entry isn't removed until both LineRead
		// and VictimRead (if needed) are completely done.
		contention_increment(p.flash_addr);


#if SINGLE_WORD
		// Schedule a read from Flash to get the new line 
		Transaction t = Transaction(DATA_READ, page_addr, NULL);
		flash_queue.push_back(t);
#else
		// Schedule reads for the entire page.
		flash_pending_wait[page_addr] = unordered_set<uint64_t>();
		for(uint64_t i=0; i<PAGE_SIZE/FLASH_BURST_SIZE; i++)
		{
			uint64_t addr = page_addr + i*FLASH_BURST_SIZE;
			flash_pending_wait[page_addr].insert(addr);
			Transaction t = Transaction(DATA_READ, addr, NULL);
			flash_queue.push_back(t);
		}
#endif

		// Add a record in the Flash's pending table.
		p.op = LINE_READ;
		flash_pending[page_addr] = p;
	}


	void HybridSystem::LineReadFinish(uint64_t addr, Pending p)
	{

		if (DEBUG_CACHE)
		{
			cerr << currentClockCycle << ": " << "LINE_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ") offset="
				<< PAGE_OFFSET(addr);
		}

#if SINGLE_WORD
		if (DEBUG_CACHE)
			cerr << " num_left=0 (SINGLE_WORD)\n";
#else
		uint64_t page_addr = PAGE_ADDRESS(p.flash_addr);

		if (DEBUG_CACHE)
			cerr << " num_left=" << flash_pending_wait[page_addr].size() << "\n"; 

		// Remove the read that just finished from the wait set.
		flash_pending_wait[page_addr].erase(addr);

		if (!flash_pending_wait[page_addr].empty())
		{
			// If not done with this line, then re-enter pending map.
			flash_pending[PAGE_ADDRESS(addr)] = p;
			return;
		}

		// The line has completed. Delete the wait set object and move on.
		flash_pending_wait.erase(page_addr);
#endif


		// Decrement the pending set counter (this is used to ensure that the pending set entry isn't removed until both LineRead
		// and VictimRead (if needed) are completely done.
		contention_decrement(p.flash_addr);

		if (DEBUG_CACHE)
		{
			cerr << "The line read to Flash line " << PAGE_ADDRESS(addr) << " has completed.\n";
		}


		// Update the cache state
		cache_line cur_line = cache[p.cache_addr];
		cur_line.tag = TAG(p.flash_addr);
		cur_line.dirty = false;
		cur_line.valid = true;
		cur_line.ts = currentClockCycle;
		cur_line.used = false;
		if (p.type == PREFETCH)
		{
			cur_line.prefetched = true;
			total_prefetches++;
			unused_prefetches++;
		}
		else
		{
			cur_line.prefetched = false;
		}
		cache[p.cache_addr] = cur_line;

		// Schedule LineWrite operation to store the line in DRAM.
		LineWrite(p);

		// Use the CacheReadFinish/CacheWriteFinish functions to mark the page dirty (DATA_WRITE only), perform
		// the callback to the requesting module, and remove this set from the pending sets to allow future
		// operations to this set to start.
		// Note: Only write operations are pending at this point, which will not interfere with future operations.
		if (p.type == DATA_READ)
			CacheReadFinish(p.cache_addr, p);
		else if(p.type == DATA_WRITE)
			CacheWriteFinish(p);
		else if(p.type == PREFETCH)
		{
			// Do not call cache functions because prefetch does not send data back to the caller.

			// Erase the page from the pending set.
			// Note: the if statement is needed to ensure that the VictimRead operation (if it was invoked as part of a cache miss)
			// is already complete. If not, the pending_set removal will be done in VictimReadFinish().

			uint64_t victim_address = FLASH_ADDRESS(p.victim_tag, SET_INDEX(p.cache_addr));
			contention_unlock(p.flash_addr, p.orig_addr, "PREFETCH", p.victim_valid, victim_address, true, p.cache_addr);
		}
	}


	void HybridSystem::LineWrite(Pending p)
	{
		// After a LineRead from flash completes, the LineWrite stores the read line into the DRAM.

		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "Performing LINE_WRITE for (" << p.flash_addr << ", " << p.cache_addr << ")\n";

#if SINGLE_WORD
		// Schedule a write to DRAM to simulate the write of the line that was read from Flash.
		Transaction t = Transaction(DATA_WRITE, p.cache_addr, NULL);
		dram_queue.push_back(t);
#else
		// Schedule writes for the entire page.
		for(uint64_t i=0; i<PAGE_SIZE/BURST_SIZE; i++)
		{
			Transaction t = Transaction(DATA_WRITE, p.cache_addr + i*BURST_SIZE, NULL);
			dram_queue.push_back(t);
		}
#endif

		// No pending event schedule necessary (might add later for debugging though).
	}


	void HybridSystem::CacheRead(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr)
	{
		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "Performing CACHE_READ for (" << flash_addr << ", " << cache_addr << ")\n";

		// Compute the actual DRAM address of the data word we care about.
		uint64_t data_addr = cache_addr + PAGE_OFFSET(flash_addr);

		assert(cache_addr == PAGE_ADDRESS(data_addr));

		Transaction t = Transaction(DATA_READ, data_addr, NULL);
		dram_queue.push_back(t);

		// Update the cache state
		// This could be done here or in CacheReadFinish
		// It really doesn't matter (AFAICT) as long as it is consistent.
		cache_line cur_line = cache[cache_addr];
		cur_line.ts = currentClockCycle;
		if ((cur_line.prefetched) && (cur_line.used == false)) // Note: this if statement must come before cur_line.used is set to true.
			unused_prefetches--;
		cur_line.used = true;
		cache[cache_addr] = cur_line;

		// Add a record in the DRAM's pending table.
		Pending p;
		p.op = CACHE_READ;
		p.orig_addr = orig_addr;
		p.flash_addr = flash_addr;
		p.cache_addr = cache_addr;
		p.victim_tag = 0;
		p.victim_valid = false;
		p.callback_sent = false;
		p.type = DATA_READ;
		assert(dram_pending.count(data_addr) == 0);
		dram_pending[data_addr] = p;

		// Assertions for "this can't happen" situations.
		assert(dram_pending.count(data_addr) != 0);
	}

	void HybridSystem::CacheReadFinish(uint64_t addr, Pending p)
	{
		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "CACHE_READ callback for (" << p.flash_addr << ", " << p.cache_addr << ")\n";

		// Read operation has completed, call the top level callback.
		// Only do this if it hasn't been sent already by the critical cache line first callback.
		// Also, do not do this for prefetch since it does not have an external caller waiting on it.
		if (!p.callback_sent)
			ReadDoneCallback(systemID, p.orig_addr, currentClockCycle);

		// Erase the page from the pending set.
		// Note: the if statement is needed to ensure that the VictimRead operation (if it was invoked as part of a cache miss)
		// is already complete. If not, the pending_set removal will be done in VictimReadFinish().
		uint64_t victim_address = FLASH_ADDRESS(p.victim_tag, SET_INDEX(p.cache_addr));
		contention_unlock(p.flash_addr, p.orig_addr, "CACHE_READ", p.victim_valid, victim_address, true, p.cache_addr);
	}

	void HybridSystem::CacheWrite(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr)
	{
		if (DEBUG_CACHE)
			cerr << currentClockCycle << ": " << "Performing CACHE_WRITE for (" << flash_addr << ", " << cache_addr << ")\n";

		// Compute the actual DRAM address of the data word we care about.
		uint64_t data_addr = cache_addr + PAGE_OFFSET(flash_addr);

		Transaction t = Transaction(DATA_WRITE, data_addr, NULL);
		dram_queue.push_back(t);

		// Finish the operation by updating cache state, doing the callback, and removing the pending set.
		// Note: This is only split up so the LineWrite operation can reuse the second half
		// of CacheWrite without actually issuing a new write.
		Pending p;
		p.orig_addr = orig_addr;
		p.flash_addr = flash_addr;
		p.cache_addr = cache_addr;
		p.victim_tag = 0;
		p.victim_valid = false;
		p.callback_sent = false;
		p.type = DATA_WRITE;

		CacheWriteFinish(p);

	}

	//void HybridSystem::CacheWriteFinish(uint64_t orig_addr, uint64_t flash_addr, uint64_t cache_addr, bool callback_sent)
	void HybridSystem::CacheWriteFinish(Pending p)
	{
		// Update the cache state
		cache_line cur_line = cache[p.cache_addr];
		cur_line.dirty = true;
		cur_line.valid = true;
		if ((cur_line.prefetched) && (cur_line.used == false)) // Note: this if statement must come before cur_line.used is set to true.
			unused_prefetches--;
		cur_line.used = true;
		cur_line.ts = currentClockCycle;
		cache[p.cache_addr] = cur_line;

		if (DEBUG_CACHE)
			cerr << cur_line.str() << endl;

		// Call the top level callback.
		// This is done immediately rather than waiting for callback.
		// Only do this if it hasn't been sent already by the critical cache line first callback.
		if (!p.callback_sent)
			WriteDoneCallback(systemID, p.orig_addr, currentClockCycle);

		// Erase the page from the pending set.
		// Note: the if statement is needed to ensure that the VictimRead operation (if it was invoked as part of a cache miss)
		// is already complete. If not, the pending_set removal will be done in VictimReadFinish().
		uint64_t victim_address = FLASH_ADDRESS(p.victim_tag, SET_INDEX(p.cache_addr));
		contention_unlock(p.flash_addr, p.orig_addr, "CACHE_WRITE", p.victim_valid, victim_address, true, p.cache_addr);
	}

	
	void HybridSystem::Flush(uint64_t cache_addr)
	{
		// The flush transaction simply sets the timestamp of the current cache line to 0.
		// This forces the next miss in this set to remove this line.
		// Note: Flush does not actually cause a write to happen.

		// Update the cache state
		cache_line cur_line = cache[cache_addr];
		cur_line.ts = 0;
		cache[cache_addr] = cur_line;

		uint64_t set_index = SET_INDEX(cache_addr);
		uint64_t flash_address = FLASH_ADDRESS(cur_line.tag, set_index);
		contention_unlock(flash_address, flash_address, "FLUSH", false, 0, true, cache_addr);
	}

	void HybridSystem::RegisterCallbacks( TransactionCompleteCB *readDone, TransactionCompleteCB *writeDone)
	{
		// Save the external callbacks.
		ReadDone = readDone;
		WriteDone = writeDone;
	}


	void HybridSystem::DRAMReadCallback(uint id, uint64_t addr, uint64_t cycle)
	{
		// Determine which address to look up in the pending table.
		// If there is an entry for this page in the dram_pending_wait, then that
		// means this is for a VICTIM_READ operation and we should use the page address.
		// Otherwise, this is for a CACHE_READ operation and we should use the addr directly.
		uint64_t pending_addr;
		if (dram_pending_wait.count(PAGE_ADDRESS(addr)) != 0)
		{
			pending_addr = PAGE_ADDRESS(addr);
		}
		else
		{
			pending_addr = addr;
		}


		if (dram_pending.count(pending_addr) != 0)
		{
			// Get the pending object for this transaction.
			Pending p = dram_pending[pending_addr];

			// Remove this pending object from dram_pending
			dram_pending.erase(pending_addr);
			assert(dram_pending.count(pending_addr) == 0);

			if (p.op == VICTIM_READ)
			{
				VictimReadFinish(addr, p);
			}
			else if (p.op == CACHE_READ)
			{
				CacheReadFinish(addr, p);
			}
			else
			{
				ERROR("DRAMReadCallback received an invalid op.");
				abort();
			}
		}
		else
		{
			ERROR("DRAMReadCallback received an address not in the pending set.");
			abort();
		}

		// Erase from the pending set AFTER everything else (so I can see if failed values are in the pending set).
		dram_pending_set.erase(addr);
	}

	void HybridSystem::DRAMWriteCallback(uint id, uint64_t addr, uint64_t cycle)
	{
		// Nothing to do (it doesn't matter when the DRAM write finishes for the cache controller, as long as it happens).
		dram_pending_set.erase(addr);
	}

	void HybridSystem::DRAMPowerCallback(double a, double b, double c, double d)
	{
		printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
	}

	void HybridSystem::FlashReadCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped)
	{
		if (flash_pending.count(PAGE_ADDRESS(addr)) != 0)
		{
			// Get the pending object.
			Pending p = flash_pending[PAGE_ADDRESS(addr)];

			// Remove this pending object from flash_pending
			flash_pending.erase(PAGE_ADDRESS(addr));

			if (p.op == LINE_READ)
			{
				LineReadFinish(addr, p);
			}
			else
			{
				ERROR("FlashReadCallback received an invalid op.");
				abort();
			}
		}
		else
		{
			ERROR("FlashReadCallback received an address not in the pending set.");
			cerr << "flash_pending count was " << flash_pending.count(PAGE_ADDRESS(addr)) << "\n";
			cerr << "address: " << addr << " page: " << PAGE_ADDRESS(addr) << " set: " << SET_INDEX(addr) << "\n";
			abort();
		}
	}

	void HybridSystem::FlashCriticalLineCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped)
	{
		// This function is called to implement critical line first for reads.
		// This allows HybridSim to tell the external user it can make progress as soon as the data
		// it is waiting for is back in the memory controller.

		//cerr << cycle << ": Critical Line Callback Received for address " << addr << "\n";

		if (flash_pending.count(PAGE_ADDRESS(addr)) != 0)
		{
			// Get the pending object.
			Pending p = flash_pending[PAGE_ADDRESS(addr)];

			// Note: DO NOT REMOVE THIS FROM THE PENDING SET.


			if (p.op == LINE_READ)
			{
				if (p.callback_sent)
				{
					ERROR("FlashCriticalLineCallback called twice on the same pending item.");
					abort();
				}
					
				// Make the callback and mark it as being called.
				if (p.type == DATA_READ)
					ReadDoneCallback(systemID, p.orig_addr, currentClockCycle);
				else if(p.type == DATA_WRITE)
					WriteDoneCallback(systemID, p.orig_addr, currentClockCycle);
				else
				{
					// Do nothing because this is a PREFETCH.
				}

				// Mark the pending item's callback as being sent so it isn't sent again later.
				p.callback_sent = true;

				flash_pending[PAGE_ADDRESS(addr)] = p;
			}
			else
			{
				ERROR("FlashCriticalLineCallback received an invalid op.");
				abort();
			}
		}
		else
		{
			ERROR("FlashCriticalLineCallback received an address not in the pending set.");
			abort();
		}

	}

	void HybridSystem::FlashWriteCallback(uint64_t id, uint64_t addr, uint64_t cycle, bool unmapped)
	{
		// Nothing to do (it doesn't matter when the flash write finishes for the cache controller, as long as it happens).

		if (DEBUG_CACHE)
			cerr << "The write to Flash line " << PAGE_ADDRESS(addr) << " has completed.\n";
	}



	void HybridSystem::ReadDoneCallback(uint sysID, uint64_t orig_addr, uint64_t cycle)
	{
		if (ReadDone != NULL)
		{
			uint64_t callback_addr = orig_addr;
			if (REMAP_MMIO)
			{
				if (orig_addr >= THREEPOINTFIVEGB)
				{
					// Give the same address in the callback that we originally received.
					callback_addr += HALFGB;
				}
			}

			// Call the callback.
			(*ReadDone)(sysID, callback_addr, cycle);
		}

		// Finish the logging for this access.
		if (ENABLE_LOGGER)
			log.access_stop(orig_addr);
	}


	void HybridSystem::WriteDoneCallback(uint sysID, uint64_t orig_addr, uint64_t cycle)
	{
		if (WriteDone != NULL)
		{
			uint64_t callback_addr = orig_addr;
			if (REMAP_MMIO)
			{
				if (orig_addr >= THREEPOINTFIVEGB)
				{
					// Give the same address in the callback that we originally received.
					callback_addr += HALFGB;
				}
			}

			// Call the callback.
			(*WriteDone)(sysID, callback_addr, cycle);
		}

		// Finish the logging for this access.
		if (ENABLE_LOGGER)
			log.access_stop(orig_addr);
	}

	void HybridSystem::reportPower()
	{
		// TODO: Remove this funnction from the external API.
	}



	string HybridSystem::SetOutputFileName(string tracefilename) { return ""; }

	void HybridSystem::printLogfile()
	{
		// Save the cache table if necessary.
		saveCacheTable();

		cerr << "TLB Misses: " << tlb_misses << "\n";
		cerr << "TLB Hits: " << tlb_hits << "\n";
		cerr << "Total prefetches: " << total_prefetches << "\n";
		cerr << "Unused prefetches in cache: " << unused_prefetches << "\n";
		cerr << "Unused prefetch victims: " << unused_prefetch_victims << "\n";
		cerr << "Prefetch hit NOPs: " << prefetch_hit_nops << "\n";
		cerr << "Prefetch cheat count: " << prefetch_cheat_count << "\n";

		if (ENABLE_STREAM_BUFFER)
		{
			cerr << "Unique one misses: " << unique_one_misses << "\n";
			cerr << "Unique stream buffers: " << unique_stream_buffers << "\n";
			cerr << "Stream buffers hits: " << stream_buffer_hits << "\n";
		}

		// Print out the log file.
		if (ENABLE_LOGGER)
		{
			log.print();
			flash->saveStats();
		}
	}


	void HybridSystem::restoreCacheTable()
	{
		if (PREFILL_CACHE)
		{
			// Fill the cache table.
			for (uint64_t i=0; i<CACHE_PAGES; i++)
			{
				uint64_t cache_addr = i*PAGE_SIZE;
				cache_line line;

				line.valid = true;
				line.dirty = PREFILL_CACHE_DIRTY;
				line.locked = false;
				line.tag = TAG(cache_addr);
				line.data = 0;
				line.ts = 0;

				// Put this in the cache.
				cache[cache_addr] = line;
			}
		}

		if (ENABLE_RESTORE)
		{
			cerr << "PERFORMING RESTORE OF CACHE TABLE!!!\n";

			ifstream inFile;
			confirm_directory_exists("state"); // Assumes using state directory, otherwise the user is on their own.
			inFile.open(HYBRIDSIM_RESTORE_FILE);
			if (!inFile.is_open())
			{
				cerr << "ERROR: Failed to load HybridSim's state restore file: " << HYBRIDSIM_RESTORE_FILE << "\n";
				abort();
			}

			uint64_t tmp;

			// Read the parameters and confirm that they are the same as the current HybridSystem instance.
			inFile >> tmp;
			if (tmp != PAGE_SIZE)
			{
				cerr << "ERROR: Attempted to restore state and PAGE_SIZE does not match in restore file and ini file."  << "\n";
				abort();
			}
			inFile >> tmp;
			if (tmp != SET_SIZE)
			{
				cerr << "ERROR: Attempted to restore state and SET_SIZE does not match in restore file and ini file."  << "\n";
				abort();
			}
			inFile >> tmp;
			if (tmp != CACHE_PAGES)
			{
				cerr << "ERROR: Attempted to restore state and CACHE_PAGES does not match in restore file and ini file."  << "\n";
				abort();
			}
			inFile >> tmp;
			if (tmp != TOTAL_PAGES)
			{
				cerr << "ERROR: Attempted to restore state and TOTAL_PAGES does not match in restore file and ini file."  << "\n";
				abort();
			}
				
			// Read the cache table.
			while(inFile.good())
			{
				uint64_t cache_addr;
				cache_line line;

				// Get the cache line data from the file.
				inFile >> cache_addr;
				inFile >> line.valid;
				inFile >> line.dirty;
				inFile >> line.tag;
				inFile >> line.data;
				inFile >> line.ts;

				if (RESTORE_CLEAN)
				{
					line.dirty = 0;
				}

				// The line must not be locked on restore.
				// This is a point of weirdness with the replay warmup design (since we can't restore the system
				// exactly as it was), but it is unavoidable. In flight transactions are simply lost. Although, if
				// replay warmup is done right, the system should run until all transactions are processed.
				line.locked = false;

				// Put this in the cache.
				cache[cache_addr] = line;
			}
		
			inFile.close();

			flash->loadNVState(NVDIMM_RESTORE_FILE);
		}
	}

	void HybridSystem::saveCacheTable()
	{
		if (ENABLE_SAVE)
		{
			ofstream savefile;
			confirm_directory_exists("state"); // Assumes using state directory, otherwise the user is on their own.
			savefile.open(HYBRIDSIM_SAVE_FILE, ios_base::out | ios_base::trunc);
			if (!savefile.is_open())
			{
				cerr << "ERROR: Failed to load HybridSim's state save file: " << HYBRIDSIM_SAVE_FILE << "\n";
				abort();
			}
			cerr << "PERFORMING SAVE OF CACHE TABLE!!!\n";

			savefile << PAGE_SIZE << " " << SET_SIZE << " " << CACHE_PAGES << " " << TOTAL_PAGES << "\n";

			for (uint64_t i=0; i < CACHE_PAGES; i++)
			{
				uint64_t cache_addr= i * PAGE_SIZE;

				if (cache.count(cache_addr) == 0)
					// Skip to next page if this cache_line entry is not in the cache table.
					continue;

				// Get the line entry.
				cache_line line = cache[cache_addr];

				if (!line.valid)
					// If the line isn't valid, then don't need to save it.
					continue;
				
				savefile << cache_addr << " " << line.valid << " " << line.dirty << " " << line.tag << " " << line.data << " " << line.ts << "\n";
			}

			savefile.close();

			flash->saveNVState(NVDIMM_SAVE_FILE);
		}
	}



	// Page Contention functions
	void HybridSystem::contention_lock(uint64_t flash_addr)
	{
		pending_flash_addr[flash_addr] = 0;
	}

	void HybridSystem::contention_page_lock(uint64_t flash_addr)
	{
		// Add to the pending pages map. And set the count to 0.
		pending_pages[PAGE_ADDRESS(flash_addr)] = 0;
	}

	void HybridSystem::contention_unlock(uint64_t flash_addr, uint64_t orig_addr, string operation, bool victim_valid, uint64_t victim_page, 
			bool cache_line_valid, uint64_t cache_addr)
	{
		uint64_t page_addr = PAGE_ADDRESS(flash_addr);

		// If there is no page entry, then this means only the flash address was locked (i.e. it is a DRAM hit).
		if (pending_pages.count(page_addr) == 0)
		{
			int num = pending_flash_addr.erase(flash_addr);
			assert(num == 1);

			// Victim should never be valid if we were only servicing a cache hit.
			assert(victim_valid == false);

			// If the cache line is valid, unlock it.
			if (cache_line_valid)
				contention_cache_line_unlock(cache_addr);

			// Restart queue checking.
			this->check_queue = true;
			pending_count -= 1;

			return;
		}

		// At this point, we know that the page_addr is in the pending_pages list.
		// This implies we also know that there was a cache miss for this access.
		// If the count for the pending_pages entry is > 0, then we DO NOT unlock
		// the page yet.

		// Erase the page from the pending page map.
		// Note: the if statement is needed to ensure that the VictimRead operation (if it was invoked as part of a cache miss)
		// is already complete. If not, the pending_set removal will be done in VictimReadFinish().
		else if (pending_pages[PAGE_ADDRESS(flash_addr)] == 0)
		{
			int num = pending_pages.erase(PAGE_ADDRESS(flash_addr));
			if (num != 1)
			{
				cerr << "pending_pages.erase() was called after " << operation << " and num was 0.\n";
				cerr << "orig:" << orig_addr << " aligned:" << flash_addr << "\n\n";
				abort();
			}

			// Also remove the pending_flash_addr entry.
			num = pending_flash_addr.erase(flash_addr);
			assert(num == 1);

			// If the victim page is valid, then unlock it too.
			if (victim_valid)
				contention_victim_unlock(victim_page);

			// If the cache line is valid, unlock it.
			if (cache_line_valid)
				contention_cache_line_unlock(cache_addr);

			// Restart queue checking.
			this->check_queue = true;
			pending_count -= 1;
		}
	}

	bool HybridSystem::contention_is_unlocked(uint64_t flash_addr)
	{
		uint64_t page_addr = PAGE_ADDRESS(flash_addr);

		// First see if the set is locked. This is done by looking at the set_counter.
		// If the set counter exists and is equal to the set size, then we should NOT be trying to do any more accesses
		// to the set, because this means that all of the cache lines are locked.
		uint64_t set_index = SET_INDEX(page_addr);
		if (set_counter.count(set_index) > 0)
		{
			if (set_counter[set_index] == SET_SIZE)
			{
				return false;
			}
		}

		// If the page is not in the penting_pages and pending_flash_addr map, then it is unlocked.
		if ((pending_pages.count(page_addr) == 0) && (pending_flash_addr.count(flash_addr) == 0))
			return true;
		else
			return false;
	}


	void HybridSystem::contention_increment(uint64_t flash_addr)
	{
		uint64_t page_addr = PAGE_ADDRESS(flash_addr);
		// TODO: Add somme error checking here (e.g. make sure page is in pending_pages and make sure count is >= 0)

		// This implements a counting semaphore for the page so that it isn't unlocked until the count is 0.
		pending_pages[page_addr] += 1;
	}

	void HybridSystem::contention_decrement(uint64_t flash_addr)
	{
		uint64_t page_addr = PAGE_ADDRESS(flash_addr);
		// TODO: Add somme error checking here (e.g. make sure page is in pending_pages and make sure count is >= 0)

		// This implements a counting semaphore for the page so that it isn't unlocked until the count is 0.
		pending_pages[page_addr] -= 1;
	}

	void HybridSystem::contention_victim_lock(uint64_t page_addr)
	{
		pending_pages[page_addr] = 0;
	}

	void HybridSystem::contention_victim_unlock(uint64_t page_addr)
	{
		int num = pending_pages.erase(page_addr);
		assert(num == 1);
	}

	void HybridSystem::contention_cache_line_lock(uint64_t cache_addr)
	{
		cache_line cur_line = cache[cache_addr];
		cur_line.locked = true;
		cur_line.lock_count++;
		cache[cache_addr] = cur_line;

		uint64_t set_index = SET_INDEX(cache_addr);
		if (set_counter.count(set_index) == 0)
			set_counter[set_index] = 1;
		else
			set_counter[set_index] += 1;
	}

	void HybridSystem::contention_cache_line_unlock(uint64_t cache_addr)
	{
		cache_line cur_line = cache[cache_addr];
		cur_line.lock_count--;
		assert(cur_line.lock_count >= 0);
		if (cur_line.lock_count == 0)
			cur_line.locked = false; // Only unlock if the count for outstanding accesses is 0.
		cache[cache_addr] = cur_line;

		uint64_t set_index = SET_INDEX(cache_addr);
		set_counter[set_index] -= 1;
	}

	// PREFETCHING FUNCTIONS
	void HybridSystem::issue_sequential_prefetches(uint64_t page_addr)
	{
		// Count down from the top address. This must be done because addPrefetch puts transactions at the front
		// of the queue and we want page_addr+PAGE_SIZE to be the first prefetch issued.
		for (int i=SEQUENTIAL_PREFETCHING_WINDOW; i > 0; i--)
		{
			// Compute the next prefetch address.
			uint64_t prefetch_address = page_addr + (i * PAGE_SIZE);

			// If address is above the legal address space for the main memory, then do not issue this prefetch.
			if (prefetch_address >= (TOTAL_PAGES * PAGE_SIZE))
				continue;

			// Add the prefetch.
			addPrefetch(prefetch_address);
			//cerr << currentClockCycle << ": Prefetcher adding " << prefetch_address << " to transaction queue.\n";
		}
	}


	void HybridSystem::sync(uint64_t addr, uint64_t cache_address, Transaction trans)
	{
		// The SYNC command works by reusing the VictimRead infrastructure. This works pretty well
		// except we have to be careful because that code was originaly designed to work when a miss 
		// had occurred. SYNC only happens when there is a hit.

		// TODO: Abtract this code into a common function with the miss path (if possible).

		cache_line cur_line = cache[cache_address];

		uint64_t victim_flash_addr = FLASH_ADDRESS(cur_line.tag, SET_INDEX(cache_address));

		// The address in the cache line should be the SAME as the address we are syncing on.
		assert(victim_flash_addr == addr);

		// Lock the cache line so no one else tries to use it while this miss is being serviced.
		contention_cache_line_lock(cache_address);
	
		Pending p;
		p.orig_addr = trans.address;
		p.flash_addr = addr;
		p.cache_addr = cache_address;
		p.victim_tag = cur_line.tag;
		p.victim_valid = false; // MUST SET THIS TO FALSE SINCE SYNC PAGE AND VICTIM PAGE MATCH.
		p.callback_sent = false;
		p.type = trans.transactionType;

		// The line MUST be dirty for a sync operation to be valid.
		assert(cur_line.dirty);

		VictimRead(p);

		// Mark the line clean (since this is the whole point of SYNC).
		cur_line.dirty = false;
		cache[cache_address] = cur_line;
	}


	void HybridSystem::syncAllCounter(uint64_t addr, Transaction trans)
	{
		//cout << "Processing SYNC_ALL_COUNTER " << addr << "\n";
		uint64_t next_addr = addr + PAGE_SIZE;

		//cout << "next_addr = " << next_addr << endl;
		if (next_addr < (CACHE_PAGES * PAGE_SIZE))
		{
			// Issue SYNC_ALL_COUNTER transaction to next_addr.
			// This is what iterates through all lines.
			// Note: This must be done BEFORE the SYNC is added for the current line so 
			// it is placed after the SYNC in the queue with add_front().
			addSyncCounter(next_addr, false);
		}

		// Look up cache line.
		if (cache.count(addr) == 0)
		{
			// If i is not allocated yet, allocate it.
			cache[addr] = cache_line();
		}
		cache_line cur_line = cache[addr];

		if (cur_line.valid && cur_line.dirty)
		{
			// Compute flash address.
			uint64_t flash_addr = FLASH_ADDRESS(cur_line.tag, SET_INDEX(addr));

			// Issue sync command for flash address.
			addSync(flash_addr);
			//cout << "Added sync for address " << flash_addr << endl;
		}

		// Unlock the page and return.
		contention_unlock(addr, trans.address, "SYNC_ALL_COUNTER", false, 0, false, 0);
	}


	void HybridSystem::addSync(uint64_t addr)
	{
		// Create flush transaction.
		Transaction t = Transaction(SYNC, addr, NULL);

		// Push the operation onto the front of the transaction queue so it stays at the front.
		trans_queue.push_front(t);

		trans_queue_size += 1;

		pending_count += 1;

		// Restart queue checking.
		this->check_queue = true;

	}


	void HybridSystem::addSyncCounter(uint64_t addr, bool initial)
	{
		// Create flush transaction.
		Transaction t = Transaction(SYNC_ALL_COUNTER, addr, NULL);

		if (initial)
		{
			// The initial SYNC_ALL_COUNTER operation must wait to get to the front of the queue.
			trans_queue.push_back(t);
		}
		else
		{
			// Push the operation onto the front of the transaction queue so it stays at the front.
			trans_queue.push_front(t);
		}

		trans_queue_size += 1;

		pending_count += 1;

		// Restart queue checking.
		this->check_queue = true;
	}


	void HybridSystem::mmio(uint64_t operation, uint64_t address)
	{
		if (operation == 0)
		{
			// NOP
			cerr << "\n" << currentClockCycle << " : HybridSim received MMIO NOP.\n";
		}
		else if (operation == 1)
		{
			// SYNC_ALL
			cerr << "\n" << currentClockCycle << " : HybridSim received MMIO SYNC_ALL.\n";
			syncAll();
		}
		else if (operation == 2)
		{
			// TASK_SWITCH
			cerr << "\n" << currentClockCycle << " : HybridSim received MMIO TASK_SWITCH.\n";
			cerr << "\n" << "Address=" << address << " Accesses=" << log.num_accesses << " Reads=" << log.num_reads << " Writes=" << log.num_writes << "\n";
		}
		else if ((operation == 3) || (operation == 4))
		{
			// PREFETCH_RANGE

			// Split address into lower 48 bits for base address and upper 16 bits for number of pages to prefetch.
			// TODO: Rework this so the upper 48 bits are the address and the lower 48 bits are the prefetch_pages count.
			uint64_t base_address = address & 0x0000FFFFFFFFFFFF;
			uint64_t prefetch_pages = (address >> 48) & 0x000000000000FFFF;
			bool cheat = (operation == 4);

			// Add one to prefetch_pages. This means 0 in the upper bits means prefetch a single page.
			// This allows the caller to prefetch up to 2^16 pages (256 MB when using 4k pages).
			prefetch_pages += 1;

			cerr << "\n" << currentClockCycle << " : HybridSim received MMIO PREFETCH_RANGE. ";
			cerr << "base_address=" << base_address << " prefetch_pages=" << prefetch_pages << " cheat=" << cheat << "\n";

			for (uint64_t i=0; i<prefetch_pages; i++)
			{
				uint64_t prefetch_addr = base_address + i*PAGE_SIZE;
				addPrefetch(prefetch_addr);

				// If using cheat mode, then put this address in the cheat map.
				if (cheat)
				{
					// Create an entry in the cheap map if it already isn't in there.
					if (prefetch_cheat_map.count(prefetch_addr) == 0)
					{
						prefetch_cheat_map[prefetch_addr] = 0;
					}

					// Increment the number of cheat prefetches are outstanding for this address.
					prefetch_cheat_map[prefetch_addr] += 1;
				}
			}
		}
		else
		{
			cerr << "\n" << currentClockCycle << " : HybridSim received invalid MMIO operation.\n";
			abort();
		}
	}


	void HybridSystem::syncAll()
	{
		addSyncCounter(0, true);
	}

	void HybridSystem::check_tlb(uint64_t page_addr)
	{
		// A TLB_SIZE of 0 disables the TLB.
		// This means we always have the tags in SRAM on the CPU.
		if (TLB_SIZE == 0)
			return;

		// TLB processing code.
		uint64_t tlb_base_addr = TLB_BASE_ADDRESS(page_addr);
		if (tlb_base_set.count(tlb_base_addr) == 0)
		{
			//cerr << "TLB miss with address " << page_addr << ".\n";
			tlb_misses++;
			// TLB miss.
			if (tlb_base_set.size() == TLB_MAX_ENTRIES)
			{
				// TLB is full, so must pick a victim.
				uint64_t tlb_victim = (*(tlb_base_set.begin())).first;
				uint64_t tlb_victim_ts = (*(tlb_base_set.begin())).second;
				unordered_map<uint64_t, uint64_t>:: iterator tlb_it;
				for (tlb_it = tlb_base_set.begin(); tlb_it != tlb_base_set.end(); tlb_it++)
				{
					uint64_t cur_ts = (*tlb_it).second;
					if (cur_ts < tlb_victim_ts)
					{
						// Found an older entry than the current victim.
						tlb_victim = (*tlb_it).first;
						tlb_victim_ts = cur_ts;
					}
				}

				// Remove the victim entry.
				//cerr << "Evicting " << tlb_victim << " from TLB.\n";
				tlb_base_set.erase(tlb_victim);
			}

			// At this point, there is at least one empty spot in the TLB.
			assert(tlb_base_set.size() < TLB_MAX_ENTRIES);
			
			// Insert the new page with the current clock cycle.
			tlb_base_set[tlb_base_addr] = currentClockCycle;

			// Add TLB_MISS_DELAY to the controller delay.
			delay_counter += TLB_MISS_DELAY;
		}
		else
		{
			// TLB hit. Just update the timestamp for the LRU algorithm.
			//cerr << "TLB hit with address " << page_addr << ".\n";
			tlb_hits++;
			tlb_base_set[tlb_base_addr] = currentClockCycle;
		}
	}

	void HybridSystem::stream_buffer_miss_handler(uint64_t miss_page)
	{
		// Don't do any miss processing for the first or last pages in memory.
		if ((miss_page == 0) || (miss_page == (TOTAL_PAGES - 1)*PAGE_SIZE))
			return;

		// Calculate the neighboring pages.
		uint64_t prior_page = miss_page - PAGE_SIZE;
		uint64_t next_page = miss_page + PAGE_SIZE;

		// Look through the one miss table to see if there are any matches.
		bool stream_detected = false;
		list<pair<uint64_t, uint64_t> >::iterator it;
		for (it=one_miss_table.begin(); it != one_miss_table.end(); it++)
		{
			uint64_t entry_page = (*it).first;
			//uint64_t entry_cycle = (*it).second;

			if (entry_page == miss_page)
			{
				// Somehow we managed to miss the same page twice in a short period of time.
				// Go ahead and remove this entry to make room for this to be readded.
				it = one_miss_table.erase(it);

				if (DEBUG_STREAM_BUFFER)
					cerr << currentClockCycle << " : Stream buffer one miss double hit. miss_page=" << miss_page << "\n";
			}
			if (entry_page == prior_page)
			{
				// Stream detected!
				stream_detected = true;

				if (DEBUG_STREAM_BUFFER)
					cerr << currentClockCycle << " : New stream detected. Allocating stream buffer at addr " << next_page << "\n";


				// Remove the entry for the prior page.
				it = one_miss_table.erase(it);

				// Save the next page in the stream buffer table
				// This is the address we will detect on a hit to the buffer.
				stream_buffers[next_page] = currentClockCycle;
				unique_stream_buffers++;

				if (stream_buffers.size() > NUM_STREAM_BUFFERS)
				{
					// Evict stream buffer with oldest cycle.
					unordered_map<uint64_t, uint64_t>::iterator sb_it;
					uint64_t oldest_key = 0;
					uint64_t oldest_cycle = currentClockCycle;
					for (sb_it=stream_buffers.begin(); sb_it != stream_buffers.end(); sb_it++)
					{
						uint64_t cur_sb_cycle = (*sb_it).second;
						if (cur_sb_cycle < oldest_cycle)
						{
							oldest_key = (*sb_it).first;
							oldest_cycle = cur_sb_cycle;
						}
					}
					stream_buffers.erase(oldest_key);

					if (DEBUG_STREAM_BUFFER)
						cerr << currentClockCycle << " : Stream buffer evicted addr=" << oldest_key << "\n";
				}

				// Issue prefetches to start the stream.
				// Count down from the top address. This must be done because addPrefetch puts transactions at the front
				// of the queue and we want page_addr+PAGE_SIZE to be the first prefetch issued.
				for (int i=STREAM_BUFFER_LENGTH; i > 0; i--)
				{
					// Compute the next prefetch address.
					uint64_t prefetch_address = miss_page + (i * PAGE_SIZE);

					// If address is above the legal address space for the main memory, then do not issue this prefetch.
					if (prefetch_address >= (TOTAL_PAGES * PAGE_SIZE))
						continue;

					// Add the prefetch.
					addPrefetch(prefetch_address);
				}

				break;
			}
		}

		if (!stream_detected)
		{
			// Insert miss address into the one_miss_table.
			one_miss_table.push_back(make_pair(miss_page, currentClockCycle));
			unique_one_misses++;

			if (DEBUG_STREAM_BUFFER)
				cerr << currentClockCycle << " : One miss detected addr=" << miss_page << "\n";
		}

		// Enforce the size of the one miss table.
		if (one_miss_table.size() > ONE_MISS_TABLE_SIZE)
		{
			if (DEBUG_STREAM_BUFFER)
			{
				cerr << currentClockCycle << " : One miss evicted addr=" << one_miss_table.front().first << "\n";
			}

			one_miss_table.pop_front();
		}

	}

	void HybridSystem::stream_buffer_hit_handler(uint64_t hit_page)
	{
		if (stream_buffers.count(hit_page) == 1)
		{
			// Stream buffer hit!
			stream_buffer_hits++;

			uint64_t next_page = hit_page + PAGE_SIZE;
			uint64_t prefetch_address = hit_page + (STREAM_BUFFER_LENGTH * PAGE_SIZE);

			if ((DEBUG_STREAM_BUFFER==1) && (DEBUG_STREAM_BUFFER_HIT==1))
			{
				cerr << currentClockCycle << " : Stream Buffer Hit. hit_page=" << hit_page
					<< " prior cycle=" << stream_buffers[hit_page] << " next_page=" << next_page 
					<< " prefetch_addr=" << prefetch_address << "\n";
			}

			// Remove the current stream buffer entry and replace it with the next page.
			stream_buffers.erase(hit_page);

			// If the prefetch address is out of range, then do nothing.
			if (prefetch_address < (TOTAL_PAGES * PAGE_SIZE))
			{
				// If it is in range, add the prefetch and readd the stream buffer.
				addPrefetch(prefetch_address);
				stream_buffers[next_page] = currentClockCycle;
			}
		}

	}


// Extra functions for C interface (used by Python front end)
class HybridSim_C_Callbacks
{
	public:
	// Lists for tracking received callback data.
	list<uint> done_id;
	list<uint64_t> done_address;
	list<uint64_t> done_cycle;
	list<bool> done_isWrite;

	void read_complete(uint id, uint64_t address, uint64_t cycle)
	{
		done_id.push_back(id);
		done_address.push_back(address);
		done_cycle.push_back(cycle);
		done_isWrite.push_back(false);
	}

	void write_complete(uint id, uint64_t address, uint64_t cycle)
	{
		done_id.push_back(id);
		done_address.push_back(address);
		done_cycle.push_back(cycle);
		done_isWrite.push_back(true);
	}

	void register_cb(HybridSystem *hs)
	{
		typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
		Callback_t *read_cb = new Callback<HybridSim_C_Callbacks, void, uint, uint64_t, uint64_t>(this, &HybridSim_C_Callbacks::read_complete);
		Callback_t *write_cb = new Callback<HybridSim_C_Callbacks, void, uint, uint64_t, uint64_t>(this, &HybridSim_C_Callbacks::write_complete);
		hs->RegisterCallbacks(read_cb, write_cb);
	}

	bool get_next_result(uint *sysID, uint64_t *addr, uint64_t *cycle, bool *isWrite)
	{
		if (done_id.empty())
		{
			*sysID = 0;
			*addr = 0;
			*cycle = 0;
			*isWrite = false;

			return false;
		}
		else
		{
			// Set the result pointers.
			*sysID = done_id.front();
			*addr = done_address.front();
			*cycle = done_cycle.front();
			*isWrite = done_isWrite.front();

			// Pop the front of each list.
			done_id.pop_front();
			done_address.pop_front();
			done_cycle.pop_front();
			done_isWrite.pop_front();

			return true;
		}
	}
};
HybridSim_C_Callbacks c_callbacks;

extern "C"
{
	HybridSystem *HybridSim_C_getMemorySystemInstance(uint id, char *ini)
	{
		// Note ini is implicitly transformed to C++ string type.
		HybridSystem *hs = getMemorySystemInstance(id, ini);

		// Register callbacks to the HybridSim_C_callbacks object.
		c_callbacks.register_cb(hs);

		return hs;
	}

	bool HybridSim_C_addTransaction(HybridSystem *hs, bool isWrite, uint64_t addr)
	{
		//cout << "C interface... addr=" << addr << " isWrite=" << isWrite << "\n";
		return hs->addTransaction(isWrite, addr);
	}

	bool HybridSim_C_WillAcceptTransaction(HybridSystem *hs)
	{
		return hs->WillAcceptTransaction();
	}

	void HybridSim_C_update(HybridSystem *hs)
	{
		hs->update();
	}

	// use this instead of the callbacks since I can't do callbacks through the Python cdll interface
	// The protocol is to call this repetitively after each update until it returns false.
	// When it returns false, the sysID, addr, cycle, and isWrite are don't cares
	// When it returns true, the output values are set to the completed transaction
	bool HybridSim_C_PollCompletion(HybridSystem *hs, uint *sysID, uint64_t *addr, uint64_t *cycle, bool *isWrite)
	{
		return c_callbacks.get_next_result(sysID, addr, cycle, isWrite);
	}

	void HybridSim_C_mmio(HybridSystem *hs, uint64_t operation, uint64_t address)
	{
		hs->mmio(operation, address);
	}

	void HybridSim_C_syncAll(HybridSystem *hs)
	{
		hs->syncAll();
	}

	void HybridSim_C_reportPower(HybridSystem *hs)
	{
		hs->reportPower();
	}

	void HybridSim_C_printLogfile(HybridSystem *hs)
	{
		hs->printLogfile();
	}

}

} // Namespace HybridSim

// Extra function needed for Sandia SST.
extern "C"
{
    void libhybridsim_is_present(void)
    {
	;
    }
}



