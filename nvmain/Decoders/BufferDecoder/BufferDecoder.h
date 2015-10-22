/**
 * created on 2015/5/29
 **/

#ifndef _BUFFER_DECODER_H__
#define _BUFFER_DECODER_H__

#include "src/AddressTranslator.h"
#include "include/Exception.h"

namespace NVM
{
	class BufferDecoder : public AddressTranslator
	{
		public:
			BufferDecoder();
			~BufferDecoder();

			void Translate( uint64_t address, uint64_t *row, uint64_t *col, uint64_t *bank,uint64_t *rank, uint64_t *channel, uint64_t *subarray );

			uint64_t ReverseTranslate( const uint64_t& row, const uint64_t& col,
				const uint64_t& bank, const uint64_t& rank, const uint64_t& channel,
				const uint64_t& subarray );
		    void SetAddrWidth( uint64_t width);
			void SetAddrMask( uint64_t addr_mask);
				
			using AddressTranslator::Translate;

		private:
			uint64_t addr_width;
			uint64_t addr_mask;
			uint64_t max_addr;
	};
};
#endif
