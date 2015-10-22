/*************************************************************************
    > File Name: RBLA_Dyn.h
    > Author: YuJie Chen
    > Mail: cyjseagull@163.com 
    > Created Time: Mon 02 Mar 2015 10:32:12 AM CST
    > Function: 
    > Para:
 ************************************************************************/
#ifndef _RBLA_DYN_
#define _RBLA_DYN_


#include <iostream>
#include "MemControl/DRAMCache/AbstractDRAMCache.h"
#include "NVM/nvmain.h"
#include "Utils/Caches/CacheBank.h"
#include "math.h"
#include "include/global_vars.h"

namespace NVM{

	enum Unit
	{
		B,KB,MB,GB
	};

	//class AbstractDRAMCache has compliment the function of ordinary Memory Controller
	class RBLA_Dyn:public AbstractDRAMCache
	{
		public:
			RBLA_Dyn();
			virtual ~RBLA_Dyn();
			//set config
			void SetConfig(Config *conf,bool createChildren = true);
			//set main memory 
			void SetMainMemory(NVMain *memory);
			void SetFunctionalCache(CacheBank* cache);
			void SetCacheSize( uint64_t size , Unit unit = B);
			void SetCacheSize( unint64_t size, std::string unit);
			void SetStatsStore(StatsStoreOp* statsStore)
			{
				this->statsStore = statsStore;
			}

			//get functions
			uint64_t GetCacheSize()
			{
				return this->statsStore;
			}

			StatsStoreOp* GetStatsStore()
			{
				return this->statsStore;
			}

			//issue
			bool IssueAtomic(NVMainRequest *req);
			bool IssueFunctional(NVMainRequest *req);
			bool IssueCommand(NVMainRequest *req);
			bool RequestComplete(NVMainRequest *req);
			void Cycle(ncycle_t);

			//stats related
			void RegisterStats();
			void CalculateStats();
		private:
			NVMain *mainMemory;
			Config *mainMemoryConfig;
			//object ptr pointed to cache(for example:DRAM cache)
			CacheBank *functionalCache;

			//cache related params
			uint64_t cacheSize;
			ncounter_t assoc;
			ncounter_t sets;
			ncounter_t cacheLineSize;
			ncounter_t rows;	//row number
			
			//stat store hardware related params
			ncounter_t statsStoreSize;	//stats store entry num(default is 16)

			//statistics 
			ncounter_t DRAMCache_hits,DRAMCache_misses , DRAMCache_evicts;
			ncounter_t rb_hits,rb_misses,rb_evicts;
			ncounter_t writeIncrementSteps,readIncrementSteps;
			double DRAMCache_hitrate;	
			StatsStoreOp *statsStore;
	};
}


#endif
