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
#include "NVM/Fine_NVMain/FineNVMain.h"
#include "src/EventQueue.h"
#include "MemControl/MemoryControllerFactory.h"
#include "traceWriter/TraceWriterFactory.h"
#include "Decoders/DecoderFactory.h"
#include "include/NVMainRequest.h"
#include "include/NVMHelpers.h"
#include "Prefetchers/PrefetcherFactory.h"
#include <vector>

#include <sstream>
#include <cassert>
using namespace std;
using namespace NVM;
FineNVMain::FineNVMain( )
{
    config = NULL;
	mem_size = 0;
	mem_word_size = 0;
	cache_size = 0;
	cache_word_size = 0;

	reserved_channels = 0;
	reservedControllers = NULL;
	cacheTranslator = NULL;
	translator = NULL;

	channelConfig = NULL;
	reservedConfig = NULL;
	memoryControllers = NULL;
	prefetcher = NULL;
	preTracer = NULL;
	reserve_region_param = NULL;
	syncValue = 0.0;
	is_configed = false;
	std::cout<<"Fine memory"<<std::endl;
}

uint64_t FineNVMain::GetMemorySize()
{
	return mem_size;
}

uint64_t FineNVMain::GetWordSize()
{
	return mem_word_size;
}

FineNVMain::~FineNVMain( )
{
    if( config ) 
        delete config;
    
    if( memoryControllers )
    {
        for( unsigned int i = 0; i < numChannels; i++ )
        {
            if( memoryControllers[i] )
                delete memoryControllers[i];
        }

        delete [] memoryControllers;
    }

	if( reservedControllers)
	{
		for( uint64_t i=0 ; i < reserved_channels;i++)
			delete reservedControllers[i];
		delete []reservedControllers;
	}

    if( channelConfig )
    {
        for( unsigned int i = 0; i < numChannels; i++ )
        {
            delete channelConfig[i];
        }

        delete [] channelConfig;
    }

	if( reservedConfig)
	{
		for( uint64_t i=0 ; i<reserved_channels ; i++)
			delete reservedConfig[i];
		delete [] reservedConfig;
	}
	if( reserve_region_param )
	{
		delete reserve_region_param;
	}
	if(cacheTranslator)
		delete cacheTranslator;
}


Config *FineNVMain::GetConfig( )
{
    return config;
}


void FineNVMain::SetConfig( Config *conf, std::string memoryName, bool createChildren )
{
	Params* params = new Params( );
    params->SetParams( conf );
    SetParams( params );

    StatName( memoryName );
    config = conf;
	std::string buffer_decoder = "BufferDecoder";
	if( conf->KeyExists("DRAMBufferDecoder"))
		buffer_decoder = conf->GetString("DRAMBufferDecoder");

	 cacheTranslator = DecoderFactory::CreateNewDecoder( buffer_decoder);
    if( config->GetSimInterface( ) != NULL )
        config->GetSimInterface( )->SetConfig( conf, createChildren );
    else
      std::cout << "Warning: Sim Interface should be allocated before configuration!" << std::endl;
    if( createChildren )
    {
        uint64_t channels = static_cast<int>(p->CHANNELS);
		reserved_channels = static_cast<int>( p->ReservedChannels);
		
		SetTranslators(conf , translator , p , mem_width ,mem_word_size, mem_size);
		SetDecoder( translator);

		std::cout<<"reserved_channel num:"<<reserved_channels<<std::endl;
		std::cout<<"channels:"<<channels<<std::endl;
        memoryControllers = new MemoryController* [channels];
        channelConfig = new Config* [channels];	

		SetMemoryControllers( memoryControllers ,channelConfig,
							  conf , channels ,
							  "CONFIG_CHANNEL" ,"DRAMBuffer" );
		
		if( reserved_channels )
		{
			reservedControllers = new MemoryController* [reserved_channels];
			reservedConfig = new Config* [reserved_channels];
			reserve_region_param = new Params;
			SetMemoryControllers(reservedControllers , reservedConfig , 
								 conf , reserved_channels , 
								 "CONFIG_DRAM_CHANNEL" , memoryName);

			reserve_region_param->SetParams(reservedConfig[0]);	//set reserve region param
			SetTranslators(	reservedConfig[0]  , cacheTranslator , 
							reserve_region_param , cache_width,
							cache_word_size , cache_size);
			cacheTranslator->SetAddrWidth( cache_width );
		}
	}

	if( p->MemoryPrefetcher != "none" )
	{
		prefetcher = PrefetcherFactory::CreateNewPrefetcher( p->MemoryPrefetcher );
		std::cout << "Made a " << p->MemoryPrefetcher << " prefetcher." << std::endl;
	}

    numChannels = static_cast<unsigned int>(p->CHANNELS);
    
    std::string pretraceFile;

    if( p->PrintPreTrace || p->EchoPreTrace )
    {
        if( config->GetString( "PreTraceFile" ) == "" )
            pretraceFile = "trace.nvt";
        else
            pretraceFile = config->GetString( "PreTraceFile" );

        if( pretraceFile[0] != '/' )
        {
            pretraceFile  = NVM::GetFilePath( config->GetFileName( ) );
            pretraceFile += config->GetString( "PreTraceFile" );
        }

        std::cout << "Using trace file " << pretraceFile << std::endl;

        if( config->GetString( "PreTraceWriter" ) == "" )
            preTracer = TraceWriterFactory::CreateNewTraceWriter( "NVMainTrace" );
        else
            preTracer = TraceWriterFactory::CreateNewTraceWriter( config->GetString( "PreTraceWriter" ) );

        if( p->PrintPreTrace )
            preTracer->SetTraceFile( pretraceFile );
        if( p->EchoPreTrace )
            preTracer->SetEcho( true );
    }
    RegisterStats();
	is_configed = true;
}



void FineNVMain::SetTranslators( Config* conf , AddressTranslator* &translator ,
								 Params* param , uint64_t &bit_width ,
								 uint64_t &word_size , uint64_t &mem_size )
{
	uint64_t cols , rows , banks , ranks , channels , subarrays;
	unsigned int col_bits, row_bits , bank_bits , rank_bits , channel_bits , subarray_bits;
	if( !param )
		return;

	if( conf->KeyExists( "Decoder" ) )
		translator = DecoderFactory::CreateNewDecoder( conf->GetString("Decoder"));
	else
		translator = new AddressTranslator();
	
	cols = static_cast<uint64_t>(param->COLS);
	rows = static_cast<uint64_t>(param->ROWS);
	banks = static_cast<uint64_t>(param->BANKS);
	ranks = static_cast<uint64_t>(param->RANKS);
	channels = static_cast<uint64_t>(param->CHANNELS);
	if( conf->KeyExists("MATHeight"))
	{
		uint64_t mat_height = static_cast<uint64_t>( p->MATHeight);
		subarrays =	static_cast<uint64_t>(mat_height/rows); 
	}
	else
		subarrays = 1;
	col_bits = NVM::mlog2(cols);
	row_bits = NVM::mlog2(rows);
	bank_bits = NVM::mlog2(banks);
	rank_bits = NVM::mlog2(ranks);
	channel_bits = NVM::mlog2(channels);
	subarray_bits = NVM::mlog2(subarrays);
	TranslationMethod *method = new TranslationMethod();
	bit_width = static_cast<uint64_t>(col_bits + row_bits + bank_bits
									  + rank_bits + channel_bits + subarray_bits );
	method->SetBitWidths( row_bits , col_bits ,
						  bank_bits , rank_bits,
						  channel_bits , subarray_bits );
	method->SetCount( rows , cols , banks , ranks , channels , subarrays);
	method->SetAddressMappingScheme( param->AddressMappingScheme);
	translator->SetTranslationMethod( method);
	translator->SetDefaultField(CHANNEL_FIELD);
	word_size =	static_cast<uint64_t> (param->tBURST * param->RATE * param->BusWidth/8);
	mem_size = static_cast<uint64_t>( word_size*cols*rows*banks*ranks*channels*subarrays );
}

void FineNVMain::SetMemoryControllers( MemoryController** &mem_ctl ,
									   Config** &ctl_conf , Config* conf ,
									   int num , std::string base_str ,
									   std::string memory_name )
{
	if( !num )
		return;

	std::stringstream confStr;
	std::string channelConfigFile;
	int i=0;
	while( i <  num)
	{
		ctl_conf[i] = new Config( *config );
		confStr<< base_str << i ;
		if( conf->GetString(confStr.str()) !="" )
		{
			//relative path,get config file path
			if( channelConfigFile[0]!='/')
			{
				channelConfigFile = NVM::GetFilePath( conf->GetFileName() );
				channelConfigFile += conf->GetString( confStr.str()) ;
			}
			//read channel config 
			ctl_conf[i]->Read(channelConfigFile);
		}
		//create memory controller
		mem_ctl[ i ] = MemoryControllerFactory::CreateNewController( ctl_conf[i ]->GetString("MEM_CTL"));

		confStr.str("");
		confStr << memory_name << ".channel" <<i <<"."<<ctl_conf[i]->GetString("MEM_CTL");
		mem_ctl[i]->StatName(confStr.str());
		mem_ctl[i]->SetID(i);
		AddChild( mem_ctl[i]);
		mem_ctl[i]->SetParent(this);
		mem_ctl[i]->SetConfig( ctl_conf[i] , true);
		mem_ctl[i]->RegisterStats();
		i++;
		confStr.str("");
	}
}

bool FineNVMain::IsIssuable( NVMainRequest *request, FailReason *reason )
{
	uint64_t row, col , rank , bank , channel, subarray;
	uint64_t pa = request->address.GetPhysicalAddress();
	//main memory
	if( pa < mem_size)
	{
		GetDecoder()->Translate( pa , &row , &col , &rank , &bank, &channel , &subarray);
		return memoryControllers[channel]->IsIssuable(request , reason);
	}
	//dram buffer
	else
	{
		cacheTranslator->Translate( pa , &row , &col , &rank , &bank , &channel , &subarray);
		return reservedControllers[channel]->IsIssuable(request,reason);
	}
}



bool FineNVMain::IssueCommand( NVMainRequest *request )
{
    ncounter_t channel, rank, bank, row, col, subarray;
	uint64_t pa = request->address.GetPhysicalAddress();
    bool mc_rv;
	bool access_cache = false;
	uint64_t page_num = pa>>12;	//get page num(page size:4KB)
	if( !page_access_map_.count(page_num))
	{
		page_access_map_[page_num] = 1;
	}
	else
		page_access_map_[page_num]++;

	if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }
    /* Translate the address, then copy to the address struct, and copy to request. */
	//access main memory 
	if( pa < mem_size )
	{
		GetDecoder( )->Translate( pa, &row, &col, &bank, &rank, &channel, &subarray );
	}
	else
	{
		//translator should be modified 
		cacheTranslator->Translate( pa , &row , &col , &bank , &rank , &channel , &subarray);
		access_cache = true;
		std::cout<<"access dram cache"<<std::endl;
	}

		request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
	    request->bulkCmd = CMD_NOP;


		/* Check for any successful prefetches. */
	    if( CheckPrefetch( request ) )
		{
			//insert event into event queue , type is EventResponse ,
			//when process evnet , call this->RequestComplete
	        GetEventQueue()->InsertEvent( EventResponse, this, request, 
                                      GetEventQueue()->GetCurrentCycle() + 1 );
	
		    return true;
	    }

		if( access_cache )
			mc_rv = GetChild( channel + numChannels)->IssueCommand(request);

		else
		{
			assert( GetChild( request )->GetTrampoline( ) == memoryControllers[channel] );
			mc_rv = GetChild( request )->IssueCommand( request );
		}

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

bool FineNVMain::IssueAtomic( NVMainRequest *request )
{
	//std::cout<<"FineNVMain: issue atomic"<<std::endl;

    ncounter_t channel, rank, bank, row, col, subarray;
    bool mc_rv;
	bool access_cache = false;

    if( !config )
    {
        std::cout << "NVMain: Received request before configuration!\n";
        return false;
    }
	
	uint64_t pa = request->address.GetPhysicalAddress();
    /* Translate the address, then copy to the address struct, and copy to request. */
	if ( pa < mem_size )
	{
		//std::cout<<"access main memory"<<std::endl;
		GetDecoder( )->Translate( pa, 
                           &row, &col, &bank, &rank, &channel, &subarray );
	}
	else
	{
		//translator should be modified
		cacheTranslator->Translate( pa , &row , &col , &bank , &rank , &channel , &subarray);
		access_cache = true;
		//std::cout<<"access dram cache"<<std::endl;
	}
    request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
    request->bulkCmd = CMD_NOP;
    /* Check for any successful prefetches. */
    if( CheckPrefetch( request ) )
    {
        return true;
    }
	
	if( access_cache )
		mc_rv = memoryControllers[reserved_channels+ numChannels ]->IssueAtomic(request);
	else
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


void FineNVMain::RegisterStats( )
{
	NVMain::RegisterStats();
	AddStat( reserved_channels );
	AddStat( mem_size );
	AddStat( cache_size );
	AddStat( mem_width );
	AddStat( cache_width );
}


void FineNVMain::CalculateStats( )
{
	NVMain::CalculateStats();
}

void FineNVMain::Cycle( ncycle_t steps )
{
	//std::cout<<"FineNVMain:Cycle"<<std::endl;
	assert( !p->EventDriven );
	if( (!channelConfig || !memoryControllers)&&( !reservedConfig || !reservedControllers ) )
	{
		//std::cout<<"config or controllers is NULL"<<std::endl;
		return;
	}
	double cpuFreq = static_cast<double>(p->CPUFreq);
	double busFreq = static_cast<double>(p->CLK);

	syncValue += static_cast<double>( busFreq / cpuFreq );
	//std::cout<<"syncValue is "<<syncValue<<std::endl;
	if( syncValue >= 1.0f )
	{   
		//std::cout<<"enter sync value"<<std::endl;
		syncValue -= 1.0f;
    }
	else 
		return;
	
     if(numChannels)
	    {   
			//std::cout<<"cycle mmorycontrollers"<<std::endl;
			for( unsigned int i = 0; i < numChannels; i++ )
				{   
				
					memoryControllers[i]->Cycle( 1 );
				}   
		}
	 if(reserved_channels)
	 {

		 //std::cout<<"cycle reservedcontrollers"<<std::endl;
		 for( uint64_t i=0 ;i <reserved_channels ; i++)
		 {
			 reservedControllers[i]->Cycle(1);
		 }
	 }
	GetEventQueue()->Loop( steps );
	std::map<uint64_t , uint64_t>::iterator it;
	uint64_t current_cycle = GetEventQueue()->GetCurrentCycle();
	std::vector< std::pair<uint64_t , uint64_t> > vec;
	std::vector< std::pair<uint64_t , uint64_t> >::iterator vec_it;
	//output counter every 10000cycles
	if( !(current_cycle%100000))
	{
		std::cout<<"current cycle:"<<current_cycle<<std::endl;
		for( it=page_access_map_.begin();it!=page_access_map_.end();it++)
		{
			vec.push_back( std::make_pair( it->first , it->second) );
		}
		sort( vec.begin() , vec.end() ,cmp);
		for( vec_it = vec.begin() ; vec_it!=vec.end();vec_it++)
		{
			std::cout<<vec_it->first<<" "<<vec_it->second<<std::endl;
		}
		std::cout<<std::endl<<std::endl;
	}
	
}

