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

#ifndef NVFTL_H
#define NVFTL_H
//Ftl.h
//header file for the ftl

#include <iostream>
#include <fstream>
#include <string>
#include "SimObj.h"
#include "FlashConfiguration.h"
#include "ChannelPacket.h"
#include "FlashTransaction.h"
#include "Controller.h"
#include "Logger.h"
#include "Util.h"

namespace NVDSim{
        class NVDIMM;
	class Ftl : public SimObj{
		public:
	                Ftl(Controller *c, Logger *l, NVDIMM *p);

			ChannelPacket *translate(ChannelPacketType type, uint64_t vAddr, uint64_t pAddr);
			bool attemptAdd(FlashTransaction &t, std::list<FlashTransaction> *queue, uint64_t queue_limit);
			bool addScheduledTransaction(FlashTransaction &t);
			bool addPerfectTransaction(FlashTransaction &t);
			virtual bool addTransaction(FlashTransaction &t);
			void scriptCurrentTransaction(void);
			void scheduleCurrentTransaction(void);
			virtual void update(void);
			void handle_disk_read(bool gc);
			void handle_read(bool gc);
			virtual void write_used_handler(uint64_t vAddr);
			void write_success(uint64_t block, uint64_t page, uint64_t vAddr, uint64_t pAddr, bool gc, bool mapped);
			void handle_scripted_write(void);
			void handle_write(bool gc);
			uint64_t get_ptr(void); 
			void inc_ptr(void); 

			virtual void popFront(ChannelPacketType type);

			void sendQueueLength(void);
			
			void powerCallback(void);

			virtual void saveNVState(void);
			virtual void loadNVState(void);

			void queuesNotFull(void);
			void flushWriteQueues(void);

			virtual void GCReadDone(uint64_t vAddr);
		       
			Controller *controller;

			NVDIMM *parent;

			Logger *log;

			// temp stuff **************************
			uint64_t locked_counter;
			// *************************************

		protected:
			std::ifstream scriptfile;
			uint64_t write_cycle;
			uint64_t write_addr;
			uint64_t write_pack;
			uint64_t write_die;
			uint64_t write_plane;
			FlashTransaction writeTransaction;

			bool gc_flag;
			uint64_t channel, die, plane, lookupCounter;
			uint64_t temp_channel, temp_die, temp_plane;
			uint64_t max_queue_length;
			FlashTransaction currentTransaction;
			bool busy;

			uint64_t deadlock_counter;
			uint64_t deadlock_time;
			uint64_t write_counter;
			uint64_t used_page_count;
			uint64_t write_wait_count;
			std::list<FlashTransaction>::iterator read_pointer; // stores location of the last place we tried in the read queue

			bool saved;
			bool loaded;
			bool read_queues_full;
			bool write_queues_full;
			bool flushing_write;

			uint64_t queue_access_counter; // time it takes to get the data out of the write queue
			uint64_t read_iterator_counter; // double check for the end() function
			std::list<FlashTransaction>::iterator reading_write;

			std::unordered_map<uint64_t,uint64_t> addressMap;
			std::vector<vector<bool>> used;
			std::list<FlashTransaction> readQueue; 
			std::list<FlashTransaction> writeQueue;
	};
}
#endif
