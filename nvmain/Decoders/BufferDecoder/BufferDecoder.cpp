/**
 * created on 2015/5/29
 **/

#include "Decoders/BufferDecoder/BufferDecoder.h"

using namespace NVM;

BufferDecoder::BufferDecoder()
{
	addr_width = 0;
	addr_mask = 0;
	max_addr = 0;
}

BufferDecoder::~BufferDecoder()
{
	
}

void BufferDecoder::Translate(uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,uint64_t *rank, uint64_t *channel, uint64_t *subarray )
{
	if( max_addr || addr_mask )
	{
		//access cache
		address &= addr_mask;
		AddressTranslator::Translate( address , row , col , bank, rank , channel , subarray);
	}
	else
		NVM::Fatal("must set memory bit width first!");
	
}

uint64_t BufferDecoder::ReverseTranslate( const uint64_t& row, const uint64_t& col,const uint64_t& bank,
							const uint64_t& rank, const uint64_t& channel,
							const uint64_t& subarray )
{
	uint64_t tmp_pa = AddressTranslator::ReverseTranslate( row , col , bank , rank , channel , subarray );
	return tmp_pa | ( 1<< addr_width );
}


void BufferDecoder::SetAddrWidth(uint64_t width)
{
	addr_width = width ;
	max_addr = static_cast<uint64_t>(1<<width);
	addr_mask = max_addr - 1 ;
}

void BufferDecoder::SetAddrMask( uint64_t addr_mask)
{
	this->addr_mask = addr_mask;
}
