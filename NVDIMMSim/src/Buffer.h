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

#ifndef NVBUFFER_H
#define NVBUFFER_H
// Buffer.h
// Header file for the buffer class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Die.h"
#include "Channel.h"

namespace NVDSim{
    class Buffer : public SimObj{
        public:
	    Buffer(uint64_t i);
	    void attachDie(Die *d);
	    void attachChannel(Channel *c);
	    void sendToDie(ChannelPacket *busPacket);
	    void sendToController(ChannelPacket *busPacket);

	    bool sendPiece(SenderType t, int type, uint64_t die, uint64_t plane);
	    bool isFull(SenderType t, ChannelPacketType bt, uint64_t die);
	    
	    void update(void);

	    void prepareOutChannel(uint64_t die);

	    void processInData(uint64_t die);
	    void processOutData(uint64_t die);

	    bool dataReady(uint64_t die, uint64_t plane); // die asking to send data back

	    Channel *channel;
	    std::vector<Die *> dies;

	    uint64_t id;

        private:
	    class BufferPacket{
	        public:
		// type of packet
		int type;
		// how many bits are outstanding for this page
		uint64_t number;
		// plane that this page is for
		uint64_t plane;

		BufferPacket(){
		    type = 0;
		    number = 0;
		    plane = 0;
		}
	    };
	    
	    uint64_t* cyclesLeft;	    
	    uint64_t* outDataLeft;
	    uint64_t* critData; // burst on which the critical line will be done
	    uint64_t* inDataLeft;
	    bool* waiting;
	    bool* sending;
	    uint64_t lookupTimeLeft;
	    uint64_t dieLookingUp;

	    uint64_t sendingDie;
	    uint64_t sendingPlane;

	    uint64_t* outDataSize;
	    std::vector<std::list<BufferPacket *> >  outData;
	    uint64_t* inDataSize;
	    std::vector<std::list<BufferPacket *> > inData;
    };
} 

#endif
