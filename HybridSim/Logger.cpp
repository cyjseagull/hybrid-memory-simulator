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

#include "Logger.h"

using namespace std;

namespace HybridSim 
{
	Logger::Logger()
	{
	}

	Logger::~Logger()
	{
		if (DEBUG_LOGGER && debug.is_open()) 
			debug.close();
	}

	void Logger::init()
	{
		// Overall state
		num_accesses = 0;
		num_reads = 0;
		num_writes = 0;

		num_misses = 0;
		num_hits = 0;

		num_read_misses = 0;
		num_read_hits = 0;
		num_write_misses = 0;
		num_write_hits = 0;

		sum_latency = 0;
		sum_read_latency = 0;
		sum_write_latency = 0;
		sum_queue_latency = 0;
		sum_miss_latency = 0;
		sum_hit_latency = 0;

		sum_read_hit_latency = 0;
		sum_read_miss_latency = 0;

		sum_write_hit_latency = 0;
		sum_write_miss_latency = 0;

		max_queue_length = 0;
		sum_queue_length = 0;

		idle_counter = 0;
		flash_idle_counter = 0;
		dram_idle_counter = 0;

		num_mmio_dropped = 0;
		num_mmio_remapped = 0;


		// Init the latency histogram.
		for (uint64_t i = 0; i <= HISTOGRAM_MAX; i += HISTOGRAM_BIN)
		{
			latency_histogram[i] = 0;
		}

		// Init the set conflicts.
		for (uint64_t i = 0; i < NUM_SETS; i++)
		{
			set_conflicts[i] = 0;
		}

		// Resetting the epoch state will initialize it.
		epoch_count = 0;
		this->epoch_reset(true);

		if (DEBUG_LOGGER) 
		{
			debug.open("debug.log", ios_base::out | ios_base::trunc);
			if (!debug.is_open())
			{
				cerr << "ERROR: HybridSim Logger debug file failed to open.\n";
				abort();
			}
		}
	}


	void Logger::update()
	{
		// Every EPOCH_LENGTH cycles, reset the epoch state.
		if (this->currentClockCycle % EPOCH_LENGTH == 0)
			epoch_reset(false);

		// Increment to the next clock cycle.
		this->step();
	}

	//start access memory: insert request to access_queue
	void Logger::access_start(uint64_t addr)
	{
		access_queue.push_back(pair <uint64_t, uint64_t>(addr, currentClockCycle));

		if (DEBUG_LOGGER)
		{
			list<pair <uint64_t, uint64_t>>::iterator it = access_queue.begin();
			debug << "access_start( " << addr << " , " << currentClockCycle << " ) / aq: ( " << (*it).first << " , " << (*it).second << " )\n\n";
		}
	}


	/*
	 * @function: 1.get request whose access address is equal of addr 
	 *				from access_queue
	 *			  2.if find the request , insert request to access_map
	 *			  3. modify queue latency
	 */
	void Logger::access_process(uint64_t addr, bool read_op, bool hit)
	{
		if (DEBUG_LOGGER)
			debug << "access_process( " << addr << " , " << read_op << " )\n";

		// Get entry off of the access_queue.
		uint64_t start_cycle = 0;
		bool found = false;
		list<pair <uint64_t, uint64_t>>::iterator it;
		uint64_t counter = 0;
		//find request whose address is equal of inputed address
		for (it = access_queue.begin(); it != access_queue.end(); it++, counter++)
		{
			uint64_t cur_addr = (*it).first;
			uint64_t cur_cycle = (*it).second;

			if (DEBUG_LOGGER)
				debug << counter << " cur_addr = " << cur_addr << ", cur_cycle = " << cur_cycle << "\n";

			if (cur_addr == addr)
			{
				start_cycle = cur_cycle;
				found = true;
				//delete request from access_queue
				access_queue.erase(it);

				if (DEBUG_LOGGER)
					debug << "found match!\n";

				break;
			}
		}

		if (!found)
		{
			cerr << "ERROR: Logger.access_process() called with address not in the access_queue. address=0x" << hex << addr << "\n" << dec;
			abort();
		}

		if (access_map.count(addr) != 0)
		{
			cerr << "ERROR: Logger.access_process() called with address already in access_map. address=0x" << hex << addr << "\n" << dec;
			abort();
		}
		//insert this request to access_map
		AccessMapEntry a;
		a.start = start_cycle;
		a.read_op = read_op;
		a.hit = hit;
		a.process = this->currentClockCycle;
		access_map[addr] = a;

		//get queue time of request accessing addr
		uint64_t time_in_queue = a.process - a.start;
		this->queue_latency(time_in_queue);

		if (DEBUG_LOGGER)
			debug << "finished access_process. time_in_queue = " << time_in_queue << "\n\n";
	}

	void Logger::access_stop(uint64_t addr)
	{
		if (DEBUG_LOGGER)
			debug << "access_stop( " << addr << " )\n";

		if (access_map.count(addr) == 0)
		{
			cerr << "ERROR: Logger.access_stop() called with address not in access_map. address=" << hex << addr << "\n" << dec;
			abort();
		}
		//get access process "request" from access map , modify stop cycle
		AccessMapEntry a = access_map[addr];
		a.stop = this->currentClockCycle;
		access_map[addr] = a;
		//get latency
		uint64_t latency = a.stop - a.start;

		// Log cache event type and latency.
		// read hit
		if (a.read_op && a.hit)
		{
			this->read_hit();
			this->read_hit_latency(latency);
		}
		//read miss
		else if (a.read_op && !a.hit)
		{
			this->read_miss();
			this->read_miss_latency(latency);
		}
		//write hit
		else if (!a.read_op && a.hit)
		{
			this->write_hit();
			this->write_hit_latency(latency);
		}
		//write miss
		else if (!a.read_op && !a.hit)
		{
			this->write_miss();
			this->write_miss_latency(latency);
		}
		//delete request from access_map
		access_map.erase(addr);

		if (DEBUG_LOGGER)
			debug << "finished access_stop. latency = " << latency << "\n\n";
	}


	//
	void Logger::access_update(uint64_t queue_length, bool idle, bool flash_idle, bool dram_idle)
	{
		// Log the queue length.
		if (queue_length > max_queue_length)
			max_queue_length = queue_length;
		sum_queue_length += queue_length;

		// Log the queue length for the current epoch.
		if (queue_length > cur_max_queue_length)
			cur_max_queue_length = queue_length;
		cur_sum_queue_length += queue_length;

		//cerr << "access_queue length = " << access_queue.size() << "; queue_length = " << queue_length << ";\n";

		// Update idle counters.
		if (idle)
		{
			idle_counter++;
			cur_idle_counter++;
		}

		if (flash_idle)
		{
			flash_idle_counter++;
			cur_flash_idle_counter++;
		}

		if (dram_idle)
		{
			dram_idle_counter++;
			cur_dram_idle_counter++;
		}
	}

	//access page_addr 
	void Logger::access_page(uint64_t page_addr)
	{
		if (pages_used.count(page_addr) == 0)
		{
			// Create an entry for a page that has not been previously accessed.
			pages_used[page_addr] = 0;
		}

		// At this point, the invariant is that the page entry exists in pages_used
		// and is >= 0.

		// Load, increment, and store.
		uint64_t cur_count = pages_used[page_addr];
		cur_count += 1;
		pages_used[page_addr] = cur_count;

		// Now do the same for the cur_pages_used (which is for this epoch only).

		if (cur_pages_used.count(page_addr) == 0)
		{
			// Create an entry for a page that has not been previously accessed.
			cur_pages_used[page_addr] = 0;
		}

		// At this point, the invariant is that the page entry exists in pages_used
		// and is >= 0.

		// Increment.
		cur_count = cur_pages_used[page_addr];
		cur_count += 1;
		cur_pages_used[page_addr] = cur_count;
	}

	void Logger::access_set_conflict(uint64_t cache_set)
	{
		// Increment the conflict counter for this set.
		uint64_t tmp = set_conflicts[cache_set];
		set_conflicts[cache_set] = tmp + 1;
	}

	void Logger::access_miss(uint64_t missed_page, uint64_t victim_page, uint64_t cache_set, uint64_t cache_page, bool dirty, bool valid)
	{
		MissedPageEntry m(currentClockCycle, missed_page, victim_page, cache_set, cache_page, dirty, valid);
		
		missed_page_list.push_back(m);
	}


	void Logger::mmio_dropped()
	{
		num_mmio_dropped++;
		cur_num_mmio_dropped++;
	}

	void Logger::mmio_remapped()
	{
		num_mmio_remapped++;
		cur_num_mmio_remapped++;
	}


	void Logger::read()
	{
		num_accesses += 1;
		num_reads += 1;

		cur_num_accesses += 1;
		cur_num_reads += 1;
	}

	void Logger::write()
	{
		num_accesses += 1;
		num_writes += 1;

		cur_num_accesses += 1;
		cur_num_writes += 1;
	}


	void Logger::hit()
	{
		num_hits += 1;

		cur_num_hits += 1;
	}

	void Logger::miss()
	{
		num_misses += 1;

		cur_num_misses += 1;
	}

	void Logger::read_hit()
	{
		read();
		hit();
		num_read_hits += 1;

		cur_num_read_hits += 1;
	}

	void Logger::read_miss()
	{
		read();
		miss();
		num_read_misses += 1;

		cur_num_read_misses += 1;
	}

	void Logger::write_hit()
	{
		write();
		hit();
		num_write_hits += 1;

		cur_num_write_hits += 1;
	}

	void Logger::write_miss()
	{
		write();
		miss();
		num_write_misses += 1;

		cur_num_write_misses += 1;
	}

	//computer new average value according to input values
	double Logger::compute_running_average(double old_average, double num_values, double new_value)
	{
		return ((old_average * (num_values - 1)) + (new_value)) / num_values;
	}

	void Logger::latency(uint64_t cycles)
	{
		//average_latency = compute_running_average(average_latency, num_accesses, cycles);
		sum_latency += cycles;
		cur_sum_latency += cycles;
		// Update the latency histogram.
		uint64_t bin = (cycles / HISTOGRAM_BIN) * HISTOGRAM_BIN;
		if (cycles >= HISTOGRAM_MAX)
			bin = HISTOGRAM_MAX;
		uint64_t bin_cnt = latency_histogram[bin];
		latency_histogram[bin] = bin_cnt + 1;
	}

	void Logger::read_latency(uint64_t cycles)
	{
		this->latency(cycles);
		sum_read_latency += cycles;

		cur_sum_read_latency += cycles;
	}

	void Logger::write_latency(uint64_t cycles)
	{
		this->latency(cycles);
		sum_write_latency += cycles;

		cur_sum_write_latency += cycles;
	}

	void Logger::queue_latency(uint64_t cycles)
	{
		sum_queue_latency += cycles;

		cur_sum_queue_latency += cycles;
	}

	void Logger::hit_latency(uint64_t cycles)
	{
		sum_hit_latency += cycles;

		cur_sum_hit_latency += cycles;
	}

	void Logger::miss_latency(uint64_t cycles)
	{
		sum_miss_latency += cycles;

		cur_sum_miss_latency += cycles;
	}

	void Logger::read_hit_latency(uint64_t cycles)
	{
		this->read_latency(cycles);
		this->hit_latency(cycles);
		sum_read_hit_latency += cycles;

		cur_sum_read_hit_latency += cycles;
	}

	void Logger::read_miss_latency(uint64_t cycles)
	{
		this->read_latency(cycles);
		this->miss_latency(cycles);
		sum_read_miss_latency += cycles;

		cur_sum_read_miss_latency += cycles;
	}

	void Logger::write_hit_latency(uint64_t cycles)
	{
		this->write_latency(cycles);
		this->hit_latency(cycles);
		sum_write_hit_latency += cycles;

		cur_sum_write_hit_latency += cycles;
	}

	void Logger::write_miss_latency(uint64_t cycles)
	{
		this->write_latency(cycles);
		this->miss_latency(cycles);
		sum_write_miss_latency += cycles;

		cur_sum_write_miss_latency += cycles;
	}

	double Logger::divide(uint64_t a, uint64_t b)
	{
		// This is a division routine to prevent floating point exceptions if the denominator is 0.
		if (b == 0)
			return 0.0;
		else
			return (double)a / (double)b;
	}

	double Logger::miss_rate()
	{
		return this->divide(num_misses, num_accesses);
	}

	double Logger::read_miss_rate()
	{
		return this->divide(num_read_misses, num_reads);
	}

	double Logger::write_miss_rate()
	{
		return this->divide(num_write_misses, num_writes);
	}

	double Logger::compute_throughput(uint64_t cycles, uint64_t accesses)
	{
		// Calculate the throughput in kilobytes per second.
		return ((this->divide(accesses, cycles) * CYCLES_PER_SECOND) * BURST_SIZE) / 1024.0;
	}

	double Logger::latency_cycles(uint64_t sum, uint64_t accesses)
	{
		return this->divide(sum, accesses);
	}
	//transfer "us" to cycles
	double Logger::latency_us(uint64_t sum, uint64_t accesses)
	{
		// Calculate the average latency in microseconds.
		return (this->divide(sum, accesses) / CYCLES_PER_SECOND) * 1000000;
	}


	void Logger::epoch_reset(bool init)
	{
		// If this is not initialization, then save the epoch state to the lists.
		if (init)
		{
			// Open up the hybridsim_epoch.log
			ofstream savefile;
			savefile.open("hybridsim_epoch.log", ios_base::out | ios_base::trunc);
			if (!savefile.is_open())
			{
				cerr << "ERROR: HybridSim Logger epoch output file failed to open.\n";
				abort();
			}

			savefile << "================================================================================\n\n";
			savefile << "Epoch data:\n\n";

			savefile.close();
		}

		if (!init)
		{
			// Open up the hybridsim_epoch.log
			ofstream savefile;
			savefile.open("hybridsim_epoch.log", ios_base::out | ios_base::app);
			if (!savefile.is_open())
			{
				cerr << "ERROR: HybridSim Logger epoch output file failed to open.\n";
				abort();
			}

			// Output the current epoch data.
			savefile << "---------------------------------------------------\n";
			savefile << "Epoch number: " << epoch_count << "\n";

			// Print everything out.
			savefile << "total accesses: " << cur_num_accesses << "\n";
			savefile << "cycles: " << EPOCH_LENGTH << "\n";
			savefile << "execution time: " << (EPOCH_LENGTH / (double)CYCLES_PER_SECOND) * 1000000 << " us\n";
			savefile << "misses: " << cur_num_misses << "\n";
			savefile << "hits: " << cur_num_hits << "\n";
			savefile << "miss rate: " << this->divide(cur_num_misses, cur_num_accesses) << "\n";
			savefile << "average latency: " << this->latency_cycles(cur_sum_latency, cur_num_accesses) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_latency, cur_num_accesses) << " us)\n";
			savefile << "average queue latency: " << this->latency_cycles(cur_sum_queue_latency, cur_num_accesses) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_queue_latency, cur_num_accesses) << " us)\n";
			savefile << "average miss latency: " << this->latency_cycles(cur_sum_miss_latency, cur_num_misses) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_miss_latency, cur_num_misses) << " us)\n";
			savefile << "average hit latency: " << this->latency_cycles(cur_sum_hit_latency, cur_num_hits) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_hit_latency, cur_num_hits) << " us)\n";
			savefile << "throughput: " << this->compute_throughput(EPOCH_LENGTH, cur_num_accesses) << " KB/s\n";
			savefile << "working set size in pages: " << cur_pages_used.size() << "\n";
			savefile << "working set size in bytes: " << cur_pages_used.size() * PAGE_SIZE << " bytes\n";
			savefile << "current queue length: " << access_queue.size() << "\n";
			savefile << "max queue length: " << cur_max_queue_length << "\n";
			savefile << "average queue length: " << this->divide(cur_sum_queue_length, EPOCH_LENGTH) << "\n";
			savefile << "idle counter: " << cur_idle_counter << "\n";
			savefile << "idle percentage: " << this->divide(cur_idle_counter, EPOCH_LENGTH) << "\n";
			savefile << "flash idle counter: " << cur_flash_idle_counter << "\n";
			savefile << "flash idle percentage: " << this->divide(cur_flash_idle_counter, EPOCH_LENGTH) << "\n";
			savefile << "dram idle counter: " << cur_dram_idle_counter << "\n";
			savefile << "dram idle percentage: " << this->divide(cur_dram_idle_counter, EPOCH_LENGTH) << "\n";
			savefile << "MMIO Accesses Dropped: " << cur_num_mmio_dropped << "\n";
			savefile << "MMIO Accesses Remapped: " << cur_num_mmio_remapped << "\n";
			savefile << "\n";

			savefile << "reads: " << cur_num_reads << "\n";
			savefile << "misses: " << cur_num_read_misses << "\n";
			savefile << "hits: " << cur_num_read_hits << "\n";
			savefile << "miss rate: " << this->divide(cur_num_read_misses, cur_num_reads) << "\n";
			savefile << "average latency: " << this->latency_cycles(cur_sum_read_latency, cur_num_reads) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_read_latency, cur_num_reads) << " us)\n";
			savefile << "average miss latency: " << this->latency_cycles(cur_sum_read_miss_latency, cur_num_read_misses) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_read_miss_latency, cur_num_read_misses) << " us)\n";
			savefile << "average hit latency: " << this->latency_cycles(cur_sum_read_hit_latency, cur_num_read_hits) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_read_hit_latency, cur_num_read_hits) << " us)\n";
			savefile << "throughput: " << this->compute_throughput(EPOCH_LENGTH, cur_num_reads) << " KB/s\n";
			savefile << "\n";

			savefile << "writes: " << cur_num_writes << "\n";
			savefile << "misses: " << cur_num_write_misses << "\n";
			savefile << "hits: " << cur_num_write_hits << "\n";
			savefile << "miss rate: " << this->divide(cur_num_write_misses, cur_num_writes) << "\n";
			savefile << "average latency: " << this->latency_cycles(cur_sum_write_latency, cur_num_writes) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_write_latency, cur_num_writes) << " us)\n";
			savefile << "average miss latency: " << this->latency_cycles(cur_sum_write_miss_latency, cur_num_write_misses) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_write_miss_latency, cur_num_write_misses) << " us)\n";
			savefile << "average hit latency: " << this->latency_cycles(cur_sum_write_hit_latency, cur_num_write_hits) << " cycles";
			savefile << " (" << this->latency_us(cur_sum_write_hit_latency, cur_num_write_hits) << " us)\n";
			savefile << "throughput: " << this->compute_throughput(EPOCH_LENGTH, cur_num_writes) << " KB/s\n";
			savefile << "\n\n";

			// Output the missed page data.
			savefile << "Missed Page Data:\n";

			list<MissedPageEntry>::iterator mit;
			for (mit = missed_page_list.begin(); mit != missed_page_list.end(); mit++)
			{
				uint64_t cycle = (*mit).cycle;
				uint64_t missed_page = (*mit).missed_page;
				uint64_t victim_page = (*mit).victim_page;
				uint64_t cache_set = (*mit).cache_set;
				uint64_t cache_page = (*mit).cache_page;
				bool dirty = (*mit).dirty;
				bool valid = (*mit).valid;

				savefile << cycle << ": missed= 0x" << hex << missed_page << "; victim= 0x" << victim_page 
						<< "; set= " << dec << cache_set << "; missed_tag= " << TAG(missed_page) << "; victim_tag= " << TAG(victim_page)
						<< "; cache_page= 0x" << hex << cache_page << dec <<"; dirty = " << dirty
						<< "; valid= " << valid << ";\n";
			}

			savefile << "\n\n";

			// Clear the missed page data.
			missed_page_list.clear();
			


			// Close the output file.
			savefile.close();

			epoch_count++;
		}

		// Reset epoch state
		cur_num_accesses = 0;
		cur_num_reads = 0;
		cur_num_writes = 0;

		cur_num_misses = 0;
		cur_num_hits = 0;

		cur_num_read_misses = 0;
		cur_num_read_hits = 0;
		cur_num_write_misses = 0;
		cur_num_write_hits = 0;

		cur_sum_latency = 0;
		cur_sum_read_latency = 0;
		cur_sum_write_latency = 0;
		cur_sum_queue_latency = 0;
		cur_sum_miss_latency = 0;
		cur_sum_hit_latency = 0;

		cur_sum_read_hit_latency = 0;
		cur_sum_read_miss_latency = 0;

		cur_sum_write_hit_latency = 0;
		cur_sum_write_miss_latency = 0;

		cur_max_queue_length = 0;
		cur_sum_queue_length = 0;

		cur_idle_counter = 0;
		cur_flash_idle_counter = 0;
		cur_dram_idle_counter = 0;

		cur_num_mmio_dropped = 0;
		cur_num_mmio_remapped = 0;

		// Clear cur_pages_used
		cur_pages_used.clear();
	}

	void Logger::print()
	{
		ofstream savefile;
		savefile.open("hybridsim.log", ios_base::out | ios_base::trunc);
		if (!savefile.is_open())
		{
			cerr << "ERROR: HybridSim Logger output file failed to open.\n";
			abort();
		}

		savefile << "total accesses: " << num_accesses << "\n";
		savefile << "cycles: " << this->currentClockCycle << "\n";
		savefile << "execution time: " << (this->currentClockCycle / (double)CYCLES_PER_SECOND) * 1000000 << " us\n";
		savefile << "frequency: " << CYCLES_PER_SECOND << "\n";
		savefile << "misses: " << num_misses << "\n";
		savefile << "hits: " << num_hits << "\n";
		savefile << "miss rate: " << miss_rate() << "\n";
		savefile << "average latency: " << this->latency_cycles(sum_latency, num_accesses) << " cycles";
		savefile << " (" << this->latency_us(sum_latency, num_accesses) << " us)\n";
		savefile << "average queue latency: " << this->latency_cycles(sum_queue_latency, num_accesses) << " cycles";
		savefile << " (" << this->latency_us(sum_queue_latency, num_accesses) << " us)\n";
		savefile << "average miss latency: " << this->latency_cycles(sum_miss_latency, num_misses) << " cycles";
		savefile << " (" << this->latency_us(sum_miss_latency, num_misses) << " us)\n";
		savefile << "average hit latency: " << this->latency_cycles(sum_hit_latency, num_hits) << " cycles";
		savefile << " (" << this->latency_us(sum_hit_latency, num_hits) << " us)\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_accesses) << " KB/s\n";
		savefile << "working set size in pages: " << pages_used.size() << "\n";
		savefile << "working set size in bytes: " << pages_used.size() * PAGE_SIZE << " bytes\n";
		savefile << "page size: " << PAGE_SIZE << "\n";
		savefile << "max queue length: " << max_queue_length << "\n";
		savefile << "average queue length: " << this->divide(sum_queue_length, this->currentClockCycle) << "\n";
		savefile << "idle counter: " << idle_counter << "\n";
		savefile << "idle percentage: " << this->divide(idle_counter, currentClockCycle) << "\n";
		savefile << "flash idle counter: " << flash_idle_counter << "\n";
		savefile << "flash idle percentage: " << this->divide(flash_idle_counter, currentClockCycle) << "\n";
		savefile << "dram idle counter: " << dram_idle_counter << "\n";
		savefile << "dram idle percentage: " << this->divide(dram_idle_counter, currentClockCycle) << "\n";
		savefile << "MMIO Accesses Dropped: " << num_mmio_dropped << "\n";
		savefile << "MMIO Accesses Remapped: " << num_mmio_remapped << "\n";
		savefile << "\n";

		savefile << "reads: " << num_reads << "\n";
		savefile << "misses: " << num_read_misses << "\n";
		savefile << "hits: " << num_read_hits << "\n";
		savefile << "miss rate: " << read_miss_rate() << "\n";
		savefile << "average latency: " << this->latency_cycles(sum_read_latency, num_reads) << " cycles";
		savefile << " (" << this->latency_us(sum_read_latency, num_reads) << " us)\n";
		savefile << "average miss latency: " << this->latency_cycles(sum_read_miss_latency, num_read_misses) << " cycles";
		savefile << " (" << this->latency_us(sum_read_miss_latency, num_read_misses) << " us)\n";
		savefile << "average hit latency: " << this->latency_cycles(sum_read_hit_latency, num_read_hits) << " cycles";
		savefile << " (" << this->latency_us(sum_read_hit_latency, num_read_hits) << " us)\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_reads) << " KB/s\n";
		savefile << "\n";

		savefile << "writes: " << num_writes << "\n";
		savefile << "misses: " << num_write_misses << "\n";
		savefile << "hits: " << num_write_hits << "\n";
		savefile << "miss rate: " << write_miss_rate() << "\n";
		savefile << "average latency: " << this->latency_cycles(sum_write_latency, num_writes) << " cycles";
		savefile << " (" << this->latency_us(sum_write_latency, num_writes) << " us)\n";
		savefile << "average miss latency: " << this->latency_cycles(sum_write_miss_latency, num_write_misses) << " cycles";
		savefile << " (" << this->latency_us(sum_write_miss_latency, num_write_misses) << " us)\n";
		savefile << "average hit latency: " << this->latency_cycles(sum_write_hit_latency, num_write_hits) << " cycles";
		savefile << " (" << this->latency_us(sum_write_hit_latency, num_write_hits) << " us)\n";
		savefile << "throughput: " << this->compute_throughput(this->currentClockCycle, num_writes) << " KB/s\n";
		savefile << "\n\n";


		savefile << "================================================================================\n\n";
		savefile << "Pages accessed:\n";

		savefile << flush;

		unordered_map<uint64_t, uint64_t>::iterator it; 
		for (it = pages_used.begin(); it != pages_used.end(); it++)
		{
			uint64_t page_addr = (*it).first;
			uint64_t num_accesses = (*it).second;
			savefile << hex << "0x" << page_addr << " : " << dec << num_accesses << "\n";
		}

		savefile << "\n\n";

		savefile << "================================================================================\n\n";
		savefile << "Latency Histogram:\n\n";

		savefile << "HISTOGRAM_BIN: " << HISTOGRAM_BIN << "\n";
		savefile << "HISTOGRAM_MAX: " << HISTOGRAM_MAX << "\n\n";
		for (uint64_t bin = 0; bin <= HISTOGRAM_MAX; bin += HISTOGRAM_BIN)
		{
			savefile << bin << ": " << latency_histogram[bin] << "\n";
		}

		savefile << "\n\n";

		savefile << "================================================================================\n\n";
		savefile << "Set Conflicts:\n\n";

		for (uint64_t set = 0; set < NUM_SETS; set++)
		{
			// Only print the sets that have greater than 0 conflicts.
			if (set_conflicts[set])
				savefile << set << ": " << set_conflicts[set] << "\n";
		}

		savefile.close();
	}
}

