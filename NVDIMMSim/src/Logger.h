/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

#ifndef NVLOGGER_H
#define NVLOGGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <list>

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"

namespace NVDSim
{
    enum PlaneStateType{
	IDLE,
	READING,
	GC_READING,
	WRITING,
	GC_WRITING,
	ERASING
    };
    
    class Logger: public SimObj
    {
    public:
	Logger();

	// extended logging options
	void log_ftl_queue_event(bool write, std::list<FlashTransaction> *queue);
	void log_ctrl_queue_event(bool write, uint64_t number, std::list<ChannelPacket*> *queue);
	void log_plane_state(uint64_t address, uint64_t package, uint64_t die, uint64_t plane, PlaneStateType op);
	
	// operations
	void read();
	void write();
	void mapped();
	void unmapped();
	void locked_up(uint64_t cycle);
	void unlocked_up(uint64_t count);

	void read_mapped();
	void read_unmapped();
	void write_mapped();
	void write_unmapped();
	void forced_write();
	void idle_write(); // to determine how many times we're issuing writes when we don't have a read

	void read_latency(uint64_t cycles);
	void write_latency(uint64_t cycles);
	void queue_latency(uint64_t cycles);

	double unmapped_rate();
	double read_unmapped_rate();
	double write_unmapped_rate();
	double calc_throughput(uint64_t cycles, uint64_t accesses);

	double divide(double num, double denom);

	void ftlQueueLength(uint64_t length);
	virtual void ftlQueueLength(uint64_t length, uint64_t length2);
	void ctrlQueueLength(std::vector<std::vector<uint64_t> > length);
	void ctrlQueueSingleLength(uint64_t package, uint64_t die, uint64_t length);

	virtual void ftlQueueReset();
	void ctrlQueueReset();

	//Accessor for power data
	//Writing correct object oriented code up in this piece, what now?
	virtual std::vector<std::vector<double>> getEnergyData(void);
	
	virtual void save(uint64_t cycle, uint64_t epoch);
	virtual void print(uint64_t cycle);

	virtual void update();
	
	void access_start(uint64_t addr);
	// overloaded access start for perfect scheduling analysis
	void access_start(uint64_t addr, TransactionType op);
	void access_process(uint64_t addr, uint64_t paddr, uint64_t package, ChannelPacketType op);
	virtual void access_stop(uint64_t addr, uint64_t paddr);

	virtual void save_epoch(uint64_t cycle, uint64_t epoch);
	
	// State
	std::ofstream savefile;

	uint64_t num_accesses;
	uint64_t num_reads;
	uint64_t num_writes;
	uint64_t num_idle_writes;
	uint64_t num_forced;

	uint64_t num_locks;
	uint64_t time_locked;
	uint64_t lock_start;

	uint64_t num_unmapped;
	uint64_t num_mapped;

	uint64_t num_read_unmapped;
	uint64_t num_read_mapped;
	uint64_t num_write_unmapped;
	uint64_t num_write_mapped;
		
	uint64_t average_latency;
	uint64_t average_read_latency;
	uint64_t average_write_latency;
	uint64_t average_queue_latency;

	uint64_t ftl_queue_length;
	std::vector<std::vector <uint64_t> > ctrl_queue_length;

	uint64_t max_ftl_queue_length;
	std::vector<std::vector <uint64_t> > max_ctrl_queue_length;

	std::unordered_map<uint64_t, uint64_t> writes_per_address;

	// Extended logging state
	bool first_write_log;
	bool first_read_log;
	bool first_ftl_read_log;
	bool first_ftl_write_log;
	bool* first_ctrl_read_log;
	bool* first_crtl_write_log;
	bool first_state_log;
	PlaneStateType*** plane_states;

	// Power Stuff
	// This is computed per package
	std::vector<double> idle_energy;
	std::vector<double> access_energy;


	class AccessMapEntry
	{
		public:
		uint64_t start; // Starting cycle of access
		uint64_t process; // Cycle when processing starts
		uint64_t stop; // Stopping cycle of access
		uint64_t pAddr; // Virtual address of access
		uint64_t package; // package for the power calculations
		ChannelPacketType op; // what operation is this?
		AccessMapEntry()
		{
			start = 0;
			process = 0;
			stop = 0;
			pAddr = 0;
			package = 0;
			op = READ;
		}
	};

	// Store access info while the access is being processed.
	std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::list<AccessMapEntry>>> access_map;

	// Store the address and arrival time while access is waiting to be processed.
	// Must do this because duplicate addresses may arrive close together.
	std::list<std::pair <uint64_t, uint64_t>> access_queue;

	class EpochEntry
	{
	public:
	    uint64_t cycle;
	    uint64_t epoch;

	    uint64_t num_accesses;
	    uint64_t num_reads;
	    uint64_t num_writes;

	    uint64_t num_unmapped;
	    uint64_t num_mapped;

	    uint64_t num_read_unmapped;
	    uint64_t num_read_mapped;
	    uint64_t num_write_unmapped;
	    uint64_t num_write_mapped;
		
	    uint64_t average_latency;
	    uint64_t average_read_latency;
	    uint64_t average_write_latency;
	    uint64_t average_queue_latency;

	    uint64_t ftl_queue_length;
	    std::vector<std::vector<uint64_t> > ctrl_queue_length;
	    
	    std::unordered_map<uint64_t, uint64_t> writes_per_address;

	    std::vector<double> idle_energy;
	    std::vector<double> access_energy;

	    EpochEntry()
	    {
		cycle = 0;
		epoch = 0;

		num_accesses = 0;
		num_reads = 0;
		num_writes = 0;
	
		num_unmapped = 0;
		num_mapped = 0;

		num_read_unmapped = 0;
		num_read_mapped = 0;
		num_write_unmapped = 0;
		num_write_mapped = 0;
		
		average_latency = 0;
		average_read_latency = 0;
		average_write_latency = 0;
		average_queue_latency = 0;
		
		ftl_queue_length = 0;
		ctrl_queue_length = std::vector<std::vector<uint64_t> >(NUM_PACKAGES, std::vector<uint64_t>(DIES_PER_PACKAGE, 0));
	
		idle_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
		access_energy = std::vector<double>(NUM_PACKAGES, 0.0);
	    }
	};

	// Store system snapshot from last epoch to compute this epoch
	EpochEntry last_epoch;

	// Store the data from each epoch for printing at the end of the simulation
	std::list<EpochEntry> epoch_queue;

	virtual void write_epoch(EpochEntry *e);
    };
}

#endif
