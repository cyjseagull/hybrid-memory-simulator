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

#ifndef NVGCFTL_H
#define NVGCFTL_H
//GCFtl.h
//header file for the ftl with garbage collection

#include <iostream>
#include <fstream>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Ftl.h"
#include "Logger.h"
#include "GCLogger.h"

namespace NVDSim{
        class NVDIMM;
	class GCFtl : public Ftl{
		public:
	                GCFtl(Controller *c, Logger *l, NVDIMM *p);
			bool addTransaction(FlashTransaction &t);
			void addGcTransaction(FlashTransaction &t);
			void update(void);
			void write_used_handler(uint64_t vAddr);
			bool checkGC(void); 
			void runGC(void);
			void runGC(uint64_t plane);
			void addGC(uint64_t dirty_block);

			void popFront(ChannelPacketType type);

			void sendQueueLength(void);

			void saveNVState(void);
			void loadNVState(void);

			void GCReadDone(uint64_t vAddr);

		protected:
			bool gc_status, panic_mode;
			uint64_t start_erase;

			uint64_t erase_pointer;			
			
			class PendingErase
			{
			public:
			    std::list<uint64_t> pending_reads;
			    uint64_t erase_block;
			    
			    PendingErase()
			    {
				erase_block = 0;
			    }
			};
			std::list<PendingErase> gc_pending_erase;  

			uint64_t dirty_page_count;

			std::vector<vector<bool>> dirty;
			std::list<FlashTransaction> gcQueue;
	};
}
#endif
