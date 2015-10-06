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

//Buffer.cpp
//Buffer functions

#include "Buffer.h"

using namespace std;
using namespace NVDSim;

Buffer::Buffer(uint64_t i){

    id = i;

    outData = vector<list <BufferPacket *> >(DIES_PER_PACKAGE, list<BufferPacket *>());
    inData = vector<list <BufferPacket *> >(DIES_PER_PACKAGE, list<BufferPacket *>());

    outDataSize = new uint64_t [DIES_PER_PACKAGE];
    inDataSize = new uint64_t [DIES_PER_PACKAGE];
    cyclesLeft = new uint64_t [DIES_PER_PACKAGE];
    outDataLeft = new uint64_t [DIES_PER_PACKAGE];
    critData = new uint64_t [DIES_PER_PACKAGE];
    inDataLeft = new uint64_t [DIES_PER_PACKAGE];
    waiting =  new bool [DIES_PER_PACKAGE];

    for(uint64_t i = 0; i < DIES_PER_PACKAGE; i++){
	outDataSize[i] = 0;
	inDataSize[i] = 0;
	cyclesLeft[i] = 0;
	outDataLeft[i] = 0;
	critData[i] = 0;
	inDataLeft[i] = 0;
	waiting[i] = false;
   }

    sendingDie = 0;
    sendingPlane = 0;

    dieLookingUp = DIES_PER_PACKAGE + 1;
    lookupTimeLeft = BUFFER_LOOKUP_CYCLES;
    

}

void Buffer::attachDie(Die *d){
    dies.push_back(d);
}

void Buffer::attachChannel(Channel *c){
    channel = c;
}

void Buffer::sendToDie(ChannelPacket *busPacket){
    dies[busPacket->die]->receiveFromBuffer(busPacket);
}

void Buffer::sendToController(ChannelPacket *busPacket){
    channel->sendToController(busPacket);
}

bool Buffer::sendPiece(SenderType t, int type, uint64_t die, uint64_t plane){
    if(t == CONTROLLER)
    {
      if(IN_BUFFER_SIZE == 0 || inDataSize[die] <= (IN_BUFFER_SIZE-(CHANNEL_WIDTH)))
	{
	    if(!inData[die].empty() && inData[die].back()->type == type && inData[die].back()->plane == plane &&
	       type == 5 && inData[die].back()->number < (NV_PAGE_SIZE*8))
	    {
		inData[die].back()->number = inData[die].back()->number + CHANNEL_WIDTH;
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    else if(!inData[die].empty() && inData[die].back()->type == type && inData[die].back()->plane == plane && 
		    type != 5 && inData[die].back()->number < COMMAND_LENGTH)
	    {
		inData[die].back()->number = inData[die].back()->number + CHANNEL_WIDTH;
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    else
	    {	
		BufferPacket* myPacket = new BufferPacket();
		myPacket->type = type;
		myPacket->number = CHANNEL_WIDTH;
		myPacket->plane = plane;
		inData[die].push_back(myPacket);
		inDataSize[die] = inDataSize[die] + CHANNEL_WIDTH;
	    }
	    return true;
	}
	else
	{
	    //cout << "controller sent packet to buffer " << die << " and plane " << plane << " that didn't fit \n";
	    //cout << "packet type was " << type << "\n";
	    return false;
	}
    }
    else if(t == BUFFER)
    {
	if(OUT_BUFFER_SIZE == 0 || outDataSize[die] <= (OUT_BUFFER_SIZE-DEVICE_WIDTH))
	{
	    if(!outData[die].empty() && outData[die].back()->type == type && outData[die].back()->plane == plane &&
	       outData[die].back()->number < (NV_PAGE_SIZE*8)){
		outData[die].back()->number = outData[die].back()->number + DEVICE_WIDTH;
		outDataSize[die] = outDataSize[die] + DEVICE_WIDTH;
		// if ths was the last piece of this packet, tell the die
		if( outData[die].back()->number >= (NV_PAGE_SIZE*8))
		{
		    dies[die]->bufferLoaded();
		}
	    }else{
		BufferPacket* myPacket = new BufferPacket();
		myPacket->type = type;
		myPacket->number = DEVICE_WIDTH;
		myPacket->plane = plane;
		outData[die].push_back(myPacket);
		outDataSize[die] = outDataSize[die] + DEVICE_WIDTH;
	    }
	    return true;
	}
	else
	{
	    return false;
	}
    }
    return false;
}

bool Buffer::isFull(SenderType t, ChannelPacketType bt, uint64_t die)
{
    if(t == CONTROLLER)
    {
      if(IN_BUFFER_SIZE == 0)
      {
	      return false;
      }
      else if(CUT_THROUGH && inDataSize[die] <= (IN_BUFFER_SIZE-CHANNEL_WIDTH) && waiting[die] == false)
      {
	      return false;
      }
      else if(!CUT_THROUGH && bt == 5 && inDataSize[die] <= (IN_BUFFER_SIZE-(divide_params((NV_PAGE_SIZE*8), CHANNEL_WIDTH)*CHANNEL_WIDTH)))
      {
	      return false;
      }
      else if(!CUT_THROUGH && bt != 5 && inDataSize[die] <= (IN_BUFFER_SIZE-(divide_params(COMMAND_LENGTH, CHANNEL_WIDTH)*CHANNEL_WIDTH)))
      {
	      return false;
      }
      else
      {
	      //return false;
	      return true;
      }
    }
    else if(t == BUFFER)
    {
	    if(OUT_BUFFER_SIZE == 0)
	    {
		    return false;
	    }
	    if(CUT_THROUGH && outDataSize[die] <= (OUT_BUFFER_SIZE-DEVICE_WIDTH))
	    {
		    return false;
	    }
	    else if(!CUT_THROUGH && outDataSize[die] <= (OUT_BUFFER_SIZE-(divide_params((NV_PAGE_SIZE*8), DEVICE_WIDTH)*DEVICE_WIDTH)))
	    {
		    return false;
	    }
	    else
	    {
		    return true;
	    }
    }
    return true;
}
	    
void Buffer::update(void){
    for(uint64_t i = 0; i < DIES_PER_PACKAGE; i++){
	// moving data into a die
	//==================================================================================
	// if we're not already busy writing stuff
	if(!inData[i].empty())
	{
	    // *NOTE* removed check for inDataLeft == inData which i think was to make sure this didn't get called when things where just empty
	    // if it is a command, set the number of beats if we've not set them yet
	    if(inData[i].front()->type != 5)
	    {
		// first time we've dealt with this command so we need to set our values
		if(inDataLeft[i] == 0 && waiting[i] != true && inData[i].front()->number >= COMMAND_LENGTH)
		{
			if(BUFFER_LOOKUP_CYCLES != 0)
			{
				if(dieLookingUp == DIES_PER_PACKAGE+1)
				{
					dieLookingUp = i;
					lookupTimeLeft = BUFFER_LOOKUP_CYCLES;
				}
				else if(dieLookingUp == i)
				{
					if(lookupTimeLeft > 0)
					{
						lookupTimeLeft--;
					}
					if(lookupTimeLeft == 0)
					{
						dieLookingUp = DIES_PER_PACKAGE+1;
						inDataLeft[i] = COMMAND_LENGTH;
						cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
						processInData(i);
					}
				}
			}
			else
			{
				inDataLeft[i] = COMMAND_LENGTH;
				cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
				processInData(i);
			}
		}
		// need to make sure either enough data has been transfered to the buffer to warrant
		// sending out more data or all of the data for this particular packet has already
		// been loaded into the buffer
		else if(inData[i].front()->number >= ((COMMAND_LENGTH-inDataLeft[i])+DEVICE_WIDTH) ||
			(inData[i].front()->number >= COMMAND_LENGTH))
		{
		    processInData(i);
		}
	    }
	    // its not a command but it is the first time we've dealt with this data
	    else if(inDataLeft[i] == 0 && waiting[i] != true)
	    {
		// cut through routing enabled
		// starting the transaction as soon as we have enough data to send one beat
		if(CUT_THROUGH && inData[i].front()->number >= DEVICE_WIDTH)
		{
		    inDataLeft[i] = (NV_PAGE_SIZE*8);
		    cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    processInData(i);
		}
		// don't do cut through routing
		// wait until we have the whole page before sending
		else if(!CUT_THROUGH && inData[i].front()->number >= (NV_PAGE_SIZE*8))
		{
		    inDataLeft[i] = (NV_PAGE_SIZE*8);
		    cyclesLeft[i] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
		    processInData(i);
		}
	    }
	    // its not a command and its not the first time we've seen it but we still need to make sure either
	    // there is enough data to warrant sending out the data or all of the data for this particular packet has already
	    // been loaded into the buffer	    
	    else if (inData[i].front()->number >= (((NV_PAGE_SIZE*8)-inDataLeft[i])+DEVICE_WIDTH) ||
		     (inData[i].front()->number >= (NV_PAGE_SIZE*8)))
	    {
		processInData(i);
	    }
	}    
	    	
	// moving data away from die
	//====================================================================================
	// first scan through to see if we have stuff to send if we're not busy
	if(!outData[i].empty())
	{
	    // we're sending data as quickly as we get it
	    if(CUT_THROUGH && outData[i].front()->number >= CHANNEL_WIDTH)
	    {
		prepareOutChannel(i);
	    }
	    // waiting to send data until we have a whole page to send
	    else if(!CUT_THROUGH && outData[i].front()->number >= (NV_PAGE_SIZE*8))
	    {
		prepareOutChannel(i);
	    }
	}
    }
}

void Buffer::prepareOutChannel(uint64_t die)
{
    // see if we have control of the channel
    if (channel->hasChannel(BUFFER, id) && sendingDie == die && sendingPlane == outData[die].front()->plane)
    {
	if((outData[die].front()->number >= (((NV_PAGE_SIZE*8)-outDataLeft[die])+CHANNEL_WIDTH)) ||
	   (outData[die].front()->number >= (NV_PAGE_SIZE*8)))
	{
	    processOutData(die);
	}
    }
    // if we don't have the channel, get it
    else if (channel->obtainChannel(id, BUFFER, NULL)){
	outDataLeft[die] = (NV_PAGE_SIZE*8);
	sendingDie = die;
	sendingPlane = outData[die].front()->plane;
	processOutData(die);
    }
}

void Buffer::processInData(uint64_t die){

    // count down the time it takes for the device to latch the data
    if(cyclesLeft[die] > 0)
    {
	cyclesLeft[die]--;
    }

    // device has finished latching this particular piece of data
    if(cyclesLeft[die] == 0)
    {	    
	// do we have data to send
	if(inDataLeft[die] > 0)
	{
	    // set the device latching cycle for this next piece of data
	    cyclesLeft[die] = divide_params(DEVICE_CYCLE,CYCLE_TIME);
	    // subtract this chunk of data from the data we need to send to be done
	    if(inDataLeft[die] >= DEVICE_WIDTH)
	    {
		//cout << "sending data to die \n";
		inDataLeft[die] = inDataLeft[die] - DEVICE_WIDTH;
		if(CUT_THROUGH)
		{
			inDataSize[die] = inDataSize[die] - DEVICE_WIDTH;
		}
	    }
	    // if we only had a tiny amount left to send just set remaining count to zero
	    // to avoid negative numbers here which break things
	    else
	    {
		    if(CUT_THROUGH)
		    {
			    inDataSize[die] = inDataSize[die] - inDataLeft[die];
		    }
		inDataLeft[die] = 0;
	    }
	}
    }
    
    // we're done here
    if(inDataLeft[die] == 0)
    {
	//cout << dies[die]->isDieBusy(inData[die].front()->plane) << "\n";
	//cout << inData[die].front()->type << "\n";
	if(dies[die]->isDieBusy(inData[die].front()->plane) == 0 ||
	   (inData[die].front()->type == 5 && dies[die]->isDieBusy(inData[die].front()->plane) == 2) ||
	   (inData[die].front()->type != 5 && dies[die]->isDieBusy(inData[die].front()->plane) == 3))
	{   
	    channel->bufferDone(id, die, inData[die].front()->plane);
	    if(!CUT_THROUGH)
	    {
		    if(inData[die].front()->type == 5)
		    {
			    if( inDataSize[die] >= (divide_params((NV_PAGE_SIZE*8), CHANNEL_WIDTH)*CHANNEL_WIDTH))
			    {
				    inDataSize[die] = inDataSize[die] - (divide_params((NV_PAGE_SIZE*8), CHANNEL_WIDTH)*CHANNEL_WIDTH);
			    }
			    else
			    {
				    inDataSize[die] = 0;
			    }
		    }
		    else
		    {
			    if(inDataSize[die] >= (divide_params(COMMAND_LENGTH, CHANNEL_WIDTH)*CHANNEL_WIDTH))
			    {
				    inDataSize[die] = inDataSize[die] - (divide_params(COMMAND_LENGTH, CHANNEL_WIDTH)*CHANNEL_WIDTH);
			    }
			    else
			    {
				   inDataSize[die] = 0;  
			    }
		    }
	    }
	    inData[die].pop_front();
	    waiting[die] = false;
	}
	else
	{
	    waiting[die] = true;
	}
    }
}

void Buffer::processOutData(uint64_t die){
    // deal with the critical line first stuff first
    if(critData[die] >= 512 && critData[die] < 512+CHANNEL_WIDTH && channel->notBusy())
    {
	dies[die]->critLineDone();
    }
    
    // got the channel and we have stuff to send so send it
    if(outDataLeft[die] > 0 && channel->notBusy()){
	channel->sendPiece(BUFFER,outData[die].front()->type,die,outData[die].front()->plane);
	
	if(outDataLeft[die] >= CHANNEL_WIDTH)
	{
	    outDataLeft[die] = outDataLeft[die] - CHANNEL_WIDTH;
	    if(CUT_THROUGH)
	    {
		    outDataSize[die] = outDataSize[die] - CHANNEL_WIDTH;
	    }
	}
	else
	{
	    if(CUT_THROUGH)
	    {
		    outDataSize[die] = outDataSize[die] - outDataLeft[die];
	    }
	    outDataLeft[die] = 0;
	}
	critData[die] = critData[die] + CHANNEL_WIDTH;
    }
    
    // we're done here
    if(outDataLeft[die] == 0 && channel->notBusy())
    {
	    if(!CUT_THROUGH)
	    {
		     if( outDataSize[die] >= (divide_params((NV_PAGE_SIZE*8), CHANNEL_WIDTH)*CHANNEL_WIDTH))
		     {
			     outDataSize[die] = outDataSize[die] - (divide_params((NV_PAGE_SIZE*8), CHANNEL_WIDTH)*CHANNEL_WIDTH);
		     }
		     else
		     {
			     outDataSize[die] = 0;
		     }
	    }
	dies[die]->bufferDone(outData[die].front()->plane);
	channel->releaseChannel(BUFFER,id);
	critData[die] = 0;
	outData[die].pop_front();
    }
}

bool Buffer::dataReady(uint64_t die, uint64_t plane)
{					       
    if(!outData[die].empty())
    {
	if(outData[die].front()->type == 0 && outData[die].front()->plane == plane)
	{
	    return true;
	}
	return false;
    }
    return false;
}
