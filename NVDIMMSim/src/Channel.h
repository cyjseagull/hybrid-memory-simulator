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

#ifndef NVCHANNEL_H
#define NVCHANNEL_H
//Channel.h
//header file for the Package class

#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Util.h"

namespace NVDSim{
	enum SenderType{
		CONTROLLER,
		BUFFER
	};

	class Controller;
	class Buffer;
	class Channel{
		public:
			Channel(void);
			void attachBuffer(Buffer *b);
			void attachController(Controller *c);
			int obtainChannel(uint64_t s, SenderType t, ChannelPacket *p);
			int releaseChannel(SenderType t, uint64_t s);
			int hasChannel(SenderType t, uint64_t s);		
			void sendToBuffer(ChannelPacket *busPacket);
			void sendToController(ChannelPacket *busPacket);
			void sendPiece(SenderType t, int type, uint64_t die, uint64_t plane);
			bool isBufferFull(SenderType t, ChannelPacketType bt, uint64_t die);
			int notBusy(void);

			void update(void);

			void bufferDone(uint64_t package, uint64_t die, uint64_t plane);
			
			Controller *controller;
		private:
			SenderType sType;
			int packetType;
			uint64_t sender;
			int busy;
			int firstCheck;
			Buffer *buffer;

			uint64_t currentDie;
			uint64_t currentPlane;
	};
}
#endif
