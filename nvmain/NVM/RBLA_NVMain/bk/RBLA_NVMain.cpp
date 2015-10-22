/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*******************************************************************************/

#include "RBLA_NVMain.h"
#include "src/Config.h"
#include "src/AddressTranslator.h"
#include "src/Interconnect.h"
#include "src/SimInterface.h"
#include "src/EventQueue.h"
#include "Interconnect/InterconnectFactory.h"
#include "MemControl/MemoryControllerFactory.h"
#include "traceWriter/TraceWriterFactory.h"
#include "Decoders/DecoderFactory.h"
#include "Utils/HookFactory.h"
#include "include/NVMainRequest.h"
#include "include/NVMHelpers.h"
#include "Prefetchers/PrefetcherFactory.h"
#include "include/Exception.h"

#include <sstream>
#include <cassert>

using namespace NVM;


uint64_t NVMain::migra_thres = 20 ;
Action NVMain::pre_action = INCREMENT;
double NVMain::pre_net_benefit = 0.0;

RBLA_NVMain::RBLA_NVMain( )
{
	stats_table_entry = 16;
	write_incre = 2;
	read_incre = 1;
	col_incre_inited = false;
	update_interval = 1000000;
}

RBLA_NVMain::~RBLA_NVMain( )
{    
}

void RBLA_NVMain::SetConfig( Config *conf, std::string memoryName, bool createChildren )
{
	NVMain::SetConfig( conf , meoryName , createChildren );
    cols = static_cast<int>(p->COLS);
	//get tBurst,RATE,BusWidth
	uint64_t tburst , rate , buswidth ;
	tburst = static_cast<uint64_t>(p->tBURST);
	rate = static_cast<uint64_t>(p->RATE);
	buswidth = static_cast<uint64_t>(p->BusWidth);
	memory_word_size = tburst*rate*buswidth/8;	
	std::cout<<"memory word is:"<<memory_word_size<<std::endl;
	
	if( config->KeyExists("StatsTableEntry"))
		stats_table_entry = static_cast<uint64_t>( config->GetValue("StatsTableEntry") );
	if(config->KeyExists("WriteIncrement"))
		write_incre = static_cast<uint64_t>(config->GetValue("WriteIncrement"));
	if(config->KeyExists("ReadIncrement"))
		read_incre = static_cast<uint64_t>(config->GetValue("ReadIncrement"));
	if(config->KeyExists("UpdateInterval"))
		update_interval = static_cast<uint64_t>(config->GetValue("UpdateInterval"));
	//create stats table
	statsTable = std::auto_ptr<StatsStore>( new StatsStore(stats_table_entry));
}



bool RBLA_NVMain::IsIssuable( NVMainRequest *request, FailReason *reason )
{
    uint64_t channel, rank, bank, row, col, subarray;
    bool rv;

    assert( request != NULL );

    GetDecoder( )->Translate( request->address.GetPhysicalAddress( ), 
                           &row, &col, &rank, &bank, &channel, &subarray );

    rv = memoryControllers[channel]->IsIssuable( request, reason );
//	std::cout<<"Issuable:"<<(rv?"true":"false")<<std::endl;
    return rv;
}

void NVMain::GeneratePrefetches( NVMainRequest *request, std::vector<NVMAddress>& prefetchList )
{
    std::vector<NVMAddress>::iterator iter;
    ncounter_t channel, rank, bank, row, col, subarray;

    for( iter = prefetchList.begin(); iter != prefetchList.end(); iter++ )
    {
        /* Make a request from the prefetch address. */
        NVMainRequest *pfRequest = new NVMainRequest( );
        *pfRequest = *request;
        pfRequest->address = (*iter);
        pfRequest->isPrefetch = true;
        pfRequest->owner = this;
        
        /* Translate the address, then copy to the address struct, and copy to request. */
        GetDecoder( )->Translate( request->address.GetPhysicalAddress( ), 
                               &row, &col, &bank, &rank, &channel, &subarray );
        request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
        request->bulkCmd = CMD_NOP;

        //std::cout << "Prefetching 0x" << std::hex << (*iter).GetPhysicalAddress() << " (trigger 0x"
        //          << request->address.GetPhysicalAddress( ) << std::dec << std::endl;

        /* Just type to issue; If the queue is full it simply won't be enqueued. */
        GetChild( pfRequest )->IssueCommand( pfRequest );
    }
}

void NVMain::IssuePrefetch( NVMainRequest *request )
{
    /* 
     *  Generate prefetches here. It makes the most sense to prefetch in this class
     *  since we may prefetch across channels. However, this may be applicable to any
     *  class in the hierarchy as long as we filter out the prefetch addresses that
     *  do not belong to a child of the current module.
     */
    /* TODO: We are assuming this is the master root! */
    std::vector<NVMAddress> prefetchList;
    if( prefetcher && request->type == READ && request->isPrefetch == false 
        && prefetcher->DoPrefetch(request, prefetchList) )
    {
        GeneratePrefetches( request, prefetchList );
    }
}

bool NVMain::CheckPrefetch( NVMainRequest *request )
{
    bool rv = false;
    NVMainRequest *pfRequest = NULL;
    std::list<NVMainRequest *>::const_iterator iter;
    std::vector<NVMAddress> prefetchList;

    for( iter = prefetchBuffer.begin(); iter!= prefetchBuffer.end(); iter++ )
    {
        if( (*iter)->address.GetPhysicalAddress() == request->address.GetPhysicalAddress() )
        {
            if( prefetcher->NotifyAccess(request, prefetchList) )
            {
                GeneratePrefetches( request, prefetchList );
            }

            successfulPrefetches++;
            rv = true;
            pfRequest = (*iter);
            delete pfRequest;
            break;
        }
    }

    if( pfRequest != NULL )
    {
        //std::cout << "Prefetched 0x" << std::hex << request->address.GetPhysicalAddress( )
        //          << std::dec << " (list size " << prefetchBuffer.size() << " -> ";
        prefetchBuffer.remove( pfRequest );
        //std::cout << prefetchBuffer.size() << ")" << std::endl;
    }

    return rv;
}

void NVMain::PrintPreTrace( NVMainRequest *request )
{
    /*
     *  Here we can generate a data trace to use with trace-based testing later.
     */
    if( p->PrintPreTrace || p->EchoPreTrace )
    {
        TraceLine tl;

        tl.SetLine( request->address,
                    request->type,
                    GetEventQueue( )->GetCurrentCycle( ),
                    request->data,
                    request->threadId 
                  );

        preTracer->SetNextAccess( &tl );
    }
}

bool NVMain::IssueCommand( NVMainRequest *request )
{
    ncounter_t channel, rank, bank, row, col, subarray;
    bool mc_rv;

    if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }

    /* Translate the address, then copy to the address struct, and copy to request. */
    GetDecoder( )->Translate( request->address.GetPhysicalAddress( ), 
                           &row, &col, &bank, &rank, &channel, &subarray );
    request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
    request->bulkCmd = CMD_NOP;

    /* Check for any successful prefetches. */
    if( CheckPrefetch( request ) )
    {
		//insert event into event queue , type is EventResponse , when process evnet , call this->RequestComplete
        GetEventQueue()->InsertEvent( EventResponse, this, request, 
                                      GetEventQueue()->GetCurrentCycle() + 1 );

        return true;
    }

    assert( GetChild( request )->GetTrampoline( ) == memoryControllers[channel] );
	//memoryController[i]->IssueCommand(i decided by request)
    mc_rv = GetChild( request )->IssueCommand( request );
    if( mc_rv == true )
    {
        IssuePrefetch( request );

        if( request->type == READ ) 
        {
            totalReadRequests++;
        }
        else
        {
            totalWriteRequests++;
        }

        PrintPreTrace( request );
    }

    return mc_rv;
}

bool NVMain::IssueAtomic( NVMainRequest *request )
{
    ncounter_t channel, rank, bank, row, col, subarray;
    bool mc_rv;

    if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }

    /* Translate the address, then copy to the address struct, and copy to request. */
    GetDecoder( )->Translate( request->address.GetPhysicalAddress( ), 
                           &row, &col, &bank, &rank, &channel, &subarray );
    request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
    request->bulkCmd = CMD_NOP;

    /* Check for any successful prefetches. */
    if( CheckPrefetch( request ) )
    {
        return true;
    }

    mc_rv = memoryControllers[channel]->IssueAtomic( request );
    if( mc_rv == true )
    {
        IssuePrefetch( request );

        if( request->type == READ ) 
        {
            totalReadRequests++;
        }
        else
        {
            totalWriteRequests++;
        }

        PrintPreTrace( request );
    }
    
    return mc_rv;
}

bool RBLA_NVMain::RequestComplete( NVMainRequest *request )
{
    bool rv = false;
	if( !col_incre_inited )
	{
		uint64_t cache_line_size;	
		if( (cache_line_size = dynamic_cast<DRAMCache*>(parent->GetTrampoline())->GetCacheLineSize())==-1)
				NVM::Fatal("you haven't set cache line size now!");

		col_incre = cache_line_size/memory_word_size;
		std::cout<<"memory word is:"<<memory_word_size<<"cache line size is"<<cache_line_size<<std::endl;
		std::cout<<"col_incre is "<<col_incre<<std::endl;
		
		if(col_incre<=0)
			NVM::Fatal("CacheLine size must no little than 64Bytes and make sure you the memory params are right !");
		col_incre_inited = true;
	}
    if( request->owner == this )
    {
        if( request->isPrefetch )
        {
            //std::cout << "Placing 0x" << std::hex << request->address.GetPhysicalAddress( )
            //          << std::dec << " into prefetch buffer (cur size: " << prefetchBuffer.size( )
            //          << ")." << std::endl;

            /* Place in prefetch buffer. */
            if( prefetchBuffer.size() >= p->PrefetchBufferSize )
            {
                unsuccessfulPrefetches++;
                //std::cout << "Prefetch buffer is full. Removing oldest prefetch: 0x" << std::hex
                //          << prefetchBuffer.front()->address.GetPhysicalAddress() << std::dec
                //          << std::endl;

                delete prefetchBuffer.front();
                prefetchBuffer.pop_front();
            }
            prefetchBuffer.push_back( request );
            rv = true;
        }
        else
        {
			std::cout<<"a migration done"<<std::endl;
            delete request;
            rv = true;
        }
    }
    else
    {
		//added on 2015/5/4 , modify stats table
		//if row buffer miss , modify stats table and decide whether to migrate
		if( !(request->rbHit) )
		{
			std::cout<<"row buffer miss"<<std::endl;
			bool ret = false;
			uint64_t row_num = GetRowNum( request );
			if( request->type==READ || request->type==READ_PRECHARGE) 
				ret = UpdateStatsTable( row_num , read_incre );
			if(request->type==WRITE || request->type==WRITE_PRECHARGE)
				ret = UpdateStatsTable( row_num , write_incre);
			//can migration
			if( ret )
			{
			   uint64_t pa = row_num , cur_col = 0;
			   uint64_t row , col , channel , rank , bank ,subarray;
			   translator->Translate(row_num , &row , &col , &bank , &rank , &channel , &subarray);
				
			   while(cur_col < cols)
			   {
				   NVMainRequest* migra_req = new NVMainRequest;
				   *migra_req = *request;
				   migra_req->type = MIGRATION;
				   migra_req->owner = this;
				   migra_req->address.SetTranslatedAddress(row ,cur_col , bank, rank, channel , subarray );
				   pa = translator->ReverseTranslate( row , cur_col , bank , rank , channel , subarray);
				   migra_req->address.SetPhysicalAddress(pa);

				   //insert migra event
					parent->GetTrampoline()->IssueCommand(migra_req);
					cur_col += col_incre;
			   }
			}
		}
		 
			rv = GetParent( )->RequestComplete( request );
    }

    return rv;
}

void RBLA_NVMain::Cycle( ncycle_t steps )
{
    assert( !p->EventDriven );
    /*
     *  Previous errors can prevent config from being set. Likewise, if the first memoryController is
     *  NULL, so are all the others, so return here instead of seg faulting.
     */
    if( !config || !memoryControllers )
      return;

    /* Sync the memory clock with the cpu clock. */
    double cpuFreq = static_cast<double>(p->CPUFreq);
    double busFreq = static_cast<double>(p->CLK);

    syncValue += static_cast<double>( busFreq / cpuFreq );

    if( syncValue >= 1.0f )
    {
        syncValue -= 1.0f;
    }
    else
    {
        return;
    }

    for( unsigned int i = 0; i < numChannels; i++ )
    {
        memoryControllers[i]->Cycle( 1 );
    }
    GetEventQueue()->Loop( steps );
}

void RBLA_NVMain::RegisterStats( )
{
    AddStat(totalReadRequests);
    AddStat(totalWriteRequests);
    AddStat(successfulPrefetches);
    AddStat(unsuccessfulPrefetches);

	NVMain::RegisterStats();
}

void RBLA_NVMain::CalculateStats( )
{
    for( unsigned int i = 0; i < numChannels; i++ )
        memoryControllers[i]->CalculateStats( );
}

/////////////////////////////////////////////////////////added on 2015/5/4

/*
 * function : update stats table when row buffer miss 
 * @row_num : row address (key of stats table)
 * @incre_num : when row buffer miss , increment num of miss_time
 *
 */
bool RBLA_NVMain::UpdateStatsTable ( uint64_t row_num , uint64_t incre_num)
{
	bool can_migration = false;
	uint64_t entry_id;
	StatsStoreBlock* stat_blk;
	//stats table hit
	if( (stat_blk = statsTable->FindEntry(row_num)))
	{
		std::cout<<"stats table hit"<<std::endl;
		statsHit++;
	}
	//stats table miss
	else
	{
		std::cout<<"stats table miss"<<std::endl;
		stat_blk = statsTable->FindVictim(entry_id);
		statsTable->Install(stat_blk , row_num);
		statsMiss++;
	}
	if(stat_blk)
	{
		std::cout<<"incre "<<incre_num<<std::endl;
		statsTable->IncreMissTime( stat_blk , incre_num );
		if(stat_blk->miss_time >= migra_thres)
		{
				std::cout<<"over migration threshold,can migration"<<std::endl;
				can_migration = true;
		}
	}
	return can_migration;
}

/*
 *
 *
 */
void RBLA_NVMain::AdjustMigrateThres()
{
	double net_benefit = CalculateBenefit(); 
	if( net_benefit<0)
		migra_thres++;
	else if ( net_benefit > pre_net_benefit )
	{
		if( pre_action == INCREMENT)
			migra_thres++;
		else
			migra_thres--;
	}
	else
	{
		if(pre_action == INCREMENT)
			migra_thres--;
		else
			migra_thres++;
	}
	pre_net_benefit = net_benefit;
}


/*
 *
 *
 */
double RBLA_NVMain::CalculateBenefit()
{
	return 1.0;
}


/*
 *
 *
 */
uint64_t RBLA_NVMain::GetRowNum(NVMainRequest* req)
{
	uint64_t row , col , rank , bank , channel , subarray;
	translator->Translate(req->address.GetPhysicalAddress() ,  \
			&row , &col , &bank , &rank , &channel , &subarray);
	//return row address
	return translator->ReverseTranslate(row , 0 , bank , rank , channel , subarray);
}

