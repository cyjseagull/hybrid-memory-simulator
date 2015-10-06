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

//Channel.cpp
//class file for channel class
#include "Channel.h"
#include "ChannelPacket.h"
#include "Controller.h"
#include "Buffer.h"

using namespace NVDSim;

Channel::Channel(void){
	sender = -1;
	busy = 0;

	firstCheck = 0;
}

void Channel::attachBuffer(Buffer *b){
	buffer = b;
}

void Channel::attachController(Controller *c){
	controller = c;
}

int Channel::obtainChannel(uint64_t s, SenderType t, ChannelPacket *p){
    if( p ==  NULL && t == CONTROLLER)
    {
	cout << "something weird happened \n";
    }
    if ((sender != ULLONG_MAX) ||
	(t == CONTROLLER && !BUFFERED && (buffer->dies[p->die]->isDieBusy(p->plane) == 1)) ||
	// should allow us to send write data to a buffer that is currently writing
	(t == CONTROLLER && !BUFFERED && p->busPacketType != DATA && buffer->dies[p->die]->isDieBusy(p->plane) == 2) ||
	// should allow us to send a write command to a plane that has a loaded cache register
	(t == CONTROLLER && !BUFFERED && p->busPacketType ==DATA && buffer->dies[p->die]->isDieBusy(p->plane) == 3) ||
	(busy == 1))
    {
	return 0;		
    }
    else
    {
	sType = t;
	sender = s;
	return 1;
    }
    return 0;
}

int Channel::releaseChannel(SenderType t, uint64_t s){       
	if (t == sType && sender == s){
		sender = -1;
		return 1;
	}
	return 0;
}

int Channel::hasChannel(SenderType t, uint64_t s){
	if (t == sType && sender == s)
		return 1;
	return 0;
}

void Channel::sendToBuffer(ChannelPacket *busPacket){
        buffer->sendToDie(busPacket);
}

void Channel::sendToController(ChannelPacket *busPacket){
        controller->receiveFromChannel(busPacket);
}

void Channel::sendPiece(SenderType t, int type, uint64_t die, uint64_t plane){
	busy = 1;
	currentDie = die;
	currentPlane = plane;
	packetType = type;
}

bool Channel::isBufferFull(SenderType t, ChannelPacketType bt, uint64_t die)
{
    return buffer->isFull(t, bt, die);
}

int Channel::notBusy(void){
        if(busy == 1){
	    return 0;
	}else{
	    return 1;
	}
}

void Channel::update(void){
        if(busy == 1){
	    if(sType == CONTROLLER){
		bool success = buffer->sendPiece(CONTROLLER, packetType, currentDie, currentPlane);
		if(success == false)
		{
		    ERROR("Tried to push data into a full buffer");
		    abort();
		}
	    }
	    busy = 0;
	}
}

void Channel::bufferDone(uint64_t package, uint64_t die, uint64_t plane){
    controller->bufferDone(package, die, plane);
}
