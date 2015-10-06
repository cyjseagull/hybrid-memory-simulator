#ifdef NVMAIN
#include <memoryRequest.h>
#include <cacheController.h>
#include <memoryControllerBuilder.h>

#include <NVMainMemory.h>
#include "NVM/NVMainFactory.h"
#include "SimInterface/NullInterface/NullInterface.h"
#include "Utils/HookFactory.h"
#include <vector>
#include <iterator>
using namespace Memory;

NVMainMemory::NVMainMemory( W8 coreid , const char* name , 
			 MemoryHierarchy* mem_hierachy )
			:Controller(coreid , name , mem_hierachy),
			 nvmain_ptr_(NULL) , nvmain_config_(NULL)
{
	std::cout<<"##########nvmain defined"<<std::endl;
	nvmain_config_file_ = config.nvmain_config_path.buf;
	//nvmain_config_file_ = "/home/liao/chenyujie/hybrid-main/marss/nvmain/Config/2D_DRAM.config";
	std::cout<<"nvmain_config_file is "<<nvmain_config_file_<<std::endl;

	mem_hierachy->add_cache_mem_controller(this);
	//set signal process request completed from nvmain	
	/*SET_SIGNAL_CB( name , "_Access_Completed" , 
				   request_completed_ , 
				   &NVMainMemory::RequestComplete);	*/
				//request complete call back function

	SET_SIGNAL_CB( name , "_Wait_Interconnect", wait_interconnect_,
				   &NVMainMemory::wait_interconnect_cb);
	//wait interconnect call back function

	//nvmain related
	event_driven_ = false;
	next_event_cycle_ = 0;
	//nacked_req = false;

	nvmain_config_ = new NVM::Config();
	nvmain_config_->Read( nvmain_config_file_);	//read nvmain config

	init_nvmain_object();
	data_size_ = CacheController::get_cache_line_size();
	memdebug("data size is "<< data_size_);
	cur_cycle_=0;
	last_cycle_=0;
	total_read_times = 0;
	total_write_times = 0;
	relative_dis_ = 3;
}

/*init nvmain object*/
void NVMainMemory::init_nvmain_object( )
{
	//nvmain config object must be inited first
	assert( nvmain_config_ );
	std::string mem_type_str = "NVMain";
	if( nvmain_config_->KeyExists("CMemType") )
			mem_type_str = nvmain_config_->GetString("CMemType");
	std::cout<<"MARSSX86:create nvmain"<<std::endl;
	nvmain_ptr_ = NVM::NVMainFactory::CreateNewNVMain( mem_type_str);
	nvmain_stat_ptr_ = new NVM::Stats();
	nvmain_sim_ = new NVM::NullInterface();
	nvmain_eventq_ = new NVM::EventQueue(); //event queue
	nvmain_geventq_ = new NVM::GlobalEventQueue(); //global event queue
	nvmain_tag_generator_ = new NVM::TagGenerator(1000);
	

	nvmain_config_->SetSimInterface( nvmain_sim_ );
	if( nvmain_config_->KeyExists("StatsFile"))
	{
		nvmain_stat_filename = config.log_filename;
		int pos = nvmain_stat_filename.find_last_of('/');
		if( pos != -1 )
		{
			nvmain_stat_filename = nvmain_stat_filename.substr(0,pos+1);
			std::cout<<"nvmain config file path "<<nvmain_stat_filename<<std::endl;
		}
		//nvmain_stat_filename = "/home/liao/chenyujie/hybrid-main/marss/results/logs/";
		if( nvmain_config_->KeyExists("StatsFile"))
			nvmain_stat_filename += nvmain_config_->GetString("StatsFile");
		std::cout<<"nvmain stat filename is "<<nvmain_stat_filename<<std::endl;
		std::ofstream out( nvmain_stat_filename.c_str() , std::ios_base::out);
		out<<"###nvmain stats"<<std::endl;
		out<<"######==============="<<std::endl;
	}

	SetEventQueue( nvmain_eventq_ );
	SetStats( nvmain_stat_ptr_ );
	SetTagGenerator( nvmain_tag_generator_ );
	
	nvmain_geventq_->SetFrequency( nvmain_config_->GetEnergy("CPUFreq")*1000000.0 );
	SetGlobalEventQueue(nvmain_geventq_);
	
	//hooks
	std::vector<std::string>& hook_list = nvmain_config_->GetHooks();
	for( uint64_t i=0; i < hook_list.size() ; i++)
	{
		memdebug("creating hook "+hook_list[i]);
		NVMObject* hook = NVM::HookFactory::CreateHook( hook_list[i] );
		if( hook )
		{
			AddHook( hook);
			hook->SetParent( this );
			hook->Init( nvmain_config_ );
		}
		else
			std::cout<<"has no hook named "<<hook_list[i]<<std::endl;
	}
	AddChild( nvmain_ptr_ );
	nvmain_ptr_->SetParent( this );
	nvmain_ptr_->SetConfig( nvmain_config_ );
	nvmain_geventq_->AddSystem( nvmain_ptr_ ,nvmain_config_ );
		
	 //get nvmain freq(default is 666MHZ)(unit : MHZ)
	  if ( nvmain_config_->KeyExists("CLK") )
	  {
		  uint64_t nvm_clock = nvmain_config_->GetValue("CLK");
		 //get cpu freq (HZ)
		  uint64_t cpu_freq = get_native_core_freq_hz();
		  relative_dis_ = cpu_freq/(1000000*nvm_clock);
	  }


}

void NVMainMemory::print_stats()
{
	std::cout<<"NVMainMemory:begin print stats , nvmain stat file name is:"<<nvmain_stat_filename<<std::endl;
	std::ofstream out( nvmain_stat_filename.c_str() , std::ios_base::app);
	nvmain_ptr_->CalculateStats();
	nvmain_ptr_->GetStats()->PrintAll(out);
	out<<"*** MARSS: total read time : "<<total_read_times<<std::endl;
	out<<"*** MARSS: total write time: "<<total_write_times<<std::endl;
	total_read_times = 0;
	total_write_times = 0;
	out<<"==========="<<std::endl;
}
bool NVMainMemory::handle_request_cb( void* arg )
{
	return true;	
}

/*response to cache controller*/
bool NVMainMemory::wait_interconnect_cb(void* arg)
{
	//std::cout<<"*******response to cache controller******"<<std::endl;
	MemoryQueueEntry* entry = static_cast<MemoryQueueEntry*>(arg);
	assert(entry);
	//no response to memory update request, free request
	if( entry->request->get_type() == MEMORY_OP_UPDATE)
	{
	//	std::cout<<"drop update request"<<std::endl;
		entry->request->decRefCounter();
		ADD_HISTORY_REM(entry->request);
		pendingRequests_.free(entry);
		return true;
	}
	//std::cout<<"allocate message space"<<std::endl;
	//allocate message buffer , init it 
	Message &message = *memoryHierarchy_->get_message();
	message.sender = this;
	message.dest = entry->source;
	message.request = entry->request;
	message.hasData = true;
	//std::cout<<"get controller request signal begin"<<std::endl;	
	memdebug("[NVMain Memory]: sending message to cache");
	//call cache signal function
	bool success = cache_interconnect_->get_controller_request_signal()
		->emit(&message);
	//std::cout<<"free message"<<std::endl;
	//free message
	memoryHierarchy_->free_message( &message);
	//send response failed
	if( !success )
	{
		//std::cout<<"get controller request signal failed"<<std::endl;
		//retry after 1 cycle
		memoryHierarchy_->add_event( &wait_interconnect_ , 1 , entry);
	}
	else
	{
		//std::cout<<"get controller request signal succeed"<<std::endl;
		entry->request->decRefCounter();
		ADD_HISTORY_REM(entry->request);
		pendingRequests_.free(entry);	//free entry
		if(!pendingRequests_.isFull() )
			memoryHierarchy_->set_controller_full(this ,false);
	}
	//std::cout<<"****response to cache controller succeed****"<<std::endl;
	return true;
}


/**/
void NVMainMemory::ParseRequest(NVM::NVMainRequest* &req ,  Message* message )
{
	req->access = NVM::UNKNOWN_ACCESS;
	req->status = NVM::MEM_REQUEST_INCOMPLETE;
	//address align
	uint64_t pa = ALIGN_ADDRESS( message->request->get_physical_address() , nvmain_transaction_size);
	req->address.SetPhysicalAddress( pa );

	req->threadId = message->request->get_threadid();
	req->owner = (NVMObject *)this;	//set owner 
	//set physical address
	//read nvmain
	if( message->request->get_type() == MEMORY_OP_READ)
	{
		req->type = NVM::READ;
		total_read_times++;
	}
	//write nvmain
	else if( message->request->get_type()== MEMORY_OP_WRITE
			|| message->request->get_type()== MEMORY_OP_UPDATE
			|| message->request->get_type() ==MEMORY_OP_EVICT )
	
	{
		req->type = NVM::WRITE;
		total_write_times++;
	}

	//set request data with all 0,size is cache line size
	bool ignore_data = false;
	if( nvmain_config_->KeyExists("IgnoreData") )
	{
		ignore_data = true;
	}
	//set all zero
	if( ignore_data && message->hasData )
	{
		req->data.SetSize(data_size_);
		for( int i=0 ; i<data_size_ ; i++ )
		{
			req->data.SetByte(i,0);
		}
	}
}

//advance memory system cycle to input cycle
//return new cycle
void NVMainMemory::clock(  )
{
	//std::cout<<"begin clock nvmain"<<std::endl;
	nvmain_ptr_->Cycle(1);
	//std::cout<<"1 cycle clock end"<<std::endl;
	/*
	last_cycle_ = cycle + 1;
	cur_cycle_ = last_cycle_;
	//not seviced yet, cycle by cycle  until served
	if( next_sched_req_)
		return cycle+1;
	else 
	{
		if( inflight_req_.empty() )
		{
			next_sched_req_ = NULL;
			return 0;
		}
		else
		{
			next_sched_req_ = inflight_req_.front().first;
			//get cycle
			if( cycle >= inflight_req_.front().second.second ) 
				return cycle+1;
			else
				return inflight_req_.front().second.second;
		}
	}*/
	//return 0;
}

/**/
bool NVMainMemory::handle_interconnect_cb(void* arg)
{
	Message *message = (Message*)arg;
	//uint64_t pa = ALIGN_ADDRESS( message->request->get_physical_address() , nvmain_transaction_size);
	//std::cout<<"0x"<<std::hex<<pa<<" miss , type is : "<<message->request->get_type()<<std::endl;
	//ignore request
	if( message->hasData && message->request->get_type() != MEMORY_OP_UPDATE)
		return true;
	//ignore all the evict messages
	if( message->request->get_type()== MEMORY_OP_EVICT)
		return true;
	//if this request is a memory update request,
	//check the pending queue and see if we have a memory update request
	//to the same line;
	//if we can merge these requests, then merge them into one request
	if( message->request->get_type() == MEMORY_OP_UPDATE)
	{
		MemoryQueueEntry *entry;
		foreach_list_mutable_backwards( pendingRequests_.list(),entry , 
										entry_t , nextentry_t)
		{
			if( entry->request->get_physical_address()== 
					message->request->get_physical_address())
			{
				//merge request
				if(	!entry->inUse && entry->request->get_type() == MEMORY_OP_UPDATE )
					return true;
				break;
			}
		}
	}
	/*add request to pending request queue*/
	//allocate memory queue from pendingRequest(request pool)
	MemoryQueueEntry *queue_entry = pendingRequests_.alloc();
	if( queue_entry == NULL)
	{
		memdebug("allocate entry for request failed! pendingRequest queue is full");
		return false;
	}
	//pending request queue is full
	if( pendingRequests_.isFull() )
	{
		memoryHierarchy_->set_controller_full(this,true); //set memory controller full	
	}
	queue_entry->request = message->request;
	queue_entry->source = (Controller*)message->origin;	//source controller
	queue_entry->request->incRefCounter();
	ADD_HISTORY_ADD( queue_entry->request);
	assert(queue_entry->inUse == false);
	
	//get prehooks
	std::vector<NVM::NVMObject *> prehooks = this->GetHooks(NVM::NVMHOOK_PREISSUE);
	std::vector<NVM::NVMObject *> posthooks = this->GetHooks( NVM::NVMHOOK_POSTISSUE);
	std::vector<NVM::NVMObject *>::iterator it;

	NVM::NVMainRequest* req = new NVM::NVMainRequest();
	ParseRequest( req , message);	//parse request to nvmain request

	/*cur_cycle_ = sim_cycle+1;
	std::cout<<"current cycle of marss is:"<<cur_cycle_<<std::endl;
	assert( cur_cycle_ > last_cycle_ );
	nvmain_geventq_->Cycle( cur_cycle_ - last_cycle_);
	last_cycle_ = cur_cycle_;

	uint64_t next_cycle = nvmain_geventq_->GetNextEvent(NULL);
	if( next_cycle != std::numeric_limits<uint64_t>::max())
	{
		uint64_t currentCycle = nvmain_geventq_->GetCurrentCycle();
		assert( next_cycle > currentCycle );
		uint64_t step_cycle = next_cycle - currentCycle;
		next_event_cycle = 
	}*/
	//std::cout<<"cycle of nvmain:"<<GetEventQueue()->GetCurrentCycle()<<std::endl;
	bool can_queue = nvmain_ptr_->IsIssuable( req , NULL );
	if( can_queue )
	{
		//--call prehooks : issue command to prehooks first---- 
		for( it = prehooks.begin() ; it!=prehooks.end(); it++)
		{
			(*it)->SetParent(this);
			(*it)->IssueCommand( req );
		}
		
		//--issue command--
		/*
		std::cout<<"######################"<<std::endl;
		std::cout<<"$ issue command 0x"<<req->address.GetPhysicalAddress()<<" , current cycle is"<<GetEventQueue()->GetCurrentCycle()<<std::endl;
		std::cout<<"#############"<<std::endl;*/
		bool enqueued = this->GetChild()->IssueCommand( req );
		assert(enqueued);
		
		//--create new packet of marss memory--
		NVMainMemoryRequest* nvm_req = new NVMainMemoryRequest;
		nvm_req->req = req;
		nvm_req->msg = *message;
		nvm_req->atomic = false;	//timing mode
		nvm_req->issue_cycle = sim_cycle; 
		
		m_request_map_.insert( std::pair<NVM::NVMainRequest* , NVMainMemoryRequest*>(req,nvm_req) );
		//--call posthooks--
		if( req )
		{
			for( it = posthooks.begin(); it != posthooks.end();it++)
			{
				(*it)->SetParent(this);
				(*it)->IssueCommand(req);
			}
		}
	
	}
	else	//can not issue memory accessing request
	{
		delete req;
		req = NULL;
	}
	/*if( can_queue )
	{
		//GetEventQueue()->InsertEvent( NVM::EventResponse , nvmain_ptr_ , req , 
		//		 GetEventQueue()->GetCurrentCycle()+1 );

		//--call prehooks : issue command to prehooks first---- 
		for( it = prehooks.begin() ; it!=prehooks.end(); it++)
		{
			(*it)->SetParent(this);
			(*it)->IssueCommand( req );
		}
		//--issue command--
		bool accepted = nvmain_ptr_->IssueCommand( req );
		queue_entry->inUse = true;
		assert(accepted);
		//--create new packet of marss memory--
		NVMainMemoryRequest* nvm_req = new NVMainMemoryRequest;
		nvm_req->req = req;
		nvm_req->msg = *message;
		nvm_req->atomic = false;	//timing mode
		nvm_req->issue_cycle = sim_cycle; 
		uint64_t next_event_cycle = nvmain_geventq_->GetNextEvent(NULL);
		if( event_driven_ && next_event_cycle < next_event_cycle_)
		{
			uint64_t current_cycle = nvmain_geventq->GetCurrentCycle();
			uint64_t step_cycles;
			if( next_event_cycle > current_cycle)
				step_cycles = next_event_cycle - current_cycle;
			else
				step_cycles = 1;
			next_event_cycle_ = next_event_cycle;
		}
		//--call posthooks--
	}*/
	return true;
}
	
int NVMainMemory::access_fast_path( Interconnect* interconnect , 
								  MemoryRequest *mem_req)
{
	return true;
}


/*register cache interconnect object*/
void NVMainMemory::register_interconnect( Interconnect* inter , int con_type )
{
	switch( con_type )
	{
		case INTERCONN_TYPE_UPPER:
				//std::cout<<"register cache_interconnect_"<<std::endl;
				cache_interconnect_ = inter;
				break;
		default:
				assert(0); //exit because of assert error
				break;
	}
}


void NVMainMemory::print_map( ostream & os)
{

}

void NVMainMemory::print( ostream & os) const
{

}
									                
/*request completed from nvmain*/
bool NVMainMemory::RequestComplete(NVM::NVMainRequest *req )
{
	/*std::cout<<std::endl;
	std::cout<<"#######request complete from nvmain:#####"<<std::endl;
	std::cout<<"$ address is 0x"<<std::hex<<req->address.GetPhysicalAddress()<<", current cycle is "<<GetEventQueue()->GetCurrentCycle()<<std::endl;
	std::cout<<"##########################"<<std::endl;
	std::cout<<std::endl;*/
	//find mem request pointer in the map
	std::map< NVM::NVMainRequest * , NVMainMemoryRequest *>::iterator it;
	assert( m_request_map_.count(req));
	it = m_request_map_.find(req);
	NVMainMemoryRequest* mem_req = it->second;

	MemoryQueueEntry *entry = NULL;
	uint64_t pa = req->address.GetPhysicalAddress();
	//find request in pendingRequest queue 
	foreach_list_mutable( pendingRequests_.list(), entry, entry_t , prev_t)
	{
		if( ALIGN_ADDRESS( entry->request->get_physical_address() , 
						nvmain_transaction_size )== pa )
		{
			//std::cout<<"find entry"<<std::endl;
			memdebug("[NVMain]: request complete from address 0x"<<std::hex<<pa<<std::endl);
			break;
		}
	}
	assert(entry);
	//bool kernel = entry->request->is_kernel();
	//entry shouldn't be annuled , call wait_interconnect_cb,
	//response to cache controller
	if(!entry->annuled)
	{
		memdebug("[NVMain]: access 0x"<<std::hex<<pa<<" completed , response to cache" );
		//std::cout<<"issue response to cache,wait for interconnect cb"<<std::endl;
		wait_interconnect_cb(entry);
	}
	else	//no need response to cache , free request
	{
		entry->request->decRefCounter();
		ADD_HISTORY_REM(entry->request);
		pendingRequests_.free(entry);
	}
	m_request_map_.erase(it);
	delete mem_req;
	delete req;
	return true;
}

/*annual request*/
void NVMainMemory::annul_request(MemoryRequest *req)
{
	//std::cout<<"annul request"<<std::endl;
	MemoryQueueEntry* entry;
	foreach_list_mutable( pendingRequests_.list() , entry , entry_t , nextentry)
	{
		if( entry->request->is_same(req))
		{	//annual request
			entry->annuled = true;
			if( !entry->inUse)
			{
				entry->request->decRefCounter();
				ADD_HISTORY_REM( entry->request);
				pendingRequests_.free(entry);
			}
		}
	}
}

void NVMainMemory::dump_configuration(YAML::Emitter &out) const
{
	out<< YAML::Key <<get_name()<<YAML::Value<<YAML::BeginMap;
	YAML_KEY_VAL( out , "type" , "nvm_cont" );
	YAML_KEY_VAL( out , "pending request size" , pendingRequests_.size());
	YAML_KEY_VAL( out , "nvmain config file" , nvmain_config_file_)
	out<< YAML::EndMap;
}

MemoryControllerBuilder nvmainControllerBuilder("simple_nvm_cont");
#endif
