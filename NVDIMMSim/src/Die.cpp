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

//Die.cpp
//class file for die class

#include "Die.h"
#include "Channel.h"
#include "Controller.h"
#include "NVDIMM.h"

using namespace NVDSim;
using namespace std;

Die::Die(NVDIMM *parent, Logger *l, uint64_t idNum){
	id = idNum;
	parentNVDIMM = parent;
	log = l;

	sending = false;

	planes= vector<Plane>(PLANES_PER_DIE, Plane());

	currentCommands= vector<ChannelPacket *>(PLANES_PER_DIE, NULL);

	dataCyclesLeft= 0;
	deviceBeatsLeft= 0;
	controlCyclesLeft= new uint64_t[PLANES_PER_DIE];

	currentClockCycle= 0;

	critBeat = ((divide_params_64b((NV_PAGE_SIZE*8),DEVICE_WIDTH)-divide_params_64b((uint64_t)512,DEVICE_WIDTH)) * DEVICE_CYCLE) / CYCLE_TIME; // cache line is 64 bytes
}

void Die::attachToBuffer(Buffer *buff){
	buffer = buff;
}

void Die::receiveFromBuffer(ChannelPacket *busPacket){
	if (busPacket->busPacketType == DATA){
		planes[busPacket->plane].storeInData(busPacket);
	} else if (currentCommands[busPacket->plane] == NULL) {
		currentCommands[busPacket->plane] = busPacket;
		if (LOGGING)
		{
			// Tell the logger the access has now been processed.		        
			log->access_process(busPacket->virtualAddress, busPacket->physicalAddress, busPacket->package, busPacket->busPacketType);		
		}
		switch (busPacket->busPacketType){
			case READ:
		        case GC_READ:
			        planes[busPacket->plane].read(busPacket);
				controlCyclesLeft[busPacket->plane]= READ_CYCLES;
				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    if(busPacket->busPacketType == READ)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, READING);
				    }
				    else if(busPacket->busPacketType == GC_READ)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, GC_READING);
				    }
				}
				break;
			case WRITE:
			case GC_WRITE:
			    	planes[busPacket->plane].write(busPacket);
				parentNVDIMM->numWrites++;			
			        if((DEVICE_TYPE.compare("PCM") == 0 || DEVICE_TYPE.compare("P8P") == 0) && GARBAGE_COLLECT == 0)
				{
					controlCyclesLeft[busPacket->plane]= ERASE_CYCLES;
				}
				else
				{
					controlCyclesLeft[busPacket->plane]= WRITE_CYCLES;
				}
				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    if(busPacket->busPacketType == WRITE)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, WRITING);
				    }
				    else if(busPacket->busPacketType == GC_WRITE)
				    {
					log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, GC_WRITING);
				    }
				}
				break;
			case ERASE:
			        planes[busPacket->plane].erase(busPacket);
			        parentNVDIMM->numErases++;
			        controlCyclesLeft[busPacket->plane]= ERASE_CYCLES;

				// log the new state of this plane
				if(LOGGING && PLANE_STATE_LOG)
				{
				    log->log_plane_state(busPacket->virtualAddress, busPacket->package, busPacket->die, busPacket->plane, ERASING);
				}
				break;
			default:
				break;			
		}
	} else{
		ERROR("Die is busy");
		exit(1);
	}
}

int Die::isDieBusy(uint64_t plane){
    // not doing anything right now
    // if we're sending, then we're buffering and the channel between the buffer and the die
    // is busy and data can't be sent to the die right now
    if (currentCommands[plane] == NULL && sending == false){
	if(planes[plane].checkCacheReg() == true)
	{
	    return 0;
	}
	else
	{
	    return 3;
	}
    }
    // writing, send back a special number so we know that this plane can recieve data
    else if (currentCommands[plane] != NULL)
    {
	if(currentCommands[plane]->busPacketType == WRITE && planes[plane].checkCacheReg() == true)
	{
	    return 2;
	}
    }
    // busy but not writing so no data, please, we're all full up here
    return 1;
}

void Die::update(void){
	uint64_t i;
	ChannelPacket *currentCommand;

	for (i = 0 ; i < PLANES_PER_DIE ; i++){
	    bool no_reg_room = false; // is there a spare reg for the read data, if not we must wait
		currentCommand = currentCommands[i];
		if (currentCommand != NULL){
			if (controlCyclesLeft[i] <= 0){

				// Process each command based on the packet type.
				switch (currentCommand->busPacketType){
					case READ:
					    if(planes[currentCommand->plane].checkCacheReg())
					    {
						returnDataPackets.push(planes[currentCommand->plane].readFromData());
						no_reg_room = false;
					    }
					    else
					    {
						no_reg_room = true;
					    }
					    break;
					case GC_READ:
					    if(returnDataPackets.size() <= PLANES_PER_DIE)
					    {
						returnDataPackets.push(planes[currentCommand->plane].readFromData());
						parentNVDIMM->GCReadDone(currentCommand->virtualAddress);
					    }
					    break;
					case WRITE:	
						//call write callback					   
					    if (parentNVDIMM->WriteDataDone != NULL){
						(*parentNVDIMM->WriteDataDone)(parentNVDIMM->systemID, currentCommand->virtualAddress, currentClockCycle,true);
					    }
					    planes[currentCommand->plane].writeDone(currentCommand);
					    break;
				        case GC_WRITE:
					    break;
					case ERASE:
					    break;
					case DATA:
					    // Nothing to do.
					default:
					    break;
				}

				ChannelPacketType bpt = currentCommand->busPacketType;
				if ((bpt == WRITE) || (bpt == GC_WRITE) || (bpt == ERASE))
				{
					// For everything but READ/GC_READ, and DATA, the access is done at this point.
					// Note: for READ/GC_READ, this is handled in Controller::receiveFromChannel().
					// For DATA, this is handled as part of the WRITE in Plane.

					// Tell the logger the access is done.
					if (LOGGING)
					{
					    log->access_stop(currentCommand->virtualAddress, currentCommand->physicalAddress);
					    if(PLANE_STATE_LOG)
					    {
						log->log_plane_state(currentCommand->virtualAddress, currentCommand->package, currentCommand->die, currentCommand->plane, IDLE);
					    }
					}

					// Delete the memory allocated for the current command to prevent memory leaks.
					delete currentCommand;
				}

				if(no_reg_room == false)
				{
				    //sim output
				    currentCommands[i]= NULL;
				}
			}
			// sanity check
			if(controlCyclesLeft[i] > 0)
			{
			    controlCyclesLeft[i]--;
			}
		}
	}

	if (!returnDataPackets.empty())
	{
	    if( BUFFERED == true)
	    {
		// is there a read waiting for us and are we not doing something already
		if(deviceBeatsLeft == 0 && sending == false && 		
		   (buffer->dataReady(returnDataPackets.front()->die, returnDataPackets.front()->plane) == false ||
		    currentCommands[returnDataPackets.front()->plane] != NULL))
		{
		    dataCyclesLeft = divide_params_64b(DEVICE_CYCLE,CYCLE_TIME);
		    deviceBeatsLeft = divide_params_64b((NV_PAGE_SIZE*8),DEVICE_WIDTH);
		    sending = true;
		}
		    
		if(dataCyclesLeft == 0 && deviceBeatsLeft > 0){
		    bool success = false;
		    success = buffer->sendPiece(BUFFER, 0, id, returnDataPackets.front()->plane);
		    if(success == true)
		    {
			deviceBeatsLeft--;
			dataCyclesLeft = divide_params_64b(DEVICE_CYCLE,CYCLE_TIME);
		    }
		    else
		    {
		      //cout << "send to buffer failed at die " << id << "\n";
		    }
		}
		
		if(dataCyclesLeft > 0 && deviceBeatsLeft > 0){
		    dataCyclesLeft--;
		}
	    }
	    // not buffered
	    else
	    {
		if(buffer->channel->hasChannel(BUFFER, id)){
		    if(dataCyclesLeft == 0){
			if(LOGGING && PLANE_STATE_LOG)
			{
			    log->log_plane_state(returnDataPackets.front()->virtualAddress, 
						 returnDataPackets.front()->package, 
						 returnDataPackets.front()->die, 
						 returnDataPackets.front()->plane, 
						 IDLE);
			}
			planes[returnDataPackets.front()->plane].dataGone();
			buffer->channel->sendToController(returnDataPackets.front());
			buffer->channel->releaseChannel(BUFFER, id);		
			returnDataPackets.pop();
		    }
		    if(CRIT_LINE_FIRST && dataCyclesLeft == critBeat)
		    {
			buffer->channel->controller->returnCritLine(returnDataPackets.front());
		    }
		    dataCyclesLeft--;
		}else{
		    // is there a read waiting for us and are we not doing something already
		    if((buffer->channel->controller->dataReady(returnDataPackets.front()->package, returnDataPackets.front()->die, 
							      returnDataPackets.front()->plane) == 0 ||
			currentCommands[returnDataPackets.front()->plane] != NULL))
		    {
			if(buffer->channel->obtainChannel(id, BUFFER, NULL))
			{
			    dataCyclesLeft = (divide_params_64b((NV_PAGE_SIZE*8),DEVICE_WIDTH) * DEVICE_CYCLE) / CYCLE_TIME;
			}
		    }
		}
	    }
	}
}

void Die::bufferDone(uint64_t plane)
{
    //sanity check
    if(pendingDataPackets.front()->plane == plane)
    {
	buffer->sendToController(pendingDataPackets.front());
	pendingDataPackets.pop();
    }
    else
    {
	ERROR("Tried to complete a pending data packet for the wrong plane, things got out of order somehow");
    }
}

void Die::bufferLoaded()
{
    pendingDataPackets.push(returnDataPackets.front());
    if(LOGGING && PLANE_STATE_LOG)
    {
	log->log_plane_state(returnDataPackets.front()->virtualAddress, returnDataPackets.front()->package, returnDataPackets.front()->die, returnDataPackets.front()->plane, IDLE);
    }
    planes[returnDataPackets.front()->plane].dataGone();
    returnDataPackets.pop();	
    sending = false;
}

void Die::critLineDone()
{
    if(CRIT_LINE_FIRST)
    {
	buffer->channel->controller->returnCritLine(returnDataPackets.front());
    }
}

void Die::writeToPlane(ChannelPacket *packet)
{
    planes[packet->plane].write(packet);
}
