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

#ifndef NVFRONTBUFFER_H
#define NVFRONTBUFFER_H
//Channel.h
//header file for the Package class

#include "SimObj.h"
#include "FlashConfiguration.h"
#include "FlashTransaction.h"
#include "Ftl.h"
#include "Util.h"

namespace NVDSim{	
	class NVDIMM;
	class FrontBuffer : public SimObj{
		public:
	                FrontBuffer(NVDIMM* parent, Ftl *f);

			bool addTransaction(FlashTransaction transaction);		        		

			// decrements the counters on the transfers and initiates new
			// transfers
			// really just calls the appropriate methods
			void update(void);

			// common code for updating request queue
			FlashTransaction newRequestTrans(void);

			// update routines for each of the possible channels
			void updateRequest(void);
			void updateResponse(void);
			void updateCommand(void);

			// common code for determining how many cycles are needed to move
			// the data protion of a request transaction
			uint64_t setDataCycles(FlashTransaction transaction, uint64_t width);

			// common code for finding if a transaction is present in the pendingData or 
			// pendingCommand vectors
			FlashTransaction findTransaction(std::vector<FlashTransaction> *pendingSearch, std::vector<FlashTransaction> *pendingAdd, FlashTransaction transaction);

			// called after request delay
			bool sendToFTL(FlashTransaction transaction);

			// called after return delay
			void sendToHybrid(const FlashTransaction &transaction);
			
			Ftl *ftl;
			NVDIMM *parentNVDIMM;

		private:
			int sender;
			
			// transaction pointer queues
			std::queue<FlashTransaction>  requests;
			std::queue<FlashTransaction>  responses;
			std::queue<FlashTransaction>  commands;

			// queue size tracking
			uint64_t requestsSize;
			uint64_t responsesSize;

			// transaction currently being serviced
			FlashTransaction requestTrans;
			FlashTransaction responseTrans;
			FlashTransaction commandTrans;

			// transactions that have been partially completed due to split
			// command or data channels
			std::vector<FlashTransaction>  pendingData;
			std::vector<FlashTransaction>  pendingCommand;

			// counters for transfer tracking
			uint64_t requestCyclesLeft;
			uint64_t responseCyclesLeft;
			uint64_t commandCyclesLeft;

			//debug stuff
			int requestStartedCount;
			int responseStartedCount;
			int commandStartedCount;
			
			int requestCompletedCount;
			int responseCompletedCount;
			int commandCompletedCount;
	};
}

#endif
