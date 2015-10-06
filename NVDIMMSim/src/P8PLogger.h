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
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. *********************************************************************************/

#ifndef NVP8PLOGGER_H
#define NVP8PLOGGER_H

#include <iostream>
#include <fstream>
#include <vector>

#include "FlashConfiguration.h"
#include "Logger.h"

namespace NVDSim
{
    class P8PLogger: public Logger
    {
    public:
	P8PLogger();
	
	void save(uint64_t cycle, uint64_t epoch);
	void print(uint64_t cycle);

	void update();

	void access_stop(uint64_t addr, uint64_t paddr);

	//Accessors for power data
	//Writing correct object oriented code up in this piece, what now?
	std::vector<std::vector<double>> getEnergyData(void);

	void save_epoch(uint64_t cycle, uint64_t epoch);

	// State
	// Power Stuff
	// This is computed per package
	std::vector<double> vpp_idle_energy;
	std::vector<double> vpp_access_energy;

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
	    std::vector<std::vector <uint64_t> > ctrl_queue_length;

	    std::unordered_map<uint64_t, uint64_t> writes_per_address;

	    std::vector<double> idle_energy;
	    std::vector<double> access_energy;

	    std::vector<double> vpp_idle_energy;
	    std::vector<double> vpp_access_energy;

	    EpochEntry()
	    {
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

		vpp_idle_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
		vpp_access_energy = std::vector<double>(NUM_PACKAGES, 0.0); 
	    }
	};

	// Store system snapshot from last epoch to compute this epoch
	EpochEntry last_epoch;

	// Store the data from each epoch for printing at the end of the simulation
	std::list<EpochEntry> epoch_queue;

	using Logger::write_epoch; // This is to make Clang happy since Logger::EpochEntry is different from P8PLogger::EpochEntry.
	virtual void write_epoch(EpochEntry *e);
    };
}

#endif
