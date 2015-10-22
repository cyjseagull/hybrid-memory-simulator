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

#include "NVM/RBLA_NVMain/RBLA_NVMain.h"
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


uint64_t RBLA_NVMain::migra_thres = 20 ;
Action RBLA_NVMain::pre_action = INCREMENT;
double RBLA_NVMain::pre_net_benefit = 0.0;

RBLA_NVMain::RBLA_NVMain( )
{
	stats_table_entry = 16;
	write_incre = 2;
	read_incre = 1;
	col_incre_inited = false;
	update_interval = 10000;
	migra_done = 0;
	migra_times = 0;
}

RBLA_NVMain::~RBLA_NVMain( )
{    
}

void RBLA_NVMain::SetConfig( Config *conf, std::string memoryName, bool createChildren )
{
	NVMain::SetConfig( conf , memoryName , createChildren );
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
	if(config->KeyExists("MigrationThres"))
		migra_thres = static_cast<uint64_t>(config->GetValue("MigrationThres"));
	//create stats table
	statsTable = std::auto_ptr<StatsStore>( new StatsStore(stats_table_entry));

	uint64_t cache_line_size;	
	if( (cache_line_size = dynamic_cast<DRAMCache*>(parent->GetTrampoline())->GetCacheLineSize())==-1)
		NVM::Fatal("you haven't set cache line size now!");

	col_incre = memory_word_size*cols/cache_line_size;
	std::cout<<"memory word is:"<<memory_word_size<<"cache line size is"<<cache_line_size<<std::endl;
	std::cout<<"col_incre is "<<col_incre<<std::endl;
		
	if(col_incre<=0)
		NVM::Fatal("CacheLine size must no little than 64Bytes and make sure you the memory params are right !");
}

bool RBLA_NVMain::RequestComplete( NVMainRequest *request )
{
    bool rv = false;
    if( request->owner == this )
    {
        if( request->isPrefetch )
        {
            /* Place in prefetch buffer. */
            if( prefetchBuffer.size() >= p->PrefetchBufferSize )
            {
                unsuccessfulPrefetches++;
                delete prefetchBuffer.front();
                prefetchBuffer.pop_front();
            }
            prefetchBuffer.push_back( request );
            rv = true;
        }
        else if(request->tag==MIGRATION)
        {
			migra_done++;
			std::cout<<"migration done:"<<migra_done<<std::endl;
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
			   migra_times++;
			   std::cout<<"migration begin: "<<migra_times<<std::endl;
			   std::cout<<"cols is "<<cols<<std::endl;
			   while(cur_col < col_incre)
			   {
				   NVMainRequest* migra_req = new NVMainRequest;
				   *migra_req = *request;
				   migra_req->tag = MIGRATION;
				   migra_req->owner = this;
				   migra_req->type = READ;
				   migra_req->address.SetTranslatedAddress(row ,cur_col , bank, rank, channel , subarray );
				   pa = translator->ReverseTranslate( row , cur_col , bank , rank , channel , subarray);
				   migra_req->address.SetPhysicalAddress(pa);
				   parent->GetTrampoline()->IssueCommand(migra_req);
				   cur_col++;
				   std::cout<<"RBLA_MEM: cur col is:"<<cur_col<<" address is "<<pa<<std::endl;
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
    
     //  Previous errors can prevent config from being set. Likewise, if the first memoryController is
     // NULL, so are all the others, so return here instead of seg faulting.
     
    if( !config || !memoryControllers )
      return;

    // Sync the memory clock with the cpu clock. 
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
	
	uint64_t cur_cycle = GetEventQueue()->GetCurrentCycle();
	//every update_interval ,call function "AdjustMigrateThres" adjust migra_thres
	if( !(cur_cycle%update_interval) )
		{
			AdjustMigrateThres();
			std::cout<<"current cycle is:"<<cur_cycle<<std::endl;
		}
}

void RBLA_NVMain::RegisterStats( )
{
	AddStat(migra_times);
	AddStat( migra_done);
	NVMain::RegisterStats();
}

void RBLA_NVMain::CalculateStats( )
{
	for( int i =0; i< numChannels ; i++)
	    memoryControllers[i]->CalculateStats( );

	NVMain::CalculateStats();
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
		statsTable->IncreMissTime( stat_blk , incre_num );
		if(stat_blk->miss_time >= migra_thres)
		{
				std::cout<<"over migration threshold,can migration"<<std::endl;
				//after migration set miss_time to 0?
				stat_blk->miss_time = 0;
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
	else if ( net_benefit >= pre_net_benefit )
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
	std::cout<<"Adjust: migration threshold : migration threshold is:"<<migra_thres<<std::endl;
	pre_net_benefit = net_benefit;
}


/*
 *
 *
 */
double RBLA_NVMain::CalculateBenefit()
{
	return 1.0 ;
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

