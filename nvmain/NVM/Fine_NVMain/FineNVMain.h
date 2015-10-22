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

#ifndef __FINE_NVMAIN_H__
#define __FINE_NVMAIN_H__

#include <iostream>
#include <fstream>
#include <stdint.h>
#include "NVM/nvmain.h"
#include "src/NVMObject.h"
#include "src/Prefetcher.h"
#include "include/NVMainRequest.h"
#include "traceWriter/GenericTraceWriter.h"
#include "include/Exception.h"
#include <map>
#include <iterator>
#include <algorithm>
namespace NVM {

class FineNVMain : public NVMain
{
  public:
    FineNVMain( );
    ~FineNVMain( );
    virtual void SetConfig( Config *conf, std::string memoryName = "MainMemory",
							bool createChildren = true );
    Config *GetConfig( );
	bool IsIssuable( NVMainRequest *request , FailReason* reason);
    bool IssueCommand( NVMainRequest *request );
    bool IssueAtomic( NVMainRequest *request );
	void Cycle( ncycle_t steps = 1 );
    void RegisterStats( );
	void CalculateStats();
	

	uint64_t GetMemorySize();
	uint64_t GetWordSize();
	
	uint64_t GetBufferSize()
	{
		if(!is_configed)
			NVM::Warning( "haven't set configuration yet,	\
						returned buffer size has non-sense!");
		return cache_size;
	}
	uint64_t GetBufferWordSize()
	{
		if(!is_configed)
			NVM::Warning("haven't set configuration yet ,	\
					returned buffer word size has non-sense!");
		return cache_word_size;
	}

	uint64_t mem_size;
	uint64_t cache_size;

	uint64_t mem_word_size;
	uint64_t cache_word_size;

	uint64_t mem_width;
	uint64_t cache_width;
	uint64_t reserved_channels;
	
	typedef std::pair<uint64_t , uint64_t> pair;
	std::map<uint64_t , uint64_t> page_access_map_; 
  protected:
    Config *config;
    Config **channelConfig;
	Config **reservedConfig;
    MemoryController **memoryControllers;
	MemoryController **reservedControllers;
	
	AddressTranslator *translator;
	AddressTranslator *cacheTranslator;

    unsigned int numChannels;

    Prefetcher *prefetcher;

    std::ofstream pretraceOutput;
    GenericTraceWriter *preTracer;

    void GeneratePrefetches( NVMainRequest *request,
							std::vector<NVMAddress>& prefetchList );
 
  private:
	void SetMemoryControllers( MemoryController** &mem_ctl , Config** &ctl_conf,
							   Config* conf , int num , std::string base_str , 
							   std::string memory_name);
	void SetTranslators( Config* conf , AddressTranslator* &translator ,
						Params* param , uint64_t &bit_width , 
						uint64_t &word_size , uint64_t &mem_size );

	static int cmp( const pair &x , const pair &y)
	{
		return x.second > y.second;
	}

	Params* reserve_region_param;
	bool is_configed;
};
};

#endif
