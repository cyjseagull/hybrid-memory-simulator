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

//Ftl.cpp
//class file for ftl
//
#include "Ftl.h"
#include "ChannelPacket.h"
#include "NVDIMM.h"
#include <cmath>

using namespace NVDSim;
using namespace std;

Ftl::Ftl(Controller *c, Logger *l, NVDIMM *p){
	int numBlocks = NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE * BLOCKS_PER_PLANE;

	channel = 0;
	die = 0;
	plane = 0;
	lookupCounter = 0;

	busy = 0;

	addressMap = std::unordered_map<uint64_t, uint64_t>();

	used = vector<vector<bool>>(numBlocks, vector<bool>(PAGES_PER_BLOCK, false));

	readQueue = list<FlashTransaction>();
	writeQueue = list<FlashTransaction>();

	read_iterator_counter = 0;
	read_pointer = readQueue.begin();

	controller = c;

	parent = p;

	log = l;

	locked_counter = 0;
	
	saved = false;
	loaded = false;
	read_queues_full = false;
	write_queues_full = false;

	// Counter to keep track of succesful writes.
	write_counter = 0;
	used_page_count = 0;

	// Counter to keep track of how long we've been access data in the write queue
	queue_access_counter = 0;

	// Counter to keep track of cycles we spend waiting on erases
	// if we wait longer than the length of an erase, we've probably deadlocked
	deadlock_counter = 0;

	// the maximum amount of time we can wait before we're sure we've deadlocked
	// time it takes to read all of the pages in a block
	deadlock_time = PAGES_PER_BLOCK * (READ_CYCLES + ((divide_params_64b((NV_PAGE_SIZE*8),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME) +
					   ((divide_params_64b(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME));
	// plus the time it takes to write all of the pages in a block
	deadlock_time += PAGES_PER_BLOCK * (WRITE_CYCLES + ((divide_params_64b((NV_PAGE_SIZE*8),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME) +
					   ((divide_params_64b(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME));
	// plus the time it takes to erase the block
	deadlock_time += ERASE_CYCLES + ((divide_params_64b(COMMAND_LENGTH,DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME);

	write_wait_count = DELAY_WRITE_CYCLES;

	if(ENABLE_WRITE_SCRIPT)
	{
	    std::string temp;

	    // open the file and get the first write
	    scriptfile.open(NV_WRITE_SCRIPT, ifstream::in);

	    if(!scriptfile)
	    {
		cout << "ERROR: Could not open NVDIMM write script file: " << NV_WRITE_SCRIPT << "\n";
		abort();
	    }
	    // get the cycle
	    scriptfile >> temp;
	    write_cycle = convert_uint64_t(temp);
	    // get the address
	    scriptfile >> temp;
	    // write_addr = convert_uint64_t(temp); Not going to use this because the addresses change with each run
	    // get the package
	    scriptfile >> temp;
	    write_pack = convert_uint64_t(temp);
	    // get the die
	    scriptfile >> temp;
	    write_die = convert_uint64_t(temp);
	    // get the plane
	    scriptfile >> temp;
	    write_plane = convert_uint64_t(temp);	    
	}
}

ChannelPacket *Ftl::translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr){

	uint64_t package, die, plane, block, page;
	//uint64_t tempA, tempB, physicalAddress = pAddr;
	uint64_t physicalAddress = pAddr;

	if (physicalAddress > TOTAL_SIZE - 1){
		ERROR("Inavlid address in Ftl: "<<physicalAddress);
		exit(1);
	}

	physicalAddress /= NV_PAGE_SIZE;
	page = physicalAddress % PAGES_PER_BLOCK;
	physicalAddress /= PAGES_PER_BLOCK;
	block = physicalAddress % BLOCKS_PER_PLANE;
	physicalAddress /= BLOCKS_PER_PLANE;
	plane = physicalAddress % PLANES_PER_DIE;
	physicalAddress /= PLANES_PER_DIE;
	die = physicalAddress % DIES_PER_PACKAGE;
	physicalAddress /= DIES_PER_PACKAGE;
	package = physicalAddress % NUM_PACKAGES;

	return new ChannelPacket(type, vAddr, pAddr, page, block, plane, die, package, NULL);
}

bool Ftl::attemptAdd(FlashTransaction &t, std::list<FlashTransaction> *queue, uint64_t queue_limit)
{
    if(queue->size() >= queue_limit && queue_limit != 0)
    {
	return false;
    }
    else
    {
	queue->push_back(t);
	
	if(LOGGING)
	{
	    // Start the logging for this access.
	    log->access_start(t.address, t.transactionType);
	    log->ftlQueueLength(queue->size());
	    if(QUEUE_EVENT_LOG)
	    {
		log->log_ftl_queue_event(false, queue);
	    }
	}
	return true;
    }
}

bool Ftl::addScheduledTransaction(FlashTransaction &t)
{
    if(t.transactionType == DATA_READ || t.transactionType == BLOCK_ERASE)
    {
	bool success =  attemptAdd(t, &readQueue, FTL_READ_QUEUE_LENGTH);
	if (success == true)
	{
	    if( readQueue.size() == 1)
	    {
		read_pointer = readQueue.begin();
	    }
	}
	return success;
    }
    else if(t.transactionType == DATA_WRITE)
    {
	// see if this write replaces another already in the write queue
	// if it does remove that other write from the queue
	list<FlashTransaction>::iterator it;
	int count = 0;
	for (it = writeQueue.begin(); it != writeQueue.end(); it++)
	{
	    // don't replace the write if we're already working on it
	    if((*it).address == t.address && currentTransaction.address != t.address)
	    {
		if(LOGGING)
		{
		    // access_process for that write is called here since its over now.
		    log->access_process(t.address, t.address, 0, WRITE);
		    
		    // stop_process for that write is called here since its over now.
		    log->access_stop(t.address, t.address);
		}
		// issue a callback for this write
		if (parent->WriteDataDone != NULL){
		    (*parent->WriteDataDone)(parent->systemID, (*it).address, currentClockCycle, true);
		}
		writeQueue.erase(it);
		break;
	    }
	    count++;
	}
	// if we erased the write that this write replaced then we should definitely
	// always have room for this write
	return attemptAdd(t, &writeQueue, FTL_WRITE_QUEUE_LENGTH);
    }
    return false;
}

bool Ftl::addPerfectTransaction(FlashTransaction &t)
{
    if(t.transactionType == DATA_READ || t.transactionType == BLOCK_ERASE)
    {
	bool success =  attemptAdd(t, &readQueue, FTL_READ_QUEUE_LENGTH);
	if (success == true)
	{
	    if( readQueue.size() == 1)
	    {
		read_pointer = readQueue.begin();
	    }
	}
	return success;
    }
    else if(t.transactionType == DATA_WRITE)
    {
	return attemptAdd(t, &writeQueue, FTL_WRITE_QUEUE_LENGTH);
    }
    return false;
}

bool Ftl::addTransaction(FlashTransaction &t){
    if(t.address < (VIRTUAL_TOTAL_SIZE*1024))
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
    
    ERROR("Tried to add a transaction with a virtual address that was out of bounds");
    exit(5001);
}

// primary scheduling algroithm 
void Ftl::scheduleCurrentTransaction(void)
{
    // do we need to issue a write?
    if((WRITE_ON_QUEUE_SIZE == true && writeQueue.size() >= WRITE_QUEUE_LIMIT) ||
       (WRITE_ON_QUEUE_SIZE == false && writeQueue.size() >= FTL_WRITE_QUEUE_LENGTH))
    {
	busy = 1;
	currentTransaction = writeQueue.front();
	lookupCounter = LOOKUP_CYCLES;
    }
    // no? then issue a read
    else if (!readQueue.empty()) {
	    busy = 1;		
	    currentTransaction = (*read_pointer);
	    lookupCounter = LOOKUP_CYCLES;
    }
    // no reads to issue? then issue a write if we have opted to issue writes during idle
    else if(IDLE_WRITE == true && !writeQueue.empty())
    {
	if(write_wait_count != 0 && DELAY_WRITE)
	{
	    write_wait_count--;				
	}
	else
	{
	    busy = 1;
	    currentTransaction = writeQueue.front();
	    lookupCounter = LOOKUP_CYCLES;
	    write_wait_count = DELAY_WRITE_CYCLES;
	}
    }
    //otherwise do nothing
    else
    {
	busy = 0;
    }
}

// algorithm for scheduling scripted writes
void Ftl::scriptCurrentTransaction(void)
{
    // is it time to issue a write?
    if(currentClockCycle >= write_cycle && !writeQueue.empty())
    {
	busy = 1;
	currentTransaction = writeQueue.front();
	lookupCounter = LOOKUP_CYCLES;
    }
    // no? then issue a read
    else if(!readQueue.empty())
    {
	busy = 1;
	currentTransaction = readQueue.front();
	lookupCounter = LOOKUP_CYCLES;
    }
    // otherwise do nothing
    else
    {
	busy = 0;
	
    }
}

void Ftl::update(void){
	if (busy) {
	    if (lookupCounter <= 0 && !write_queues_full){
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

				default:
					ERROR("FTL received an illegal transaction type:" << currentTransaction.transactionType);
					break;
			}
	    } //if lookupCounter is not 0
	    else if(lookupCounter > 0)
	    {
		lookupCounter--;
	    }
	    else if(write_queues_full || read_queues_full)// if queues full is true, these are reset by the 
	    {
		locked_counter++;
	    }
	} // Not currently busy.
	else {
	    // we're favoring reads over writes so we need to check the write queues to make sure they
	    // aren't filling up. if they are we issue a write, otherwise we just keeo on issuing reads
	    if(SCHEDULE || PERFECT_SCHEDULE)
	    {
		if(ENABLE_WRITE_SCRIPT)
		{
		    // use the script to determine whether we're issuing a write here
		    scriptCurrentTransaction();
		}
		else
		{
		    // schedule the next transaction
		    scheduleCurrentTransaction();
		}
	    }
	    // we're not scheduling so everything is in the read queue
	    // just issue from there
	    else
	    {
		if (!readQueue.empty()) {
		    busy = 1;
		    currentTransaction = readQueue.front();
		    lookupCounter = LOOKUP_CYCLES;
		}
	    }
	}
}

// fake an unmapped read for the disk case by first fast writing the page and then normally reading that page
void Ftl::handle_disk_read(bool gc)
{
    ChannelPacket *commandPacket;
    uint64_t vAddr = currentTransaction.address, pAddr;
    uint64_t start;
    bool done = false;;
    uint64_t block, page, tmp_block, tmp_page;


    //=============================================================================
    // the fast write part
    //=============================================================================
    //look for first free physical page starting at the write pointer

    start = BLOCKS_PER_PLANE * (plane + PLANES_PER_DIE * (die + DIES_PER_PACKAGE * channel));
    
    // Search from the current write pointer to the end of the flash for a free page.
    for (block = start ; block < TOTAL_SIZE / BLOCK_SIZE && !done; block++)
    {
	for (page = 0 ; page < PAGES_PER_BLOCK  && !done ; page++)
	{
	    if (!used[block][page])
	    {
		tmp_block = block;
		tmp_page = page;
		pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
		done = true;
	    }
	}
    }
    block = tmp_block;
    page = tmp_page;
    
    if (!done)
    {							
	for (block = 0 ; block < start / BLOCK_SIZE && !done; block++)
	{
	    for (page = 0 ; page < PAGES_PER_BLOCK  && !done; page++)
	    {
		if (!used[block][page])
		{
		    tmp_block = block;
		    tmp_page = page;
		    pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
		    done = true;
		}
	    }
	}
	block = tmp_block;
	page = tmp_page;
    }

    if (!done)
    {
	deadlock_counter++;
	if(deadlock_counter == deadlock_time)
	{
	    //bad news
	    cout << deadlock_time;
	    ERROR("FLASH DIMM IS COMPLETELY FULL AND DEADLOCKED - If you see this, something has gone horribly wrong.");
	    exit(9001);
	}
    } 
    else 
    {
	// We've found a used page. Now we need to try to add the transaction to the Controller queue.
	
	// first things first, we're no longer in danger of dead locking so reset the counter
	deadlock_counter = 0;
	
	// quick write the page
	ChannelPacket *tempPacket = Ftl::translate(FAST_WRITE, vAddr, pAddr);
	controller->writeToPackage(tempPacket);

	used.at(block).at(page) = true;
	used_page_count++;
	addressMap[vAddr] = pAddr;
	
	//update "write pointer"
	channel = (channel + 1) % NUM_PACKAGES;
	if (channel == 0){
	    die = (die + 1) % DIES_PER_PACKAGE;
	    if (die == 0)
		plane = (plane + 1) % PLANES_PER_DIE;
	}
	//=============================================================================
	// the read part
	//=============================================================================    
	// so now we can read
	// now make a read to that page we just quickly wrote
	commandPacket = Ftl::translate(READ, vAddr, addressMap[vAddr]);
	
	//send the read to the controller
	bool result = controller->addPacket(commandPacket);
	if(result)
	{
	    if(LOGGING && !gc)
	    {
		// Update the logger (but not for GC_READ).
		log->read_mapped();
	    }
	    popFront(READ);
	    read_iterator_counter = 0;
	    busy = 0;
	    
	}
	else
	{
	    // Delete the packet if it is not being used to prevent memory leaks.
	    delete commandPacket;
	    
	    if(!SCHEDULE)
	    {
		write_queues_full = true;	
		log->locked_up(currentClockCycle);
	    }
	    else
	    {
		read_pointer++;
		// making sure we don't fall off of the edge of the world
		if(read_pointer == readQueue.end())
		{
		    read_pointer = readQueue.begin();
		}
		read_iterator_counter++;
		// if we've cycled through everything then we need to wait till something gets done
		// before continuing
		if( read_iterator_counter >= readQueue.size())
		{
		    // since we're going to be starting again when we get unlocked up
		    // make sure its from the beginning
		    read_pointer = readQueue.begin();
		    read_queues_full = true;
		    log->locked_up(currentClockCycle);
		    read_iterator_counter = 0;
		}
		busy = 0;
	    }	
	}
    }
}

void Ftl::handle_read(bool gc)
{
    ChannelPacket *commandPacket;
    uint64_t vAddr = currentTransaction.address;
    bool write_queue_handled = false;
    //Check to see if the vAddr corresponds to the write waiting in the write queue
    if(!gc && SCHEDULE)
    {
	// first time here, find a write in the write queue that can satisfy this read
	if(queue_access_counter == 0)
	{
	    for (reading_write = writeQueue.begin(); reading_write != writeQueue.end(); reading_write++)
	    {
		if((*reading_write).address == vAddr)
		{
		    queue_access_counter = QUEUE_ACCESS_CYCLES;
		    write_queue_handled = true;
		    if(LOGGING)
		    {

			// Update the logger.
			log->read_mapped();

			// access_process for this read is called here since it starts here
			log->access_process(vAddr, vAddr, 0, READ);
		    }
		}
	    }
	}
	// we found a write the satisfy the read, now we're waiitng to get the data out of the queue
	else if(queue_access_counter > 0)
	{
	    queue_access_counter--;
	    write_queue_handled = true;
	    // if we're done waiting for the data to come out of the queue, then this read is finished
	    if(queue_access_counter == 0)
	    {
		if(LOGGING)
		{
		    // stop_process for this read is called here since this ends now.
		    log->access_stop(vAddr, vAddr);
		}

		controller->returnReadData(FlashTransaction(RETURN_DATA, vAddr, (*reading_write).data));
		
		if(LOGGING && QUEUE_EVENT_LOG)
		{
		    log->log_ftl_queue_event(false, &readQueue);
		}
		popFront(READ);
		read_iterator_counter = 0;
		busy = 0;
	    }
	}
    }
    // if we couldn't find a write to satisfy this read
    if(!write_queue_handled)
    {
        // Check to see if the vAddr exists in the address map.
	if (addressMap.find(vAddr) == addressMap.end())
	{
		if (gc)
		{
			ERROR("GC tried to move data that wasn't there.");
			exit(1);
		}

		// if we are disk reading then we want to map an unmapped read and treat is normally
		if(DISK_READ)
		{
		    handle_disk_read(gc);
		}
		else
		{

		    // If not, then this is an unmapped read.
		    // We return a fake result immediately.
		    // In the future, this could be an error message if we want.
		    if(LOGGING)
		    {
			// Update the logger
			log->read_unmapped();
			
			// access_process for this read is called here since this ends now.
			log->access_process(vAddr, vAddr, 0, READ);

			// stop_process for this read is called here since this ends now.
			log->access_stop(vAddr, vAddr);
		    }

		    // Miss, nothing to read so return garbage.
		    controller->returnUnmappedData(FlashTransaction(RETURN_DATA, vAddr, (void *)0xdeadbeef));
	       
		    popFront(READ);
		    read_iterator_counter = 0;
		    busy = 0;
		}
	} 
	else 
	{	
		ChannelPacketType read_type;
		if (gc)
			read_type = GC_READ;
		else
			read_type = READ;
		commandPacket = Ftl::translate(read_type, vAddr, addressMap[vAddr]);

		//send the read to the controller
		bool result = controller->addPacket(commandPacket);
		if(result)
		{
			if(LOGGING && !gc)
			{
				// Update the logger (but not for GC_READ).
				log->read_mapped();
			}
			popFront(read_type);
			read_iterator_counter = 0;
			busy = 0;
    
		}
		else
		{
			// Delete the packet if it is not being used to prevent memory leaks.
			delete commandPacket;
			if(!SCHEDULE)
			{
			    write_queues_full = true;	
			    log->locked_up(currentClockCycle);
			}
			else
			{
			    read_pointer++;
			    // making sure we don't fall off of the edge of the world
			    if(read_pointer == readQueue.end())
			    {
				read_pointer = readQueue.begin();
			    }
			    read_iterator_counter++;
			    // if we've cycled through everything then we need to wait till something gets done
			    // before continuing
			    if( read_iterator_counter >= readQueue.size())
			    {
				// since we're going to be starting again when we get unlocked up
				// make sure its from the beginning
				read_pointer = readQueue.begin();
				read_queues_full = true;
				log->locked_up(currentClockCycle);
				read_iterator_counter = 0;
			    }
			    busy = 0;
			}	
		}
	}
    }
}

void Ftl::write_used_handler(uint64_t vAddr)
{
		// we're going to write this data somewhere else for wear-leveling purposes however we will probably 
		// want to reuse this block for something at some later time so mark it as unused because it is
		used[addressMap[vAddr] / BLOCK_SIZE][(addressMap[vAddr] / NV_PAGE_SIZE) % PAGES_PER_BLOCK] = false;

		cout << "USING FTL's WRITE_USED_HANDLER!!!\n";
}

void Ftl::write_success(uint64_t block, uint64_t page, uint64_t vAddr, uint64_t pAddr, bool gc, bool mapped)
{
    // Set the used bit for this page to true.
    used[block][page] = true;
    used_page_count++;
	
    // Pop the transaction from the transaction queue.
    popFront(WRITE);
	
    // The FTL is no longer busy.
    busy = 0;
	
    // Update the write counter.
    write_counter++;
    //cout << "WRITE COUNTER IS " << write_counter << "\n";
	
	
    if (LOGGING && !gc)
    {
	if (mapped)
	    log->write_mapped();
	else
	    log->write_unmapped();
    }
	
    // Update the address map.
    addressMap[vAddr] = pAddr;
}

void Ftl::handle_scripted_write(void)
{
    uint64_t start, stop;
    uint64_t vAddr = currentTransaction.address, pAddr;
    ChannelPacket *commandPacket, *dataPacket;
    bool done = false;
    uint64_t block, page, tmp_block, tmp_page;

    // search the die for a block and a page that are unused
    //look for first free physical page starting at the write pointer
    start = BLOCKS_PER_PLANE * (write_plane + PLANES_PER_DIE * (write_die + DIES_PER_PACKAGE * write_pack));
    stop = BLOCKS_PER_PLANE * ((write_plane + 1) + PLANES_PER_DIE * (write_die + DIES_PER_PACKAGE * write_pack));
    
    // Search from the current write pointer to the end of the flash for a free page.
    for (block = start; block < stop && !done; block++)
    {
	for (page = 0 ; page < PAGES_PER_BLOCK  && !done ; page++)
	{
	    if (!used[block][page])
	    {
		tmp_block = block;
		tmp_page = page;
		pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
		done = true;
	    }
	}
    }
    block = tmp_block;
    page = tmp_page;
    
    if(!done)
    {
	ERROR("Write script tried to write to a plane that had no free pages");
    }
    
    dataPacket = Ftl::translate(DATA, vAddr, pAddr);
    commandPacket = Ftl::translate(WRITE, vAddr, pAddr);
    
    // Check to see if there is enough room for both packets in the queue (need two open spots).
    bool queue_open = controller->checkQueueWrite(dataPacket);
    
    if (!queue_open)
    {
	// These packets are not being used. Since they were dynamically allocated, we must delete them to prevent
	// memory leaks.
	delete dataPacket;
	delete commandPacket;
	
	ERROR("Write script tried to write to a plane that was not free")
	}
    else if (queue_open)
    {
	// Add the packets to the controller queue.
	// Do not need to check the return values for these since checkQueueWrite() was called.
	controller->addPacket(dataPacket);
	controller->addPacket(commandPacket);	    
	
	// made this a function cause the code was repeated a bunch of places
	// mapped and gc don't matter here
	write_success(block, page, vAddr, pAddr, false, false);
	
	std::string temp;
	
	// get the cycle
	scriptfile >> temp;
	write_cycle = convert_uint64_t(temp);
	// get the address
	// ignoring the address cause it changes with each run
	scriptfile >> temp;
	// get the package
	scriptfile >> temp;
	write_pack = convert_uint64_t(temp);
	// get the die
	scriptfile >> temp;
	write_die = convert_uint64_t(temp);
	// get the plane
	scriptfile >> temp;
	write_plane = convert_uint64_t(temp);
    }
}

void Ftl::handle_write(bool gc)
{
    uint64_t start;
    uint64_t vAddr = currentTransaction.address, pAddr;
    ChannelPacket *commandPacket, *dataPacket;
    bool done = false;
    bool finished = false;
    uint64_t block, page, tmp_block, tmp_page;

    temp_channel = channel;
    temp_die = die;
    temp_plane = plane;
    uint64_t itr_count = 0;

    // Mapped is used to indicate to the logger that a write was mapped or unmapped.
    bool mapped = false;

    if (addressMap.find(vAddr) != addressMap.end())
    {
	write_used_handler(vAddr);
	
	mapped = true;
    }
	    
    // if we are using a write script then we don't want to do any of the other stuff
    if(ENABLE_WRITE_SCRIPT)
    {
	// moved this to keep things cleaner
	handle_scripted_write();
    }
    else
    {
	while(!finished)
	{ 	    
	    //look for first free physical page starting at the write pointer
	    start = BLOCKS_PER_PLANE * (temp_plane + PLANES_PER_DIE * (temp_die + DIES_PER_PACKAGE * temp_channel));
	    
	    // Search from the current write pointer to the end of the flash for a free page.
	    for (block = start ; block < TOTAL_SIZE / BLOCK_SIZE && !done; block++)
	    {
		for (page = 0 ; page < PAGES_PER_BLOCK  && !done ; page++)
		{
		    if (!used[block][page])
		    {
			tmp_block = block;
			tmp_page = page;
			pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
			done = true;
		    }
		}
	    }
	    block = tmp_block;
	    page = tmp_page;
	    //attemptWrite(start, &vAddr, &pAddr, &done);
	    
	    
	    // If we didn't find a free page after scanning to the end. Scan from the beginning
	    // to the write pointer
	    if (!done)
	    {							
		for (block = 0 ; block < start / BLOCK_SIZE && !done; block++)
		{
		    for (page = 0 ; page < PAGES_PER_BLOCK  && !done; page++)
		    {
			if (!used[block][page])
			{
			    tmp_block = block;
			    tmp_page = page;
			    pAddr = (block * BLOCK_SIZE + page * NV_PAGE_SIZE);
			    done = true;
			}
		    }
		}
		block = tmp_block;
		page = tmp_page;
	    }
	    
	    if (!done)
	    {
		deadlock_counter++;
		if(deadlock_counter == deadlock_time)
		{
		    //bad news
		    cout << deadlock_time;
		    ERROR("FLASH DIMM IS COMPLETELY FULL AND DEADLOCKED - If you see this, something has gone horribly wrong.");
		    exit(9001);
		}
	    } 
	    else 
	    {
		// We've found a used page. Now we need to try to add the transaction to the Controller queue.
		
		// first things first, we're no longer in danger of dead locking so reset the counter
		deadlock_counter = 0;
		
		//send write to controller
		
		ChannelPacketType write_type;
		if (gc)
		    write_type = GC_WRITE;
		else
		    write_type = WRITE;
		dataPacket = Ftl::translate(DATA, vAddr, pAddr);
		commandPacket = Ftl::translate(write_type, vAddr, pAddr);
		
		// Psyche, we're not actually sending writes to the controller
		if(PERFECT_SCHEDULE)
		{
		    //update "write pointer"
		    channel = (channel + 1) % NUM_PACKAGES;
		    if (channel == 0){
			die = (die + 1) % DIES_PER_PACKAGE;
			if (die == 0)
			    plane = (plane + 1) % PLANES_PER_DIE;
		    }

		    // made this a function cause the code was repeated a bunch of places
		    write_success(block, page, vAddr, pAddr, gc, mapped);
		    
		    if(LOGGING && !gc)
		    {			
			// access_process for this write is called here since this ends now.
			log->access_process(vAddr, vAddr, 0, WRITE);
			
			// stop_process for this write is called here since this ends now.
			log->access_stop(vAddr, vAddr);
		    }
		    
		    // Now the write is done cause we're not actually issuing them.
		    //call write callback
		    if (parent->WriteDataDone != NULL){
			(*parent->WriteDataDone)(parent->systemID, vAddr, currentClockCycle, true);
		    }
		    finished = true;
		}
		// not perfect scheduling
		else
		{
		    // Check to see if there is enough room for both packets in the queue (need two open spots).
		    bool queue_open = controller->checkQueueWrite(dataPacket);
		    
		    // no room so we can't place the write there right now
		    if (!queue_open)
		    {
			// These packets are not being used. Since they were dynamically allocated, we must delete them to prevent
			// memory leaks.
			delete dataPacket;
			delete commandPacket;
			
			finished = false;
			
			if(SCHEDULE)
			{
			    //update the temp write pointer
			    temp_channel = (channel + 1) % NUM_PACKAGES;
			    if (temp_channel == 0){
				temp_die = (temp_die + 1) % DIES_PER_PACKAGE;
				if (temp_die == 0)
				    temp_plane = (temp_plane + 1) % PLANES_PER_DIE;
			    }
			    
			    itr_count++;
			    // if we've gone through everything and still can't find anything chill out for a while
			    if (itr_count >= (NUM_PACKAGES * DIES_PER_PACKAGE * PLANES_PER_DIE))
			    {
				write_queues_full = true;
				finished = true;
				busy = 0;
				log->locked_up(currentClockCycle);
			    }
			}
			else
			{
			    finished = true;
			    write_queues_full = true;
			}
		    }
		    // had room, place the write
		    else if (queue_open)
		    {
			// Add the packets to the controller queue.
			// Do not need to check the return values for these since checkQueueWrite() was called.
			controller->addPacket(dataPacket);
			controller->addPacket(commandPacket);	    
			
			// made this a function cause the code was repeated a bunch of places
			write_success(block, page, vAddr, pAddr, gc, mapped);
	
			// if we are using the real write pointer, then update it
			if(itr_count == 0)
			{
			    //update "write pointer"
			    channel = (channel + 1) % NUM_PACKAGES;
			    if (channel == 0){
				die = (die + 1) % DIES_PER_PACKAGE;
				if (die == 0)
				    plane = (plane + 1) % PLANES_PER_DIE;
			    }
			}
			finished = true;
		    }
		}
	    }
	}
    }
}

uint64_t Ftl::get_ptr(void) {
	// Return a pointer to the current plane.
	return NV_PAGE_SIZE * PAGES_PER_BLOCK * BLOCKS_PER_PLANE * 
		(plane + PLANES_PER_DIE * (die + NUM_PACKAGES * channel));
}

void Ftl::popFront(ChannelPacketType type)
{
    // if we've put stuff into different queues we must now figure out which queue to pop from
    if(SCHEDULE || PERFECT_SCHEDULE)
    {
	if(type == READ || type == ERASE)
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
    else
    {
	readQueue.pop_front();
	if(LOGGING && QUEUE_EVENT_LOG)
	{
	    log->log_ftl_queue_event(false, &readQueue);
	}
    }
}

void Ftl::powerCallback(void) 
{
    if(LOGGING)
    {
	vector<vector<double> > temp = log->getEnergyData();
	if(temp.size() == 2)
	{
		controller->returnPowerData(temp[0], temp[1]);
	}
	else if(temp.size() == 3)
	{
		controller->returnPowerData(temp[0], temp[1], temp[2]);
	}
	else if(temp.size() == 4)
	{
		controller->returnPowerData(temp[0], temp[1], temp[2], temp[3]);
	}
	else if(temp.size() == 6)
	{
		controller->returnPowerData(temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
	}
    }
}

void Ftl::sendQueueLength(void)
{
	if(LOGGING == true)
	{
		log->ftlQueueLength(readQueue.size());
	}
}

void Ftl::saveNVState(void)
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

		cout << "NVDIMM is saving the used table and address map \n";
		cout << "save file is " << NV_SAVE_FILE << "\n";

		// save the address map
		save_file << "AddressMap \n";
		std::unordered_map<uint64_t, uint64_t>::iterator it;
		for (it = addressMap.begin(); it != addressMap.end(); it++)
		{
			save_file << (*it).first << " " << (*it).second << " \n";
		}

		// save the used table
		save_file << "Used \n";
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

void Ftl::loadNVState(void)
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
		bool doing_used = 0;
	        bool doing_addresses = 0;
		uint64_t row = 0;
		uint64_t column = 0;
		bool first = 0;
		uint64_t key = 0;
		uint64_t pAddr, vAddr = 0;

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
			}
			else if(temp.compare("AddressMap") == 0)
			{
			        doing_used = 0;
				doing_addresses = 1;
			}
			// restore used data
			// have the row check cause eof sux
			else if(doing_used == 1)
			{
				used[row][column] = convert_uint64_t(temp);

				// this page was used need to issue fake write
				if(temp.compare("1") == 0)
				{
					pAddr = (row * BLOCK_SIZE + column * NV_PAGE_SIZE);
					vAddr = tempMap[pAddr];
					ChannelPacket *tempPacket = Ftl::translate(FAST_WRITE, vAddr, pAddr);
					controller->writeToPackage(tempPacket);
				}

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

void Ftl::queuesNotFull(void)
{
    read_queues_full = false;
    write_queues_full = false;   
    log->unlocked_up(locked_counter);
    locked_counter = 0;
}

void Ftl::GCReadDone(uint64_t vAddr)
{
    // an empty fucntion to make the compiler happy
}
