/****************************************************************************
*	 HybridSim: Simulator for hybrid main memories
*	 
*	 Copyright (C) 2010   	Jim Stevens
* 							Peter Enns
*							Paul Tschirhart
*							Ishwar Bhati
*							Mutien Chang
*							Bruce Jacob
*							University of Maryland
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU General Public License as published by
*	 the Free Software Foundation, either version 3 of the License, or
*	 (at your option) any later version.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU General Public License for more details.
*
*	 You should have received a copy of the GNU General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/



#include "TraceBasedSim.h"

using namespace HybridSim;
using namespace std;

const uint64_t MAX_PENDING = 3000;
//const uint64_t MAX_PENDING = 1000000000;
const uint64_t MIN_PENDING = 2500;
uint64_t complete = 0;
uint64_t pending = 0;
uint64_t throttle_count = 0;

// The cycle counter is used to keep track of what cycle we are on.
uint64_t cycle_counter = 0;

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
	cout << "Using default trace file (traces/test.txt)\n";

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
	HybridSystem *mem = new HybridSystem(1);


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
		while (cycle_counter < trans_cycle)
		{
			mem->update();
			cycle_counter++;
		}

		// add the transaction and continue
		mem->addTransaction(write, addr);
		pending++;

		// If the pending count goes above MAX_PENDING, wait until it goes back below MIN_PENDING before adding more 
		// transactions. This throttling will prevent the memory system from getting overloaded.
		if (pending >= MAX_PENDING)
		{
			//cout << "MAX_PENDING REACHED! Throttling the trace until pending is back below MIN_PENDING.\t\tcycle= " << cycle_counter << "\n";
			throttle_count++;
			while (pending > MIN_PENDING)
			{
				mem->update();
				cycle_counter++;
			}
			//cout << "Back to MIN_PENDING. Allowing transactions to be added again.\t\tcycle= " << cycle_counter << "\n";
		}

	}

	inFile.close();

	// Run update until all transactions come back.
	while (pending > 0)
	{
		mem->update();
		cycle_counter++;
	}

	// This is a hack for the moment to ensure that a final write completes.
	// In the future, we need two callbacks to fix this.
	for (int i=0; i<1000000; i++)
		mem->update();


	cout << "\n\n" << mem->currentClockCycle << ": completed " << complete << "\n\n";
	cout << "dram_pending=" << mem->dram_pending.size() << " flash_pending=" << mem->flash_pending.size() << "\n\n";
	cout << "dram_queue=" << mem->dram_queue.size() << " flash_queue=" << mem->flash_queue.size() << "\n\n";
	cout << "pending_pages=" << mem->pending_pages.size() << "\n\n";
	for (set<uint64_t>::iterator it = mem->pending_pages.begin(); it != mem->pending_pages.end(); it++)
	{
		cout << (*it) << " ";
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
	cout << "pending_sets.size() = " << mem->pending_sets.size() << "\n\n";
	cout << "pending_sets_max = " << mem->pending_sets_max << "\n\n";
	cout << "pending_pages_max = " << mem->pending_pages_max << "\n\n";
	cout << "trans_queue_max = " << mem->trans_queue_max << "\n\n";
	
	mem->printLogfile();

	for (int i=0; i<500; i++)
	{
		mem->update();
	}

	return 0;
}

