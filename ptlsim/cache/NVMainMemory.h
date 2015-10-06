#ifndef _NVMAIN_MEMORY_H_
#define _NVMAIN_MEMORY_H_
#ifdef NVMAIN
#include <iostream>
#include <fstream>
#include <controller.h>
#include <memoryHierarchy.h>
#include <interconnect.h>
#include <memoryRequest.h>
#include <memoryStats.h>
#include <superstl.h>
#include <statelist.h>
#include <memoryController.h>
//nvmain main memory
#include "NVM/NVMainFactory.h"
#include "src/Config.h"
#include "src/SimInterface.h"
#include "src/EventQueue.h"
#include "src/Stats.h"
#include "include/NVMainRequest.h"
#include "src/TagGenerator.h"
#include "src/NVMObject.h"
static const int nvmain_transaction_size = 64;

namespace Memory
{
	class NVMainAccEvent;	//forward declaration
	struct NVMainMemoryRequest;
	class NVMainMemory: public Controller , public NVM::NVMObject 
	{
		public:
			NVMainMemory( W8 coreid , const char* name , 
						  MemoryHierarchy* mem_hierachy);
			bool handle_request_cb( void* arg );
			bool handle_interconnect_cb(void* arg);
			bool wait_interconnect_cb(void* arg); //response to cache interconnect
			int access_fast_path( Interconnect* interconnect , 
								  MemoryRequest *mem_req);

			void register_interconnect( Interconnect* inter , int con_type=0);
			void print_map( ostream & os);
			void print( ostream & os) const;
			
			bool RequestComplete(NVM::NVMainRequest *req);
			void annul_request(MemoryRequest *req);
			void dump_configuration(YAML::Emitter &out) const;
			bool is_full(bool fromInterconnect = false , MemoryRequest* req=NULL) const
			{
				return pendingRequests_.isFull();
			}
			void clock( );
			std::string nvmain_stat_filename;
			void print_stats();
			#define ALIGN_ADDRESS(addr , bytes) ( addr & ~(((unsigned long)bytes)-1L))
		public:
			int relative_dis_;
		private:
			void init_nvmain_object();
			void ParseRequest(NVM::NVMainRequest* &req ,  Message* message );
			//cycle memory controller
			//uint64_t tick( uint64_t cycle);
			//void clock( );
			//enque
			//void enqueue( NVMainAccEvent* acc_event , uint64_t cycle);
		private:
			//cycle related
			uint64_t last_cycle_;
			uint64_t cur_cycle_;
			Interconnect *cache_interconnect_;
			Signal request_completed_; //signal receive requestcompleted request from NVMain
			Signal wait_interconnect_; //response to cache interconnect


			NVM::NVMain* nvmain_ptr_;	//point to nvmain memory
			NVM::Config *nvmain_config_;
			NVM::SimInterface *nvmain_sim_;
			NVM::EventQueue *nvmain_eventq_;
			NVM::Stats *nvmain_stat_ptr_;
			NVM::GlobalEventQueue *nvmain_geventq_;
			NVM::TagGenerator *nvmain_tag_generator_;
			//request queue: entry num is at most MEM_REQ_NUM
			FixStateList<MemoryQueueEntry , MEM_REQ_NUM> pendingRequests_;
			
			std::map<NVM::NVMainRequest * , NVMainMemoryRequest *> m_request_map_;

			bool event_driven_;
			//next event
			NVM::NVMainRequest* next_sched_req_;
			NVM::ncycle_t next_event_cycle_;
			//inflight request
			//for calculating memory capacity
			uint64_t bus_width_;
			uint64_t tburst_;
			uint64_t rate_;
			//statistic data
			uint64_t total_read_times;
			uint64_t total_write_times;
			//for nvmain request data
			int data_size_;
			//nvmain config file path
			std::string nvmain_config_file_;
	};

	struct NVMainMemoryRequest
	{
		NVM::NVMainRequest *req;
		Message msg;
		W64 issue_cycle;
		bool atomic;
	};

	class NVMainAccEvent
	{
		public:
			NVMainAccEvent( NVMainMemory* nvm , bool write , 
				uint64_t addr):nvm_(nvm) , write_(write) , addr_(addr)
		{}

			uint64_t get_address()
			{
				return addr_;
			}

			bool is_write()
			{
				return write_;
			}

			uint64_t get_addr()
			{
				return addr_;
			}
			//simulate
			void simulate( uint64_t startCycle)
			{
				start_cycle_ = startCycle;
				//nvm_->enqueue( this , start_cycle_);
			}
			uint64_t start_cycle_;
		private:
			NVMainMemory* nvm_;
			bool write_;
			uint64_t addr_;
	};

};
#endif
#endif
