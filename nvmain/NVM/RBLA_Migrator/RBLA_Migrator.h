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

#ifndef __RBLA_Migrator_H__
#define __RBLA_Migrator_H__

#include <iostream>
#include <fstream>
#include <stdint.h>
#include "src/Params.h"
#include "src/NVMObject.h"
#include "src/Prefetcher.h"
#include "include/NVMainRequest.h"
#include "traceWriter/GenericTraceWriter.h"
#include "MemControl/DRAMCache/DRAMCache.h"
#include "NVM/RBLA_NVMain/StatsStore.hh"
#include "NVM/nvmain.h"
#include <memory>


namespace NVM {

#define MIGRATION tagGen->CreateTag("MIGRATION")
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

    //bool IssueCommand( NVMainRequest *request );
    //bool IssueAtomic( NVMainRequest *request );
    //bool IsIssuable( NVMainRequest *request, FailReason *reason );

    bool RequestComplete( NVMainRequest *request );

    void RegisterStats( );
    void CalculateStats( );
	void Cycle( ncycle_t steps );

  protected:
	bool UpdateStatsTable ( uint64_t row_num , uint64_t incre_num);
	void AdjustMigrateThres();
	//double CalculateBenefit();
	uint64_t GetRowNum(NVMainRequest* req);
	double CalculateBenefit();

  private:
	std::auto_ptr<StatsStore> statsTable;
	ncounter_t statsHit , statsMiss;
	double statsHitRate;
	////////////////////////////////////////
	
	//added on 2015/5/4
	uint64_t stats_table_entry;
	uint64_t write_incre , read_incre;
	uint64_t update_interval;

	uint64_t memory_word_size;
	//static vars
	static uint64_t migra_thres;	//migration threshold	
	static NVM::Action pre_action;
    static double pre_net_benefit;	
	bool col_incre_inited;

	int cols;
	uint64_t col_incre;
	
	uint64_t migra_times , migra_done;
};
};

#endif
