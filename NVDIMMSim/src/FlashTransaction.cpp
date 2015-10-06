/*********************************************************************************
*  Copyright (c) 2011-2012, Paul Tschirhart
*                             Peter Enns
*                             Jim Stevens
*                             Ishwar Bhati
*                             Mu-Tien Chang
*                             Bruce Jacob
*                             University of Maryland 
*                             pkt3c [at] umd [dot] edu
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

//FlashTransaction.cpp
//
//Class file for transaction object
//	FlashTransaction is considered requests sent from the CPU to 
//	the memory controller (read, write, etc.)...

#include "FlashTransaction.h"

using namespace NVDSim;
using namespace std;

FlashTransaction::FlashTransaction()
{
    transactionType = EMPTY;
}

FlashTransaction::FlashTransaction(TransactionType transType, uint64_t addr, void *dat)
{
	transactionType = transType;
	address = addr;
	data = dat;
}

void FlashTransaction::print()
{
	if(transactionType == DATA_READ)
		{
			PRINT("T [Read] [0x" << hex << address << "]" << dec );
		}
	else if(transactionType == DATA_WRITE)
		{
			PRINT("T [Write] [0x" << hex << address << "] [" << dec << data << "]" );
		}
	else if(transactionType == RETURN_DATA)
		{
			PRINT("T [Data] [0x" << hex << address << "] [" << dec << data << "]" );
		}
}

