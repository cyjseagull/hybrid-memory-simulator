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

#ifndef __RBLA_NVMAIN_H__
#define __RBLA_NVMAIN_H__

#include <iostream>
#include <fstream>
#include <stdint.h>
#include "src/Params.h"
#include "src/NVMObject.h"
#include "src/Prefetcher.h"
#include "include/NVMainRequest.h"
#include "traceWriter/GenericTraceWriter.h"
#include "NVM/RBLA_NVMain/StatsStore.hh"
#include "include/Exception.h"
#include <memory>


namespace NVM {

class Config;
class MemoryController;
class MemoryControllerManager;
class Interconnect;
class AddressTranslator;
class SimInterface;
class NVMainRequest;
//action
enum Action
{
	DECREMENT = 0,
	INCREMENT = 1
};


class RBLA_NVMain : public NVMain
{
  public:
    RBLA_NVMain( );
    ~RBLA_NVMain( );

    void SetConfig( Config *conf, std::string memoryName = "defaultMemory", bool createChildren = true );

    Config *GetConfig( );

    void IssuePrefetch( NVMainRequest *request );
    bool IssueCommand( NVMainRequest *request );
    bool IssueAtomic( NVMainRequest *request );
    bool IsIssuable( NVMainRequest *request, FailReason *reason );

    bool RequestComplete( NVMainRequest *request );

    bool CheckPrefetch( NVMainRequest *request );

    void RegisterStats( );
    void CalculateStats( );

    void Cycle( ncycle_t steps );

  protected:
	bool UpdateStatsTable ( uint64_t row_num , uint64_t incre_num);
	void AdjustMigrateThres();
	uint64_t GetRowNum(NVMainRequest* req);
    double CalculateBenefit();

  private:
    Config *config;
    Config **channelConfig;
    MemoryController **memoryControllers;
    AddressTranslator *translator;
    SimInterface *simInterface;

    ncounter_t totalReadRequests;
    ncounter_t totalWriteRequests;
    ncounter_t successfulPrefetches;
    ncounter_t unsuccessfulPrefetches;
	
	std::auto_ptr<StatsStore> statsTable;
	ncounter_t statsHit , statsMiss;
	double statsHitRate;
	//added on 2015/5/4
	uint64_t stats_table_entry;
	uint64_t write_incre , read_incre;
	uint64_t update_interval;
 
	uint64_t memory_word_size;
	//static vars
	static uint64_t migra_thres;    //migration threshold   
	static NVM::Action pre_action;
	static double pre_net_benefit;
	bool col_incre_inited;

	int cols;
	uint64_t col_incre;

    unsigned int numChannels;
    double syncValue;

    Prefetcher *prefetcher;
    std::list<NVMainRequest *> prefetchBuffer;

    std::ofstream pretraceOutput;
    GenericTraceWriter *preTracer;

    void PrintPreTrace( NVMainRequest *request );
    void GeneratePrefetches( NVMainRequest *request, std::vector<NVMAddress>& prefetchList );
};

};

#endif
