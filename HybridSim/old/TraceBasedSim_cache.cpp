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

	const int LEN = 1000;
	//uint64_t addr[] = {0x50000, 0x90012};
	//int rd[] = {1, 0};

//	for (int i=0; i<LEN; i++)
//	{
//		TransactionType type;
//		if (i % 2 == 0)
//			type = DATA_READ;
//		else
//			type = DATA_WRITE;
//
//		cout << "Adding transaction for " << type << i*8 << endl;
//		Transaction t = Transaction(type, i*1024, NULL);
//		mem->addTransaction(t);
//
//		for (int j=0; j<1; j++)
//		{
//			mem->update();
//		}
//	}

	/* create a transaction and add it */
	//Transaction tr = Transaction(DATA_READ, 0x50000, NULL);
	Transaction tr = Transaction(DATA_WRITE, 0x50000, NULL);
	mem->addTransaction(tr);

	/* do a bunch of updates (i.e. clocks) -- at some point the callback will fire */
	for (int i=0; i<100; i++)
	{
		mem->update();
	}

	/* add another some time in the future */
	//Transaction tw = Transaction(DATA_WRITE, 0x90012, NULL);
	Transaction tw = Transaction(DATA_READ, 0x50000, NULL);
	mem->addTransaction(tw);

	for (int i=0; i<2000; i++)
	{
		mem->update();
	}

	/* get a nice summary of this epoch */
	mem->printStats();

	cout << "\ncompleted " << complete << " transactions.\n\n";

	return 0;
}

