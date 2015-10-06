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

//GCFtl.cpp
//class file for GCftl
//
#include "GCFtl.h"
#include "ChannelPacket.h"
#include "NVDIMM.h"
#include <cmath>

using namespace NVDSim;
using namespace std;

GCFtl::GCFtl(Controller *c, Logger *l, NVDIMM *p) 
    : Ftl(c, l, p)
{	
        int numBlocks = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE;

	used_page_count = 0;
	gc_status = 0;
	panic_mode = 0;

	dirty_page_count = 0;

	dirty = vector<vector<bool>>(numBlocks, vector<bool>(PAGES_PER_BLOCK, false));
	
	gcQueue = list<FlashTransaction>();

	// set up internal pointer to make sure that the gc isn't always erasing the same block
	erase_pointer = 0;
}

bool GCFtl::addTransaction(FlashTransaction &t){
    if(t.address < (VIRTUAL_TOTAL_SIZE*1024))
    {
	if(!panic_mode)
	{
	    // we are going to favor reads over writes
	    // so writes get put into a special lower prioirty queue
	    if(SCHEDULE)
	    {
		return addScheduledTransaction(t);
	    }
	    else if(PERFECT_SCHEDULE)
	    {
		return addPerfectTransaction(t);
	    }
	    // no scheduling, so just shove everything into the read queue
	    else
	    {
		return attemptAdd(t, &readQueue, FTL_READ_QUEUE_LENGTH);
	    }
	}
    }
    ERROR("Tried to add a transaction with a virtual address that was out of bounds");
    exit(5001);
}

void GCFtl::addGcTransaction(FlashTransaction &t){ 
    // we use a special GC queue whether we're scheduling or not so always just do it like this
    gcQueue.push_back(t);
    write_queues_full = false;
    
    if(LOGGING == true)
    {
	// Start the logging for this access.
	log->access_start(t.address);
    }
}

void GCFtl::update(void){
	uint64_t i;
	if (gc_status){
		if (!panic_mode && parent->numErases == start_erase + 1)
			gc_status = 0;
		if (panic_mode && parent->numErases == start_erase + PLANES_PER_DIE * DIES_PER_PACKAGE * NUM_PACKAGES){
			panic_mode = 0;
			gc_status = 0;
		}
	}

	if (!gc_status && (float)used_page_count >= (float)(FORCE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE))){
	    if(dirty_page_count != 0)
	    {
		start_erase = parent->numErases;
		gc_status = 1;
		panic_mode = 1;
		busy = 0;
		for (i = 0 ; i < PLANES_PER_DIE * DIES_PER_PACKAGE * NUM_PACKAGES; i++)
		{
			runGC(i);
		}
	    }
	    else if((float)used_page_count >= (float)(VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE))
	    {
		ERROR("FLASH DIMM IS FULL OF USED PAGES AND NONE OF THEM ARE DIRTY - there is nothing the gc can do.");
		exit(7001);
	    }
	}

	if (busy) {
	    if (lookupCounter <= 0 && !write_queues_full){
			uint64_t vAddr = currentTransaction.address;
			bool result = false;
			ChannelPacket *commandPacket;
			
			switch (currentTransaction.transactionType){
				case DATA_READ:
				{
				    if(!read_queues_full)
				    {
					handle_read(false);
				    }
				    break;
				}
				case DATA_WRITE:
					handle_write(false);
					break;

				case GC_DATA_READ:
				{
				    // right now we just lock up on a gc read if it doesn't go through
				    handle_read(true);
				    break;
				}

				case GC_DATA_WRITE:
					handle_write(true);
					break;

				case BLOCK_ERASE:
					commandPacket = Ftl::translate(ERASE, vAddr, vAddr);//note: vAddr is actually the pAddr in this case with the way garbage collection is written
					result = controller->addPacket(commandPacket);
					if(result == true)
					{
					    for (i = 0 ; i < PAGES_PER_BLOCK ; i++){
						if (dirty[vAddr / BLOCK_SIZE][i]){
							dirty[vAddr / BLOCK_SIZE][i] = false;
							dirty_page_count--;
						}
						if (used[vAddr / BLOCK_SIZE][i]){
							used[vAddr / BLOCK_SIZE][i] = false;
							used_page_count--;
						}
					    }
					    if(gc_status)
					    {
						gcQueue.pop_front();
					    }
					    else
					    {
						read_pointer = readQueue.erase(read_pointer);
					    }
					    busy = 0;
					}
					else
					{
					    delete commandPacket;
					    write_queues_full = true;
					}
					break;		

				default:
					ERROR("Transaction in Ftl that isn't a read or write... What?");
					exit(1);
					break;
			}
		} 
		else if(lookupCounter > 0)
		{
			lookupCounter--;
		}
		else if(write_queues_full || read_queues_full)
		{
		    locked_counter++;
		}
	} 
	// Not currently busy.
	else {
	    // if we're doing gc stuff then everything should be coming from the gc queue
	    if(gc_status)
	    {
		if (!gcQueue.empty()) {
		    busy = 1;
		    currentTransaction = gcQueue.front();
		    lookupCounter = LOOKUP_CYCLES;
		}
		// do nothing
		else
		{
		    busy = 0;
		}
	    }
	    // if we're not in gc mode and...
	    // we're favoring reads over writes so we need to check the write queues to make sure they
	    // aren't filling up. if they are we issue a write, otherwise we just keeo on issuing reads
	    else if(SCHEDULE || PERFECT_SCHEDULE)
	    {
		if(ENABLE_WRITE_SCRIPT)
		{
		    scriptCurrentTransaction();
		}
		// standard scheduling
		else
		{
		    // schedule the next transaction using the scheduler algorithm in Ftl.cpp
		    scheduleCurrentTransaction();
		}

	    }
	     // we're not scheduling so everything is in the read queue
	    // just issue from there
	    else {
		if (!readQueue.empty()) {
		    busy = 1;
		    currentTransaction = readQueue.front();
		    lookupCounter = LOOKUP_CYCLES;
		}
		// do nothing
		else
		{
		    busy = 0;
		}
	    }

	    // still need something to do?
	    // Check to see if GC needs to run.
	    // just using lookupCounter here as an indicator or whether or not something was done 
	    // before we got here
	    if (lookupCounter != LOOKUP_CYCLES && checkGC() && !gc_status && dirty_page_count != 0)
	    {
		// Run the GC.
		start_erase = parent->numErases;
		gc_status = 1;
		runGC();
	    }
	}

	//place power callbacks to hybrid_system
#if Verbose_Power_Callback
	  controller->returnPowerData(idle_energy, access_energy, erase_energy);
#endif

}

void GCFtl::write_used_handler(uint64_t vAddr)
{
	dirty[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = true;
	dirty_page_count ++;
}

bool GCFtl::checkGC(void){
	// Return true if more than 70% of blocks are in use and false otherwise.
        if ((float)used_page_count > ((float)IDLE_GC_THRESHOLD * (VIRTUAL_TOTAL_SIZE / NV_PAGE_SIZE)))
		return true;
	return false;
}


void GCFtl::runGC() {
  uint64_t block, page, count, dirty_block=0, dirty_count=0;
	FlashTransaction trans;
	PendingErase temp_erase;
	cout << "normal gc running \n";
	// Get the dirtiest block (assumes the flash keeps track of this with an online algorithm).
	for (block = erase_pointer; block < TOTAL_SIZE / BLOCK_SIZE; block++) {
	  count = 0;
	  for (page = 0; page < PAGES_PER_BLOCK; page++) {
		if (dirty[block][page] == true) {
			count++;
		}
	  }
	  if (count > dirty_count) {
	      	dirty_count = count;
	       	dirty_block = block;
	  }
	}
	erase_pointer = (dirty_block + 1) % (TOTAL_SIZE / BLOCK_SIZE);

	addGC(dirty_block);
}

// overloaded version of runGC that we use in panic mode to issue a different erase to each plane
// if we are in panic mode then the system is dangerously full, we need to make as much clean space as possible
// so we will erase a block on every independent plane at the same time
void GCFtl::runGC(uint64_t plane) {
  uint64_t block, page, count, dirty_block=0, dirty_count=0;
  cout << "panic mode gc running \n";
	FlashTransaction trans;
	PendingErase temp_erase;

	// Get the dirtiest block (assumes the flash keeps track of this with an online algorithm).
	for (block = (plane * BLOCKS_PER_PLANE); block < ((plane + 1) * BLOCKS_PER_PLANE); block++) {
	  count = 0;
	  for (page = 0; page < PAGES_PER_BLOCK; page++) {
		if (dirty[block][page] == true) {
			count++;
		}
	  }
	  if (count > dirty_count) {
	      	dirty_count = count;
	       	dirty_block = block;
	  }
	}

	addGC(dirty_block);
}

// the guts of the runGC function separated out to prevent duplicated code
// this adds the read GC transactions if there are any and creates a pending erase entry
void GCFtl::addGC(uint64_t dirty_block)
{
     uint64_t page, pAddr, vAddr, tmpAddr;
     FlashTransaction trans;
     PendingErase temp_erase;

     // set the block we're going to erase with this gc operation
     temp_erase.erase_block = dirty_block;

     // All used pages in the dirty block, they must be moved elsewhere.
     for (page = 0; page < PAGES_PER_BLOCK; page++) {
	 if (used[dirty_block][page] == true && dirty[dirty_block][page] == false) {
	     // Compute the physical address to move.
	     pAddr = (dirty_block * BLOCK_SIZE + page * NV_PAGE_SIZE);

	     // Do a reverse lookup for the virtual page address.
	     // This is slow, but the alternative is maintaining a full reverse lookup map.
	     // Another alternative may be to make new FlashTransaction commands for physical address read/write.
	     bool found = false;
	     for (std::unordered_map<uint64_t, uint64_t>::iterator it = addressMap.begin(); it != addressMap.end(); it++) {
		 tmpAddr = (*it).second;
		 if (tmpAddr == pAddr) {
		     vAddr = (*it).first;
		     found = true;
		     break;
		 }
	     }		assert(found);

	     // Schedule a read
	     trans = FlashTransaction(GC_DATA_READ, vAddr, NULL);
	     addGcTransaction(trans);
	     
	     // add an entry to the pending writes list in our erase record
	     temp_erase.pending_reads.push_front(vAddr);
	 }
     }
     // if we didn't need to move anything just go ahead and erase
     if(temp_erase.pending_reads.empty())
     {
	 trans = FlashTransaction(BLOCK_ERASE, dirty_block * BLOCK_SIZE, NULL); 
	 addGcTransaction(trans);
     }
     else
     {
	 gc_pending_erase.push_front(temp_erase);
     }
}

void GCFtl::popFront(ChannelPacketType type)
{
    // if its a gc operation pop from the gc queue
    if(type == ERASE || type == GC_READ || type == GC_WRITE)
    {
	gcQueue.pop_front();
    }
    // if we've put stuff into different queues we must now figure out which queue to pop from
    else if(SCHEDULE || PERFECT_SCHEDULE)
    {
	if(type == READ)
	{
	    read_pointer = readQueue.erase(read_pointer);
	    // we finished the read we were trying now go back to the front of the list and try
	    // the first one again
	    read_pointer = readQueue.begin();
	    if(LOGGING && QUEUE_EVENT_LOG)
	    {
		log->log_ftl_queue_event(false, &readQueue);
	    }
	}
	else if(type == WRITE)
	{
	    writeQueue.pop_front();
	    if(LOGGING && QUEUE_EVENT_LOG)
	    {
		log->log_ftl_queue_event(true, &writeQueue);
	    }
	}
    }
    // if we're just putting everything into the read queue, just pop from there
    // unless its a gc operation then pop from the gcQueue
    else
    {
	readQueue.pop_front();
    }
}

void GCFtl::sendQueueLength(void)
{
	if(LOGGING == true)
	{
	    log->ftlQueueLength(readQueue.size(), gcQueue.size());
	}
}

void GCFtl::saveNVState(void)
{
     if(ENABLE_NV_SAVE && !saved)
    {
	ofstream save_file;
	save_file.open(NV_SAVE_FILE, ios_base::out | ios_base::trunc);
	if(!save_file)
	{
	    cout << "ERROR: Could not open NVDIMM state save file: " << NV_SAVE_FILE << "\n";
	    abort();
	}
	
	cout << "NVDIMM is saving the used table, dirty table and address map \n";

	// save the address map
	save_file << "AddressMap \n";
	std::unordered_map<uint64_t, uint64_t>::iterator it;
	for (it = addressMap.begin(); it != addressMap.end(); it++)
	{
	    save_file << (*it).first << " " << (*it).second << " \n";
	}

        // save the dirty table
	save_file << "Dirty \n";
	for(uint64_t i = 0; i < dirty.size(); i++)
	{
	    for(uint64_t j = 0; j < dirty[i].size(); j++)
	    {
		save_file << dirty[i][j] << " ";
	    }
	    save_file << "\n";
	}

	// save the used table
	save_file << "Used";
	for(uint64_t i = 0; i < used.size(); i++)
	{
	    save_file << "\n";
	    for(uint64_t j = 0; j < used[i].size()-1; j++)
	    {
		save_file << used[i][j] << " ";
	    }
	    save_file << used[i][used[i].size()-1];
	}

	save_file.close();
	saved = true;
    }
}

void GCFtl::loadNVState(void)
{
    if(ENABLE_NV_RESTORE && !loaded)
    {
	ifstream restore_file;
	restore_file.open(NV_RESTORE_FILE);
	if(!restore_file)
	{
	    cout << "ERROR: Could not open NVDIMM restore file: " << NV_RESTORE_FILE << "\n";
	    abort();
	}

	cout << "NVDIMM is restoring the system from file " << NV_RESTORE_FILE <<"\n";

	// restore the data
	uint64_t doing_used = 0;
	uint64_t doing_dirty = 0;
	uint64_t doing_addresses = 0;
	uint64_t row = 0;
	uint64_t column = 0;
	uint64_t first = 0;
	uint64_t key = 0;
	uint64_t pAddr = 0;
	uint64_t vAddr = 0;

	std::unordered_map<uint64_t,uint64_t> tempMap;

	std::string temp;
	
	while(!restore_file.eof())
	{ 
	    restore_file >> temp;
	    
	    // these comparisons make this parser work but they are dependent on the ordering of the data in the state file
	    // if the state file changes these comparisons may also need to be changed
	    if(temp.compare("Used") == 0)
	    {
		doing_used = 1;
		doing_addresses = 0;
		doing_dirty = 0;
		
		row = 0;
		column = 0;
	    }
	    else if(temp.compare("Dirty") == 0)
	    {
		doing_used = 0;
		doing_dirty = 1;
		doing_addresses = 0;
		
		row = 0;
		column = 0;
	    }
	    else if(temp.compare("AddressMap") == 0)
	    {
		doing_used = 0;
		doing_dirty = 0;
		doing_addresses = 1;

		row = 0;
		column = 0;
	    }
	    // restore used data
	    else if(doing_used == 1)
	    {
		used[row][column] = convert_uint64_t(temp);

                // this page was used need to issue fake write
		if(temp.compare("1") == 0 && dirty[row][column] != 1)
		{
		    pAddr = (row * BLOCK_SIZE + column * NV_PAGE_SIZE);
		    vAddr = tempMap[pAddr];
		    ChannelPacket *tempPacket = Ftl::translate(FAST_WRITE, vAddr, pAddr);
		    controller->writeToPackage(tempPacket);

		    used_page_count++;
		}		

		column++;
		if(column >= PAGES_PER_BLOCK)
		{
		    row++;
		    column = 0;
		}
	    }
	    // restore dirty data
	    else if(doing_dirty == 1)
	    {
		dirty[row][column] = convert_uint64_t(temp);
		column++;
		if(column >= PAGES_PER_BLOCK)
		{
		    row++;
		    column = 0;
		}
	    }
	    // restore address map data
	    else if(doing_addresses == 1)
	    {	
		if(first == 0)
		{
		    first = 1;
		    key = convert_uint64_t(temp);
		}
		else
		{
		    addressMap[key] = convert_uint64_t(temp);
		    tempMap[convert_uint64_t(temp)] = key;
		    first = 0;
		}
	    }   
	}

	restore_file.close();
	loaded = true;
    }
}

void GCFtl::GCReadDone(uint64_t vAddr)
{
   list<PendingErase>::iterator it;
    for (it = gc_pending_erase.begin(); it != gc_pending_erase.end(); it++)
    {
	(*it).pending_reads.remove(vAddr);

	if((*it).pending_reads.empty())
	{
	    FlashTransaction trans = FlashTransaction(BLOCK_ERASE, (*it).erase_block * BLOCK_SIZE, NULL); 
	    addGcTransaction(trans);
	    gc_pending_erase.erase(it);
	    break;
	}
    } 

   FlashTransaction trans = FlashTransaction(GC_DATA_WRITE, vAddr, NULL);
   addGcTransaction(trans);
}
