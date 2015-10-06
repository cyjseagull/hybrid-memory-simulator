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




#include "TraceBasedSim.h"

using namespace HybridSim;
using namespace std;

const uint64_t MAX_PENDING = 36;
const uint64_t MIN_PENDING = 35;
uint64_t complete = 0;
uint64_t pending = 0;
uint64_t throttle_count = 0;
uint64_t throttle_cycles = 0;
uint64_t final_cycles = 0;

// The cycle counter is used to keep track of what cycle we are on.
uint64_t trace_cycles = 0;

uint64_t last_clock = 0;
uint64_t CLOCK_DELAY = 1000000;


int main(int argc, char *argv[])
{
	printf("hybridsim_test main()\n");
	HybridSimTBS obj;

	string tracefile = "traces/test.txt";
	if (argc > 1)
	{
		tracefile = argv[1];
		cout << "Using trace file " << tracefile << "\n";
	}
	else
	{
		cout << "Using default trace file (traces/test.txt)\n";
	}

	obj.run_trace(tracefile);
}

void transaction_complete(uint64_t clock_cycle)
{
	complete++;
	pending--;

	if ((complete % 10000 == 0) || (clock_cycle - last_clock > CLOCK_DELAY))
	{
		cout << "complete= " << complete << "\t\tpending= " << pending << "\t\t cycle_count= "<< clock_cycle << "\t\tthrottle_count=" << throttle_count << "\n";
		last_clock = clock_cycle;
	}

	//if (complete == 10000000)
	//	abort();
}

void HybridSimTBS::read_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	//printf("[Callback] read complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	//complete++;
	//pending--;

	transaction_complete(clock_cycle);
}

void HybridSimTBS::write_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	//printf("[Callback] write complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	//complete++;
	//pending--;

	transaction_complete(clock_cycle);
}

int HybridSimTBS::run_trace(string tracefile)
{
	HybridSystem *mem = new HybridSystem(1, "");


	/* create and register our callback functions */
	typedef CallbackBase<void,uint,uint64_t,uint64_t> Callback_t;
	Callback_t *read_cb = new Callback<HybridSimTBS, void, uint, uint64_t, uint64_t>(this, &HybridSimTBS::read_complete);
	Callback_t *write_cb = new Callback<HybridSimTBS, void, uint, uint64_t, uint64_t>(this, &HybridSimTBS::write_complete);
	mem->RegisterCallbacks(read_cb, write_cb);

	// Open input file
	ifstream inFile;
	inFile.open(tracefile, ifstream::in);
	if (!inFile.is_open())
	{
		cout << "ERROR: Failed to load tracefile: " << tracefile << "\n";
		abort();
	}
	

	char char_line[256];
	string line;

	while (inFile.good())
	{
		// Read the next line.
		inFile.getline(char_line, 256);
		line = (string)char_line;

		// Filter comments out.
		size_t pos = line.find("#");
		line = line.substr(0, pos);

		// Strip whitespace from the ends.
		line = strip(line);

		// Filter newlines out.
		if (line.empty())
			continue;

		// Split and parse.
		list<string> split_line = split(line);

		if (split_line.size() != 3)
		{
			cout << "ERROR: Parsing trace failed on line:\n" << line << "\n";
			cout << "There should be exactly three numbers per line\n";
			cout << "There are " << split_line.size() << endl;
			abort();
		}

		uint64_t line_vals[3];

		int i = 0;
		for (list<string>::iterator it = split_line.begin(); it != split_line.end(); it++, i++)
		{
			// convert string to integer
			uint64_t tmp;
			convert_uint64_t(tmp, (*it));
			line_vals[i] = tmp;
		}

		// Finish parsing.
		uint64_t trans_cycle = line_vals[0];
		bool write = line_vals[1] % 2;
		uint64_t addr = line_vals[2];

		// increment the counter until >= the clock cycle of cur transaction
		// for each cycle, call the update() function.
		while (trace_cycles < trans_cycle)
		{
			mem->update();
			trace_cycles++;
		}

		// add the transaction and continue
		mem->addTransaction(write, addr);
		pending++;

		// If the pending count goes above MAX_PENDING, wait until it goes back below MIN_PENDING before adding more 
		// transactions. This throttling will prevent the memory system from getting overloaded.
		if (pending >= MAX_PENDING)
		{
			//cout << "MAX_PENDING REACHED! Throttling the trace until pending is back below MIN_PENDING.\t\tcycle= " << trace_cycles << "\n";
			throttle_count++;
			while (pending > MIN_PENDING)
			{
				mem->update();
				throttle_cycles++;
			}
			//cout << "Back to MIN_PENDING. Allowing transactions to be added again.\t\tcycle= " << trace_cycles << "\n";
		}

	}

	inFile.close();


	//mem->syncAll();


	// Run update until all transactions come back.
	while (pending > 0)
	{
		mem->update();
		final_cycles++;
	}

	// This is a hack for the moment to ensure that a final write completes.
	// In the future, we need two callbacks to fix this.
	// This is not counted towards the cycle counts for the run though.
	for (int i=0; i<1000000; i++)
		mem->update();


	cout << "\n\n" << mem->currentClockCycle << ": completed " << complete << "\n\n";
	cout << "dram_pending=" << mem->dram_pending.size() << " flash_pending=" << mem->flash_pending.size() << "\n\n";
	cout << "dram_queue=" << mem->dram_queue.size() << " flash_queue=" << mem->flash_queue.size() << "\n\n";
	cout << "pending_pages=" << mem->pending_pages.size() << "\n\n";
	for (unordered_map<uint64_t, uint64_t>::iterator it = mem->pending_pages.begin(); it != mem->pending_pages.end(); it++)
	{
		cout << (*it).first << " ";
	}
	cout << "\n\n";
	cout << "pending_count=" << mem->pending_count << "\n\n";
	cout << "dram_pending_set.size() =" << mem->dram_pending_set.size() << "\n\n";
	cout << "dram_bad_address.size() = " << mem->dram_bad_address.size() << "\n";
	for (list<uint64_t>::iterator it = mem->dram_bad_address.begin(); it != mem->dram_bad_address.end(); it++)
	{
		cout << (*it) << " ";
	}
	cout << "\n\n";
	cout << "pending_pages_max = " << mem->pending_pages_max << "\n\n";
	cout << "trans_queue_max = " << mem->trans_queue_max << "\n\n";

	cout << "trace_cycles = " << trace_cycles << "\n";
	cout << "throttle_count = " << throttle_count << "\n";
	cout << "throttle_cycles = " << throttle_cycles << "\n";
	cout << "final_cycles = " << final_cycles << "\n";
	cout << "total_cycles = trace_cycles + throttle_cycles + final_cycles = " << trace_cycles + throttle_cycles + final_cycles << "\n\n";
	
	mem->printLogfile();

	return 0;
}

