/*************************************************************************
    > File Name: StateStore.cpp
    > Author: YuJie Chen
    > Mail: cyjseagull@163.com 
    > Created Time: Mon 02 Mar 2015 09:35:39 PM CST
    > Function: 
    > Para:
 ************************************************************************/
#include "StatsStore.h"

StatsEntry::StatsEntry()
{
	this->missCounter = 0; 
	this->indexID = UINT_MAX;
	this->flag = Invalid;
}

void StatsEntry::MissIncrement(ncounter_t steps)
{
	this->missCounter += steps;
}

StatsStoreOp::StatsStoreOp(ncounter_t statsStoreSize,ncounter_t readIncrementStep,ncounter_t writeIncrementStep)
{
	this->statsStoreSize = statsStoreSize;
	this->readIncrementSteps = ReadIncrementStep;
	this->writeIncrementSteps = writeIncrementStep;
	for(int i=0 ; i<statsStoreSize ; i++)
	{
		statsStore.push_back(new StatsEntry());
	}
	state = IDLE;
}

StatsStoreOp::~StatesStoreOp()
{
	std::list<StatsEntry *>::iterator it;
	if(!statsStore.empty())
	{
		for( it = statsStore.begin() ; it != statsStore.end() ; it++ )
		{
			delete (*it);
		}
	}
}
	
bool StatsStoreOp::FindMissCounter( ncounter_t index , StatsEntry *entry )
{
	bool flag = false;
	std::list<StatsEntry *>::iterator it;
	for( it = statsStore.begin(); it != statsStore.end() ; it++ )
	{
		if(index == (*it)->indexID)
		{
			this->entry = *it;
			flag = true;
			break;
		}
	}
	return flag;
}


bool StatsStoreOp::FindMissCounter( NVMainRequest* req , ncounter_t rows,StatsEntry* entry)
{
	ncounter_t indexID;
	indexID = req->addr.GetPhysicalAddress()>>log2(rows);
	return	FindMissCounter(indexID,entry);
}

bool StatsStoreOp::Full()
{
	bool flag = true;
	std::list<StatsEntry *>::iterator it;
	for( it = statsStore.begin() ; it != statsStore.end() ; it++)
	{
		if( (*it)->GetFlag() == Invalid )
		{
			flag = false;
			break;
		}
	}
	return flag;
}

StatsEntry* StatsStoreOp::Full()
{
	std::list<StatsEntry* >::iterator it;
	StatsEntry *entry=NULL;
	for(it = statsStore.begin(); it != statsStore.end(); it++)
	{
		if((*it)->GetFlag() == Invalid )
		{
			entry = *it;
			break;
		}
	}
	return entry;
}

//evict the oldest stats store entry
statsEntry* StatsStoreOp::FindVictim()
{
	return	*(--statsStore.end());
}

//insert new stats store entry into statsStore
void StatsStoreOp::Insert( ncounter_t row_id )
{
	state = busy;
	StatsEntry *insertEntry;
	if ((insertEntry = Full())== NULL)
	{
		insertEntry = FindVictim();
	}
	insertEntry->validate();
	insertEntry->SetMissCounter(0);
	insertEntry->SetRowID( row_id );
	statsStore.remove( insertEntry );
	statsStore.push_front(insertEntry);
}

//read row_id specified missCounter
bool Read( ncounter_t row_id , ncounter_t &miss_counter )
{
	StatsEntry *entry = NULL;
	bool isFound = FindMissCounter( row_id , entry );
	if( isFound )
	{
		miss_counter = entry->GetMissCounter();
		if(entry !=(*statsStore.begin()))
		{
			statsStore.remove(entry);
			statsStore.push_front(entry);
		}
	}
	//read miss,create a entry
	else
	{
		Insert(row_id);
		miss_counter = 0;
	}
	return isFound;
}	

//update data becase of read miss or write miss
bool Update( ncounter_t row_id , MissType type)
{
	StatsEntry *entry = NULL;
	bool isFound = FindMissCounter( row_id , entry );
	if( isFound )
	{
		switch(type)
		{
				case READ_MISS: 
						entry->MissIncrement( ReadIncrementSteps );
						break;
				case WRITE_MISS:
						entry->MissIncrement( WriteIncrentSteps);
						break;
				default:
						break;
		}
		//update position of entry (make it newer)
		if( entry !=*(statsStore.begin()))
		{
				statsStore.remove(entry);
				statsStore.push_font(entry);
		}
	}
	//update miss (maybe it has been evicted), create a entry
	else
	{
		Insert(row_id);
	}
	return isFound;
}

//clear counter
bool ClearCounter()
{
	
}


//can issue command ? 
bool StatsStoreOp::IsIssuable()
{
		bool issuable = false;
		if( state == IDLE )
		{
			issuable = true;
		}
		return issuable;
}

bool StatsStoreOp::IssueCommand()
{

}

bool StatStoreOp::RequestComplete()
{

}
