/*************************************************************************
    > File Name: RBLA_Dyn.cpp
    > Author: YuJie Chen
    > Mail: cyjseagull@163.com 
    > Created Time: Mon 02 Mar 2015 10:47:59 AM CST
    > Function: 
    > Para:
 ************************************************************************/
#include "MemControl/RBLA-Dyn/RBLA_Dyn.h"

RBLA_Dyn::RBLA_Dyn()
{
	mainMemory = NULL;
	mainMemoryCOnfig = NULL;
	functionalCache = NULL;
	cacheSize = 0;
	assoc = 1;	//default is fully associative DRAM cache
	cacheLineSize = 4*power(2,10);	//default cache line size is 4KB(same as page size)

	statsStoreSize = 16;
	writeIncrementSteps = 2;	//default steps when writing data
	readIncrementSteps = 1;		//default steps when reading data

	//init statistic value
	DRAMCache_hits = 0;
	DRAMCache_misses = 0;
	DRAMCache_evicts = 0;

	rb_hits = 0;
	rb_misses = 0;
	rb_evicts = 0;

	DRAMCache_hitrate = 0;
}


RBLA_Dyn::~RBLA_Dyn()
{
}

void RBLA_Dyn::SetFunctionalCache( CacheBank *cache)
{
	functionalCache = cache;
}

//byte is the uniform unit,default is B
void RBLA_Dyn::SetCacheSize( uint64_t size, Unit unit)
{
	switch( unit )
	{
		case KB:
			cacheSize = size * pow(2,10);
			break;
		case MB:
			cacheSize = size * pow(2,20);
			break;
		case GB:
			cacheSize = size * pow(2,30);
			break;
		default:
			cacheSize = size;
			break;
	}
}

//set cache size
void RBLA_Dyn::SetCacheSize( uint64_t size, std::string unit)
{

	if( unit == "KB" )
	{
		cacheSize = size*pow(2,10);
		break;
	}

	if( unit == "MB" )
	{
		cacheSize = size*pow(2,20);
		break;
	}

	if( unit == "GB" )
	{
		cacheSize = size*pow(2,30);
		break;
	}
	else
	{	
		cacheSize = size;
	}
}

//get cache size
uint64_t RBLA_Dyn::GetCacheSize()
{
	return cacheSize;
}


//set config
void RBLA_Dyn::SetConfig( Config *conf,bool createChildren = true)
{
	//mainMemoryConfig tmp;
	uint64_t banks , channels , ranks ,cols , bytes_off;
	//default cache size is 1GB
	uint64_t cacheSize = power(2,30);

//	if( conf->KeyExists("CacheSize") )
//	{
//		cacheSize_tmp = static_cast<ncounter_t>( conf->GetValue("CacheSize") );
//	}

	if(conf->KeyExists("CacheSizeUnit"))
	{
		unit_tmp = conf->GetString("CacheSizeUnit");
	}

	if(conf->KeyExists("Assoc"))
	{
		this->assoc = conf->GetValue("Assoc");
	}
	//get cache line size
	if( conf->KeyExists("CacheLineSize") )
	{
		this->cacheLineSize = conf->GetValue("CacheLineSize");
	}

	if( conf->KeyExists("StatsStoreSize"))
	{
		this->statsStoreSize = conf->GetValue("StatsStoreSize");
	}

	if( conf->KeyExists("WriteIncrementSteps") )
	{
		this->writeIncrementSteps = conf->GetValue("WriteIncrementSteps");
	}
	if(cof->KeyExists("ReadIncrementSteps"))
		this->readIncrementSteps = conf->GetValue("ReadIncrementSteps");
	//get cache size
	if(conf->KeyExists("ROWS"))
	{
		this->rows = conf->GetValue("ROWS");
		if(conf->KeyExists("BANKS"))
		{
			banks = conf->GetValue("BANKS");
			if(conf->KeyExists("RANKS"))
			{
					ranks = conf->GetValue("RANKS");
					if(conf->KeyExists("COLS"))
					{
						cols = conf->("COLS");
						if( conf->KeyExists("BusWidth"))
						{
								bytes_off = conf->GetValue("BusWidth")/8;
								cacheSize = banks * ranks * cols * byte_off ;
						}
					}
			}
		}
	}

	this->sets = cacheSize/(cacheLineSize*assoc);

	//initialize cache bank
	functionalCache = new CacheBank( sets assoc ,cacheLineSize);

	//initialize StatsStore
	SetStatsStore(new StatsStoreOp(statsStoreSize,readIncrementSteps,writeIncrementSteps));
	
	
	MemoryController::SetConfig( conf , createChildren);
	SetDebugName("RBLA-Cache",conf);
}

 //set main memory
void RBLA_Dyn::SetMainMemory(NVMain *memory)
{
	mainMemory = memory;
}


/***************************************************************
 * issue atomic : issue request without timing model to fastforward
 * 				  simulation
 *
 * ************************************************************/
bool RBLA_Dyn::IssueAtomic(NVMainRequest *req)
{
	if( req->address.GetPysicalAddress() > max_addr )   max_addr = req->address.GetPysicalAddress();
	
	//hit
	if( functionalCache->Present( req->address ) )
	{
		hits++;
	}
	//miss
	else
	{
		NVMDataBlock evictd_data;
		NVMAddress victim;
		misses++;
		//replace
		if( functionalCache->SetFull(req->address) )
		{
			(void)functionalCache->ChooseVictim( req->address , &victim );
			(void)functionalCache->Evict( victim, &evictd_data );
			DRAMCache_evict++;
		}
		
		(void)functionalCache->Install( req->address ,  );

	}	
}

/*********************************************
 *issue functional:  issue request without updating state
 *
 *********************************************/
bool RBLA_Dyn::IssueFunctional(NVMainRequest *req)
{
	return functionalCache->Present(req->address);
}


/**************************************************
 *issue command: issue request with timing model
 *
 **************************************************/
bool RBLA_Dyn::IssueCommand(NVMainRequest *req)
{
	
	if( req->address.GetPhysicalAddress() > max_addr ) 	
}

bool RBLA_Dyn::RequestComplete(NVMainRequest *req)
{

}

//////////////////////////////////////////////////////////
void RBLA_Dyn::Cycle(ncycle_t steps)
{

}

void RBLA_Dyn::RegisterStats()
{

}

void RBLA_Dyn::CalculateStats()
{

}

///////////////////////////////////////////////////////

