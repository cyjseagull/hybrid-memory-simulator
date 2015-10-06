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

//Controller.cpp
//Class files for controller

#include "Controller.h"
#include "NVDIMM.h"
#include "FrontBuffer.h"

using namespace NVDSim;

Controller::Controller(NVDIMM* parent, Logger* l){
	parentNVDIMM = parent;
	log = l;

	channelBeatsLeft = vector<uint64_t>(NUM_PACKAGES, 0);

	readQueues = vector<vector<list <ChannelPacket *> > >(NUM_PACKAGES, vector<list<ChannelPacket *> >(DIES_PER_PACKAGE, list<ChannelPacket * >()));
	writeQueues = vector<vector<list <ChannelPacket *> > >(NUM_PACKAGES, vector<list<ChannelPacket *> >(DIES_PER_PACKAGE, list<ChannelPacket * >()));
	//writeQueues = vector<list <ChannelPacket *> >(DIES_PER_PACKAGE, list<ChannelPacket *>());
	outgoingPackets = vector<ChannelPacket *>(NUM_PACKAGES, 0);

	pendingPackets = vector<list <ChannelPacket *> >(NUM_PACKAGES, list<ChannelPacket *>());

	paused = new bool [NUM_PACKAGES];
	die_pointers = new uint64_t [NUM_PACKAGES];
	for(uint64_t i = 0; i < NUM_PACKAGES; i++)
	{
	    paused[i] = false;
	    die_pointers[i] = 0;
	}

	done = 0;
	die_counter = 0;
	currentClockCycle = 0;
}

void Controller::attachPackages(vector<Package> *packages){
	this->packages= packages;
}

void Controller::attachFrontBuffer(FrontBuffer *fb){
    this->front_buffer = fb;
}

void Controller::returnReadData(const FlashTransaction  &trans){
	if(parentNVDIMM->ReturnReadData!=NULL){
	    (*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, trans.address, currentClockCycle, true);
	}
	parentNVDIMM->numReads++;
}

void Controller::returnUnmappedData(const FlashTransaction  &trans){
	if(parentNVDIMM->ReturnReadData!=NULL){
	    (*parentNVDIMM->ReturnReadData)(parentNVDIMM->systemID, trans.address, currentClockCycle, false);
	}
	parentNVDIMM->numReads++;
}

void Controller::returnCritLine(ChannelPacket *busPacket){
	if(parentNVDIMM->CriticalLineDone!=NULL){
	    (*parentNVDIMM->CriticalLineDone)(parentNVDIMM->systemID, busPacket->virtualAddress, currentClockCycle, true);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> erase_energy,
		vector<double> vpp_idle_energy, vector<double> vpp_access_energy, vector<double> vpp_erase_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(6, vector<double>(NUM_PACKAGES, 0.0));
		for(uint64_t i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = erase_energy[i] * VCC;
			power_data[3][i] = vpp_idle_energy[i] * VPP;
			power_data[4][i] = vpp_access_energy[i] * VPP;
			power_data[5][i] = vpp_erase_energy[i] * VPP;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> vpp_idle_energy,
		vector<double> vpp_access_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(4, vector<double>(NUM_PACKAGES, 0.0));
		for(uint64_t i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = vpp_idle_energy[i] * VPP;
			power_data[3][i] = vpp_access_energy[i] * VPP;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy, vector<double> erase_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(3, vector<double>(NUM_PACKAGES, 0.0));
		for(uint64_t i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
			power_data[2][i] = erase_energy[i] * VCC;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::returnPowerData(vector<double> idle_energy, vector<double> access_energy) {
	if(parentNVDIMM->ReturnPowerData!=NULL){
		vector<vector<double>> power_data = vector<vector<double>>(2, vector<double>(NUM_PACKAGES, 0.0));
		for(uint64_t i = 0; i < NUM_PACKAGES; i++)
		{
			power_data[0][i] = idle_energy[i] * VCC;
			power_data[1][i] = access_energy[i] * VCC;
		}
		(*parentNVDIMM->ReturnPowerData)(parentNVDIMM->systemID, power_data, currentClockCycle, false);
	}
}

void Controller::receiveFromChannel(ChannelPacket *busPacket){
	// READ is now done. Log it and call delete
	if(LOGGING == true)
	{
		log->access_stop(busPacket->virtualAddress, busPacket->physicalAddress);
	}

	// Put in the returnTransaction queue 
	switch (busPacket->busPacketType)
	{
		case READ:
			returnTransaction.push_back(FlashTransaction(RETURN_DATA, busPacket->virtualAddress, busPacket->data));
			break;
		case GC_READ:
			// Nothing to do.
			break;
		default:
			ERROR("Illegal busPacketType " << busPacket->busPacketType << " in Controller::receiveFromChannel\n");
			abort();
			break;
	}

	// Delete the ChannelPacket since READ is done. This must be done to prevent memory leaks.
	delete busPacket;
}

// this is only called on a write as the name suggests
bool Controller::checkQueueWrite(ChannelPacket *p)
{
    if(CTRL_SCHEDULE)
    {
	if ((writeQueues[p->package][p->die].size() + 1 < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0))
	    return true;
	else
	    return false;
    }
    else
    {
	if ((readQueues[p->package][p->die].size() + 1 < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
	    return true;
	else
	    return false;
    }
}

bool Controller::addPacket(ChannelPacket *p){
    if(CTRL_SCHEDULE)
    {
	// If there is not room in the command queue for this packet, then return false.
	// If CTRL_QUEUE_LENGTH is 0, then infinite queues are allowed.
	switch (p->busPacketType)
	{
	case READ:
        case ERASE:
	    if ((readQueues[p->package][p->die].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
		readQueues[p->package][p->die].push_back(p);
	    else	
	        return false;
	    break;
        case WRITE:
        case DATA:
	     if ((writeQueues[p->package][p->die].size() < CTRL_WRITE_QUEUE_LENGTH) || (CTRL_WRITE_QUEUE_LENGTH == 0))
	     {
		 // search the write queue to check if this write overwrites some other write
		 // this should really only happen if we're doing in place writing though (no gc)
		 if(!GARBAGE_COLLECT)
		 {
		     list<ChannelPacket *>::iterator it;
		     for (it = writeQueues[p->package][p->die].begin(); it != writeQueues[p->package][p->die].end(); it++)
		     {
			 if((*it)->virtualAddress == p->virtualAddress && (*it)->busPacketType == p->busPacketType)
			 {
			     if(LOGGING)
			     {		
				 // access_process for that write is called here since its over now.
				 log->access_process((*it)->virtualAddress, (*it)->physicalAddress, (*it)->package, WRITE);
				 
				 // stop_process for that write is called here since its over now.
				 log->access_stop((*it)->virtualAddress, (*it)->physicalAddress);
			     }
			     //call write callback
			     if (parentNVDIMM->WriteDataDone != NULL){
				 (*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, (*it)->virtualAddress, currentClockCycle,true);
			     }
			     writeQueues[(*it)->package][(*it)->die].erase(it, it++);
			     break;
			 }
		     }   
		 }
		 writeQueues[p->package][p->die].push_back(p);
		 break;
	     }
	     else
	     {
		 return false;
	     }
	case GC_READ:
	case GC_WRITE:
	    // Try to push the gc stuff to the front of the read queue in order to give them priority
	    if ((readQueues[p->package][p->die].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
		readQueues[p->package][p->die].push_front(p);	
	    else
		return false;
	    break;
	default:
	    ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
	    break;
	}
    
	if(LOGGING && QUEUE_EVENT_LOG)
	{
	    switch (p->busPacketType)
	    {
	    case READ:
	    case GC_READ:
	    case GC_WRITE:
	    case ERASE:
		log->log_ctrl_queue_event(false, p->package, &readQueues[p->package][p->die]);
		break;
	    case WRITE:
	    case DATA:
		log->log_ctrl_queue_event(true, p->package, &writeQueues[p->package][p->die]);
		break;
	    default:
		ERROR("Illegal busPacketType " << p->busPacketType << " in Controller::receiveFromChannel\n");
		break;
	    }
	}
	return true;
    }
    // Not scheduling so everything goes to the read queue
    else
    {
	if ((readQueues[p->package][p->die].size() < CTRL_READ_QUEUE_LENGTH) || (CTRL_READ_QUEUE_LENGTH == 0))
	{
	    readQueues[p->package][p->die].push_back(p);
    
	    if(LOGGING)
	    {
		log->ctrlQueueSingleLength(p->package, p->die, readQueues[p->package][p->die].size());
		if(QUEUE_EVENT_LOG)
		{
		    log->log_ctrl_queue_event(false, p->package, &readQueues[p->package][p->die]);
		}
	    }
	    return true;
	}
	else
	{
	    return false;
	}
    }
}

// just cleaning up some of the code
// this was repeated half a dozen times in the code below
bool Controller::nextDie(uint64_t package)
{
    die_pointers[package]++;
    if (die_pointers[package] >= DIES_PER_PACKAGE)
    {
	die_pointers[package] = 0;
    }
    die_counter++;
    // if we loop the number of dies, then we're done
    if (die_counter >= DIES_PER_PACKAGE)
    {
	return 1;
    }
    return 0;
}

void Controller::update(void){
    // schedule the next operation for each die
    if(CTRL_SCHEDULE)
    {
	bool write_queue_handled = false;
	uint64_t i;	
	//loop through the channels to find a packet for each
	for (i = 0; i < NUM_PACKAGES; i++)
	{
	    // loop through the dies per package to find the packet
	    die_counter = 0;
	    done = 0;
	    while (!done)
	    {
		// do we need to issue a write
		// *** NOTE: We need to review this write condition for out new design ***
		if((CTRL_WRITE_ON_QUEUE_SIZE == true && writeQueues[i][die_pointers[i]].size() >= CTRL_WRITE_QUEUE_LIMIT)) //||
		    //(CTRL_WRITE_ON_QUEUE_SIZE == false && writeQueues[i][die_pointers[i]].size() >= CTRL_WRITE_QUEUE_LENGTH-1))
		{
		    if (!writeQueues[i][die_pointers[i]].empty() && outgoingPackets[i]==NULL){
			//if we can get the channel
			if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, writeQueues[i][die_pointers[i]].front())){
			    outgoingPackets[i] = writeQueues[i][die_pointers[i]].front();
			    if(LOGGING && QUEUE_EVENT_LOG)
			    {
				log->log_ctrl_queue_event(true, writeQueues[i][die_pointers[i]].front()->package, &writeQueues[i][die_pointers[i]]);
			    }
			    writeQueues[i][die_pointers[i]].pop_front();
			    parentNVDIMM->queuesNotFull();
			    
			    switch (outgoingPackets[i]->busPacketType){
			    case DATA:
				// Note: NV_PAGE_SIZE is multiplied by 8 since the parameter is given in bytes and we need it in bits.
				channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8),CHANNEL_WIDTH); 
				break;
			    default:
				channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
				break;
			    }
			    // managed to place something so we're done with this channel
			    // advance the die pointer since this die is now busy
			    die_pointers[i]++;
			    if (die_pointers[i] >= DIES_PER_PACKAGE)
			    {
				die_pointers[i] = 0;
			    }
			    done = 1;
			}
			// if we can't get the channel for that die, try the next die			
			else
			{
			    done = nextDie(i);
			}
		    }
		    // this queue is empty, move on
		    else
		    {
			done = nextDie(i);
		    }
		}
		// if we don't have to issue a write check to see if there is a read to send
		// we're reusing the same die pointer for reads and writes because if a die was just given a read or write
		// it can't do anything else with it so the die counters sort've act like a dies in use marker
		else if (!readQueues[i][die_pointers[i]].empty() && outgoingPackets[i]==NULL){
		    if(queue_access_counter == 0 && readQueues[i][die_pointers[i]].front()->busPacketType != GC_READ && 
		       readQueues[i][die_pointers[i]].front()->busPacketType != GC_WRITE && !writeQueues[i][die_pointers[i]].empty())
		    {
			//see if this read can be satisfied by something in the write queue
			list<ChannelPacket *>::iterator it;
			for (it = writeQueues[i][die_pointers[i]].begin(); it != writeQueues[i][die_pointers[i]].end(); it++)
			{
			    if((*it)->virtualAddress == readQueues[i][die_pointers[i]].front()->virtualAddress)
			    {
				if(LOGGING)
				{		
				    // access_process for the read we're satisfying  is called here since we're doing it here.
				    log->access_process(readQueues[i][die_pointers[i]].front()->virtualAddress, readQueues[i][die_pointers[i]].front()->physicalAddress, 
							readQueues[i][die_pointers[i]].front()->package, READ);
				}
				queue_access_counter = QUEUE_ACCESS_CYCLES;
				write_queue_handled = true;
				break;
				// done for now with this channel and die but don't advance the die counter
				// cause we have to get through the queue access counter for it
				done = 1;
			    }
			}
		    }
		    else if(queue_access_counter > 0)
		    {
			queue_access_counter--;
			write_queue_handled = true;
			if(queue_access_counter == 0)
			{
			    if(LOGGING)
			    {
				// stop_process for this read is called here since this ends now.
				log->access_stop(readQueues[i][die_pointers[i]].front()->virtualAddress, readQueues[i][die_pointers[i]].front()->virtualAddress);
			    }
			    
			    returnReadData(FlashTransaction(RETURN_DATA, readQueues[i][die_pointers[i]].front()->virtualAddress, readQueues[i][die_pointers[i]].front()->data));
			    readQueues[i][die_pointers[i]].pop_front();
			    parentNVDIMM->queuesNotFull();
			    // managed to place something so we're done with this channel
			    // advance the die pointer since this die is now busy
			    die_pointers[i]++;
			    if (die_pointers[i] >= DIES_PER_PACKAGE)
			    {
				die_pointers[i] = 0;
			    }
			    done = 1;
			}
		    }
		    else if(!write_queue_handled)
		    {
			//if we can get the channel
			if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, readQueues[i][die_pointers[i]].front())){
			    outgoingPackets[i] = readQueues[i][die_pointers[i]].front();
			    if(LOGGING && QUEUE_EVENT_LOG)
			    {
				log->log_ctrl_queue_event(false, readQueues[i][die_pointers[i]].front()->package, &readQueues[i][die_pointers[i]]);
			    }
			    readQueues[i][die_pointers[i]].pop_front();
			    parentNVDIMM->queuesNotFull();
			    
			    channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
			    // managed to place something so we're done with this channel
			    // advance the die pointer since this die is now busy
			    die_pointers[i]++;
			    if (die_pointers[i] >= DIES_PER_PACKAGE)
			    {
				die_pointers[i] = 0;
			    }
			    done = 1;
			}
			// couldn't get the channel so go to the next die
			else
			{
			    done = nextDie(i);
			}
		    }
		}
		// if there are no reads to send see if we're allowed to send a write instead
		else if (CTRL_IDLE_WRITE == true && !writeQueues[i][die_pointers[i]].empty() && outgoingPackets[i]==NULL){
		    //if we can get the channel
		    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, writeQueues[i][die_pointers[i]].front())){
			outgoingPackets[i] = writeQueues[i][die_pointers[i]].front();
			if(LOGGING && QUEUE_EVENT_LOG)
			{
			    log->log_ctrl_queue_event(true, writeQueues[i][die_pointers[i]].front()->package, &writeQueues[i][die_pointers[i]]);
			}
			writeQueues[i][die_pointers[i]].pop_front();
			// successfully issued the write so increment the die pointer
			die_pointers[i]++;
			if (die_pointers[i] >= DIES_PER_PACKAGE)
			{
			    die_pointers[i] = 0;
			}
			parentNVDIMM->queuesNotFull();
			
			switch (outgoingPackets[i]->busPacketType){
			case DATA:
				// Note: NV_PAGE_SIZE is multiplied by 8 since the parameter is given in bytes and we need it in bits.
			    channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8),CHANNEL_WIDTH); 
			    break;
			default:
			    channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
			    break;
			}
			// managed to place something so we're done with this channel
			// advance the die pointer since this die is now busy
			die_pointers[i]++;
			if (die_pointers[i] >= DIES_PER_PACKAGE)
			{
			    die_pointers[i] = 0;
			}
			done = 1;
		    }
		    // couldn't get the channel so go to the next die
		    else
		    {
			done = nextDie(i);
		    }
		}
		// queue was empty, move on
		else
		{
		    done = nextDie(i);
		}
	    }
	}
    }
    // not scheduling so everything comes from the read queue
    else
    {
	uint64_t i;	
	//Look through queues and send oldest packets to the appropriate channel
	for (i = 0; i < NUM_PACKAGES; i++){
	    // loop through the dies per package to find the packet
	    die_counter = 0;
	    done = 0;
	    while (!done)
	    {
		if (!readQueues[i][die_pointers[i]].empty() && outgoingPackets[i]==NULL){
		    //if we can get the channel
		    if ((*packages)[i].channel->obtainChannel(0, CONTROLLER, readQueues[i][die_pointers[i]].front())){
			outgoingPackets[i] = readQueues[i][die_pointers[i]].front();
			if(LOGGING && QUEUE_EVENT_LOG)
			{
			    switch (readQueues[i][die_pointers[i]].front()->busPacketType)
			    {
			    case READ:
			    case GC_READ:
			    case ERASE:
				log->log_ctrl_queue_event(false, readQueues[i][die_pointers[i]].front()->package, &readQueues[i][die_pointers[i]]);
				break;
			    case WRITE:
			    case GC_WRITE:
			    case DATA:
				log->log_ctrl_queue_event(true, readQueues[i][die_pointers[i]].front()->package, &readQueues[i][die_pointers[i]]);
				break;
			    case FAST_WRITE:
				break;
			    }
			}
			readQueues[i][die_pointers[i]].pop_front();
			parentNVDIMM->queuesNotFull();
			if(BUFFERED)
			{
			  switch (outgoingPackets[i]->busPacketType){
			    case DATA:
				// Note: NV_PAGE_SIZE is multiplied by 8 since the parameter is given in bytes and we need it in bits.
				channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8),CHANNEL_WIDTH); 
				break;
			    default:
				channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,CHANNEL_WIDTH);
				break;
			    }			    
			}
			else
			{
			    switch (outgoingPackets[i]->busPacketType){
			    case DATA:
				// Note: NV_PAGE_SIZE is multiplied by 8 since the parameter is given in bytes and we need it in bits.
				channelBeatsLeft[i] = divide_params((NV_PAGE_SIZE*8),DEVICE_WIDTH); 
				break;
			    default:
				channelBeatsLeft[i] = divide_params(COMMAND_LENGTH,DEVICE_WIDTH);
				break;
			    }
			}
			// managed to place something so we're done with this channel
			// advance the die pointer since this die is now busy
			die_pointers[i]++;
			if (die_pointers[i] >= DIES_PER_PACKAGE)
			{
			    die_pointers[i] = 0;
			}
			done = 1;
		    }
		    // couldn't get the channel so... Next die
		    else
		    {
			done = nextDie(i);
		    }
		}
		// this queue is empty so move on
		else
		{
		    done = nextDie(i);
		}
	    }
	}
    }
	
    //Use the buffer code for the NVDIMMS to calculate the actual transfer time
    if(BUFFERED)
    {	
	uint64_t i;
	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
	    if (outgoingPackets[i] != NULL){
		if(paused[outgoingPackets[i]->package] == true && 
		   !(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->busPacketType, 
										   outgoingPackets[i]->die))
		{
		    if ((*packages)[outgoingPackets[i]->package].channel->obtainChannel(0, CONTROLLER, outgoingPackets[i])){
			paused[outgoingPackets[i]->package] = false;
		    }
		}
		if ((*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0) && paused[outgoingPackets[i]->package] == false){
		    if (channelBeatsLeft[i] == 0){
			(*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
			pendingPackets[i].push_back(outgoingPackets[i]);
			outgoingPackets[i] = NULL;
		    }else if ((*packages)[outgoingPackets[i]->package].channel->notBusy()){
			    if(CUT_THROUGH)
			    {
				    if(!(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->busPacketType, 
												       outgoingPackets[i]->die))
				    {
					    (*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, outgoingPackets[i]->busPacketType, 
													outgoingPackets[i]->die, outgoingPackets[i]->plane);
					    channelBeatsLeft[i]--;
				    }
				    else
				    {
					    (*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
					    paused[outgoingPackets[i]->package] = true;
				    }
			    }
			    else
			    {
				if((outgoingPackets[i]->busPacketType == DATA && channelBeatsLeft[i] == divide_params((NV_PAGE_SIZE*8),CHANNEL_WIDTH)) ||
				   (outgoingPackets[i]->busPacketType != DATA && channelBeatsLeft[i] == divide_params(COMMAND_LENGTH,CHANNEL_WIDTH)))
				{
				    if(!(*packages)[outgoingPackets[i]->package].channel->isBufferFull(CONTROLLER, outgoingPackets[i]->busPacketType, 
												       outgoingPackets[i]->die))
				    {
					(*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, outgoingPackets[i]->busPacketType, 
												    outgoingPackets[i]->die, outgoingPackets[i]->plane);
					channelBeatsLeft[i]--;
				    }
				    else
				    {
					(*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
					paused[outgoingPackets[i]->package] = true;
				    }
				}
				else
				{
				    (*packages)[outgoingPackets[i]->package].channel->sendPiece(CONTROLLER, outgoingPackets[i]->busPacketType, 
												   outgoingPackets[i]->die, outgoingPackets[i]->plane);
				    channelBeatsLeft[i]--;					    
				}
			    }
		    }
		}
	    }
	}
	//Directly calculate the expected transfer time 
    }
    else
    {
	// BUFFERED NOT TRUE CASE...
	uint64_t i;
	//Check for commands/data on a channel. If there is, see if it is done on channel
	for (i= 0; i < outgoingPackets.size(); i++){
	    if (outgoingPackets[i] != NULL && (*packages)[outgoingPackets[i]->package].channel->hasChannel(CONTROLLER, 0)){
		channelBeatsLeft[i]--;
		if (channelBeatsLeft[i] == 0){
		    (*packages)[outgoingPackets[i]->package].channel->sendToBuffer(outgoingPackets[i]);
		    (*packages)[outgoingPackets[i]->package].channel->releaseChannel(CONTROLLER, 0);
		    outgoingPackets[i] = NULL;
		}
	    }
	}
    }

    //See if any read data is ready to return
    while (!returnTransaction.empty()){
	if(FRONT_BUFFER)
	{
	    // attempt to add the return transaction to the host channel buffer
	    bool return_success = front_buffer->addTransaction(returnTransaction.back());
	    if(return_success)
	    {
		// attempt was successful so pop the transaction and try again
		returnTransaction.pop_back();
	    }
	    else
	    {
		// attempt failed so stop trying to add things for now
		break;
	    }
	}
	else
	{
	    //call return callback
	    returnReadData(returnTransaction.back());
	    returnTransaction.pop_back();
	}
    }
}

bool Controller::dataReady(uint64_t package, uint64_t die, uint64_t plane)
{
    if(!readQueues[package][die].empty())
    {
	if(readQueues[package][die].front()->busPacketType == READ && readQueues[package][die].front()->plane == plane)
	{
	    return 1;
	}
	return 0;
    }
    return 0;
}

void Controller::sendQueueLength(void)
{
    vector<vector<uint64_t> > temp = vector<vector<uint64_t> >(NUM_PACKAGES, vector<uint64_t>(DIES_PER_PACKAGE, 0));
	for(uint64_t i = 0; i < readQueues.size(); i++)
	{
	    for(uint64_t j = 0; j < readQueues[i].size(); j++)
	    {
		temp[i][j] = writeQueues[i][j].size();
	    }
	}
	if(LOGGING == true)
	{
		log->ctrlQueueLength(temp);
	}
}

void Controller::writeToPackage(ChannelPacket *packet)
{
	return (*packages)[packet->package].dies[packet->die]->writeToPlane(packet);
}

void Controller::bufferDone(uint64_t package, uint64_t die, uint64_t plane)
{
	for (uint64_t i = 0; i < pendingPackets.size(); i++){
		std::list<ChannelPacket *>::iterator it;
		for(it = pendingPackets[i].begin(); it != pendingPackets[i].end(); it++){
		    if ((*it) != NULL && (*it)->package == package && (*it)->die == die && (*it)->plane == plane){
				(*packages)[(*it)->package].channel->sendToBuffer((*it));
				pendingPackets[i].erase(it);
				break;
		    }
		}
	}
}
