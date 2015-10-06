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

#ifndef NVCONTROLLER_H
#define NVCONTROLLER_H
//Controller.h
//header file for controller

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "Die.h"
#include "Buffer.h"
#include "Ftl.h"
#include "Channel.h"
#include "FlashTransaction.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{
	typedef struct {
		Channel *channel;
	        Buffer *buffer;
		std::vector<Die *> dies;
	} Package;

	class FrontBuffer;
	class NVDIMM;
	class Controller : public SimObj{
		public:
	                Controller(NVDIMM* parent, Logger* l);

			void attachPackages(vector<Package> *packages);
			void attachFrontBuffer(FrontBuffer *fb);
			void returnReadData(const FlashTransaction &trans);
			void returnUnmappedData(const FlashTransaction &trans);
			void returnCritLine(ChannelPacket *busPacket);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> erase_energy,
					 vector<double> vpp_idle_energy,
					 vector<double> vpp_access_energy,
					 vector<double> vpp_erase_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> vpp_idle_energy,
					 vector<double> vpp_access_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy,
					 vector<double> erase_energy);
			void returnPowerData(vector<double> idle_energy,
					 vector<double> access_energy);
			void attachChannel(Channel *channel);
			void receiveFromChannel(ChannelPacket *busPacket);
			bool checkQueueWrite(ChannelPacket *p);
			bool addPacket(ChannelPacket *p);
			bool nextDie(uint64_t package);
			void update(void);
			bool dataReady(uint64_t package, uint64_t die, uint64_t plane);

			void sendQueueLength(void);

			void bufferDone(uint64_t package, uint64_t die, uint64_t plane);

			// for fast forwarding
			void writeToPackage(ChannelPacket *packet);

			NVDIMM *parentNVDIMM;
			Logger *log;
			FrontBuffer *front_buffer;

		private:
			bool* paused;
			uint64_t* die_pointers; // for maintaining round robin fairness for channel access
			uint64_t die_counter;
			bool done;

			uint64_t queue_access_counter;

			std::list<FlashTransaction> returnTransaction;
			std::vector<Package> *packages;
			std::vector<std::vector<std::list <ChannelPacket *> > > readQueues;
			std::vector<std::vector<std::list <ChannelPacket *> > > writeQueues;
			std::vector<ChannelPacket *> outgoingPackets; //there can only ever be one outgoing packet per channel
			std::vector<std::list <ChannelPacket *> > pendingPackets; //there can be a pending package for each plane of each die of each package
			std::vector<uint64_t> channelXferCyclesLeft; //cycles per channel beat
			std::vector<uint64_t> channelBeatsLeft; //channel beats per page

	};
}
#endif
