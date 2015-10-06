/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
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

int complete = 0;

int main()
{
	printf("dramsim_test main()\n");
	some_object obj;
	obj.add_one_and_run();
}


void some_object::read_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	printf("[Callback] read complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	complete++;
}

void some_object::write_complete(uint id, uint64_t address, uint64_t clock_cycle)
{
	printf("[Callback] write complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
	complete++;
}

/* FIXME: this may be broken, currently */
void power_callback(double a, double b, double c, double d)
{
	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
}

int some_object::add_one_and_run()
{
	/* pick a DRAM part to simulate */
	HybridSystem *mem = new HybridSystem(1);

	/* create and register our callback functions */
	Callback_t *read_cb = new Callback<some_object, void, uint, uint64_t, uint64_t>(this, &some_object::read_complete);
	Callback_t *write_cb = new Callback<some_object, void, uint, uint64_t, uint64_t>(this, &some_object::write_complete);
	mem->RegisterCallbacks(read_cb, write_cb, power_callback);

	const int NUM_ACCESSES = 1000;

	for (int i=0; i<NUM_ACCESSES; i++)
	{
		TransactionType type;
		if (i >= NUM_ACCESSES/2)
			type = DATA_READ;
		else
			type = DATA_WRITE;

		// Uncomment one of the following to either test different sets or the same set.
		uint64_t addr = (i%(NUM_ACCESSES/2)) * PAGE_SIZE;
		//uint64_t addr = (i%(NUM_ACCESSES/2)) * PAGE_SIZE * NUM_SETS;

		Transaction t = Transaction(type, addr, NULL);
		mem->addTransaction(t);

#if DEBUG_CACHE
		cout << "\n\tAdded transaction " << i << " of type=" << type << " addr=" << addr << " set=" << SET_INDEX(addr) 
			<< " tag=" << TAG(addr) << endl;
#endif

		for (int j=0; j<10000; j++)
		{
			mem->update();
		}
	}


	for (int i=0; i<1000000; i++)
	{
		mem->update();
	}

//	/* get a nice summary of this epoch */
//	mem->printStats();

	cout << "\n\n" << mem->currentClockCycle << ": completed " << complete << " transactions. dram_pending=" << mem->dram_pending.size() 
		<< " flash_pending=" << mem->flash_pending.size() << "\n\n";

	return 0;
}

