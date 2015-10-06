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

#ifndef NVCHANNELPACKET_H
#define NVCHANNELPACKET_H
//ChannelPacket.h
//
//Header file for bus packet object
//

#include "FlashConfiguration.h"

namespace NVDSim
{
	enum ChannelPacketType
	{
		READ,
		GC_READ,
		WRITE,
		GC_WRITE,
		ERASE,
		DATA,
		FAST_WRITE
	};

	class ChannelPacket
	{
	public:
		//Fields
		ChannelPacketType busPacketType;

		uint64_t page;
		uint64_t block;
		uint64_t plane;
		uint64_t die;
		uint64_t package;
		uint64_t virtualAddress;
		uint64_t physicalAddress;
		void *data;

		//Functions
		ChannelPacket(ChannelPacketType packtype, uint64_t virtualAddr, uint64_t physicalAddr, uint64_t page, 
			      uint64_t block, uint64_t plane, uint64_t die, uint64_t package, void *dat);
		ChannelPacket();

		//void print();
		void print(uint64_t currentClockCycle);
		static void printData(const void *data);
	};
}

#endif

