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

#ifndef NVDIE_H
#define NVDIE_H
//Die.h
//header file for the Die class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "Plane.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{

	class Buffer;
	class Controller;
	class NVDIMM;
	class Ftl;
	class Die : public SimObj{
		public:
	                Die(NVDIMM *parent, Logger *l, uint64_t id);
			void attachToBuffer(Buffer *buff);
			void receiveFromBuffer(ChannelPacket *busPacket);
			int isDieBusy(uint64_t plane);
			void update(void);
			void channelDone(void);
			void bufferDone(uint64_t plane);
			void bufferLoaded(void);
			void critLineDone(void);

			// for fast forwarding
			void writeToPlane(ChannelPacket *packet);

		private:
			uint64_t id;
			NVDIMM *parentNVDIMM;
			Buffer *buffer;
			Logger *log;
			bool sending;
			uint64_t dataCyclesLeft; //cycles per device beat
			uint64_t deviceBeatsLeft; //device beats per page
			uint64_t critBeat; //device beat when first cache line will have been sent, used for crit line first
			std::queue<ChannelPacket *> returnDataPackets;
			std::queue<ChannelPacket *> pendingDataPackets;
			std::vector<Plane> planes;
			std::vector<ChannelPacket *> currentCommands;
			uint64_t *controlCyclesLeft;
	};
}
#endif
