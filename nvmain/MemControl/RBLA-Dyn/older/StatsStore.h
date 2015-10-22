/*************************************************************************
  > File Name: StateStore.h
  > Author: YuJie Chen
  > Mail: cyjseagull@163.com 
  > Created Time: Mon 02 Mar 2015 09:31:43 PM CST
  > Function: 
  > Para:
 ************************************************************************/

#include <iostream>
#include <list>
#include <climits>
#include <iterator>
#include "include/NVMainRequest.h"
#include "math.h"
#include "assert.h" 
#include "syswait.h"
namespace NVM{

	enum StatsEntryFlag
	{
		Invalid,
		Valid
	};

	enum StatsStoreFlag
	{
			IDLE,
			BUSY
	};

	enum MissType
	{
			READ,
			WRITE
	};

	//hardware structure in memory controller to track the row buffer locality statistics for small number of recently-accessed rows
	class StatsEntry
	{
		public:
			StatsEntry();

			//set functions
			void SetRowID(ncounter_t rowID)
			{
				this->indexID = indexID;
			}

			void SetMissCounter( ncounter_t missCounter)
			{
				this->missCounter = missCounter;
			}

			void SetFlag( StatsEntryFlag flag )
			{
				this->flag = flag;
			}

			//get functions
			ncounter_t GetRowID()
			{
				return this->indexID;
			}

			ncounter_t GetMissCounter()
			{
				return this->missCounter;
			}

			void GetFlag( )
			{
				return this->flag;
			}

			void MissIncrement( ncounter_t steps = 1 );

			void Invalidate()
			{
				flag = Invalid;
			}

			void Validate()
			{
				flag = Valid;
			}
		private:
			ncounter_t indexID;
			ncounter_t missCounter;
			StatsEntryFlag flag;

	};

	class StatsStoreOp
	{
		public:
			StatsStoreOp(ncounter_t statsStoreSize=16,ncounter_t readIncrementSteps=1,ncounter_t writeIncrementSteps=2 );
			~StatsStoreOp();

			void SetReadIncreSteps(ncounter_t steps)
			{
				this->ReadIncrementSteps = steps;
			}

			void SetWriteIncreSteps(ncounter_t steps)
			{
				this->WriteIncrementSteps = steps;
			}

			ncounter_t GetReadIncreSteps()
			{
				return this->ReadIncrementSteps;
			}

			ncounter_t GetWriteIncreSteps()
			{
				return this->writeIncrementSteps;
			}

			//find row buffer misses number according to index
			bool FindMissCounter(ncounter_t index , StatsEntry* entry);
			bool FindMissCounter(NVMainRequest* req, ncounter_t rows , StatsEntry* entry);

			//judge whether statsStore is full (size==statsStoreSize)
			bool Full();
			StatsEntry* Full();

			//when statsStore is full,evict the oldest entry
			StatsEntry* FindVictim();
			void Insert( ncounter_t row_id );	//Insert new stats store entry
			bool Read( ncounter_t row_id , ncounter_t &miss_counter );
			bool Update( ncounter_t row_id , MissType type);

			bool IsIssuable();
			bool IssueCommand();
			bool RequestComplete();
			
			bool ClearCounter(); //clear miss counter to 0 periodly in case of misscounter accumulation

		private:
			//stat store hardware related params
			ncounter_t statsStoreSize;	//stats store entry num(default is 16)
			std::list<StatsEntry *>statsStore;
			StatsStoreFlag state;
			ncounter_t readIncrementSteps,writeIncrementSteps;
	};

}

