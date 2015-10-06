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

//ChannelPacket.cpp
//Class file for ChannelPacket object

#include "ChannelPacket.h"

using namespace NVDSim;
using namespace std;

ChannelPacket::ChannelPacket(ChannelPacketType packtype, uint64_t virtualAddr, uint64_t physicalAddr, uint64_t page_num, 
			     uint64_t block_num, uint64_t plane_num, uint64_t die_num, uint64_t package_num, void *dat)
{
        virtualAddress = virtualAddr;
	physicalAddress = physicalAddr;
	busPacketType = packtype;
	data = dat;
	page = page_num;
	block = block_num;
	plane = plane_num;
	die = die_num;
	package = package_num;
}

ChannelPacket::ChannelPacket() {}

void ChannelPacket::print(uint64_t currentClockCycle){
	if (this == NULL)
		return;

	PRINT("Cycle: "<<currentClockCycle<<" Type: " << busPacketType << " addr: "<<physicalAddress<<" package: "<<package<<" die: "<<die<<" plane: "<<
			plane<<" block: "<<block<<" page: "<<page<<" data: "<<data);
}

void ChannelPacket::printData(const void *data) 
{
	if (data == NULL) 
	{
		PRINTN("NO DATA");
		return;
	}
	PRINTN("'" << hex);
	for (int i=0; i < 4; i++) 
	{
		PRINTN(((uint64_t *)data)[i]);
	}
	PRINTN("'" << dec);
}
