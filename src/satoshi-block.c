/*
 * satoshi-block.c
 * 
 * Copyright 2021 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitcoin-consensus.h"
#include "satoshi-types.h"

#include "utils.h"


#ifdef _DEBUG
#define message_parser_error_handler(fmt, ...) do { \
		fprintf(stderr, "\e31m[ERROR]::%s@%d::%s(): " fmt "\e[39m" "\n", \
			__FILE__, __LINE__, __FUNCTION__,	\
			##__VA_ARGS__);						\
		abort();								\
		goto label_error;						\
	} while(0)
#else
#define message_parser_error_handler(fmt, ...) do { \
		fprintf(stderr, "\e31m[ERROR]::%s@%d::%s(): " fmt "\e[39m" "\n", \
			__FILE__, __LINE__, __FUNCTION__,	\
			##__VA_ARGS__);						\
		goto label_error;						\
	} while(0)
#endif

int satoshi_block_header_verify(const struct satoshi_block_header * hdr, uint256_t * p_hash)
{
	/**
	 * Calculate block_hash and check if it matches the current difficulty.
	 * The hash must be less than or equal to the target which set in the block.hdr (hdr.bits).
	 * 
	 * 'hdr.bits' should be checked when building the block_chain,
	 *  and 'hdr.bits' must match the global difficulty which be recalculated every 2016 blocks.
	 */
	 
	uint256_t hash[1];
	if(NULL == p_hash) p_hash = hash;
	
	hash256(hdr, sizeof(struct satoshi_block_header), (uint8_t *)p_hash);
	int compare_diff = uint256_compare_with_compact(hash, (compact_uint256_t *)&hdr->bits);
	//~ assert(compare_diff <= 0);
	
	if(compare_diff > 0) return -1;
	
	///@ todo:  calc_time_diff(hdr->time, get_network_mean_time()); 
	
	return 0;
}

ssize_t satoshi_block_parse(satoshi_block_t * block, ssize_t length, const void * payload)
{
	assert(block && (length > 0) && payload);
	const unsigned char * p = payload;
	const unsigned char * p_end = p + length;
	
	satoshi_block_cleanup(block);	// clear old data
		
	// parse block header
	if((p + sizeof(struct satoshi_block_header)) > p_end) {
		message_parser_error_handler("parse block header failed: %s", "invalid payload length");
	}
	memcpy(&block->hdr, p, sizeof(struct satoshi_block_header));
	p += sizeof(struct satoshi_block_header);
	
	int rc = satoshi_block_header_verify(&block->hdr, &block->hash);
	if(rc != 0) {
		message_parser_error_handler("Difficulty invalid (greater than 0x%.8d).", block->hdr.bits);
	}
	
	if(length == sizeof(struct satoshi_block_header)) // parse block header only
	{
		return sizeof(struct satoshi_block_header);
	}
	
	// parse txn_count
	ssize_t vint_size = varint_size((varint_t *)p);
	if((p + vint_size) > p_end) {
		message_parser_error_handler("parse txn_count failed: %s", "invalid payload length");
	}
	
	block->txn_count = varint_get((varint_t *)p);
	p += vint_size;
	
	if(block->txn_count <= 0) {
		message_parser_error_handler("invalid txn_count: %ld", (long)block->txn_count);
	}
	
	satoshi_tx_t * txns = calloc(block->txn_count, sizeof(*txns));
	assert(txns);
	block->txns = txns;
	
	/**
	 * Use merkle_tree to verify all transactions in the block.
	 * The calculated merkle_root by all txids MUST be equal to 'hdr.merkle_root'
	 */
	uint256_merkle_tree_t * mtree = uint256_merkle_tree_new(block->txn_count, block);
	assert(mtree);
	
	for(ssize_t i = 0; i < block->txn_count; ++i)
	{
		if(p >= p_end) {
			message_parser_error_handler("parse tx[%d] failed: invalid payload length", (int)i);
		}
		ssize_t tx_size = satoshi_tx_parse(&block->txns[i], (p_end - p), p);
		if(tx_size <= 0) {
			message_parser_error_handler("parse tx[%d] failed: invalid payload data", (int)i);
		}
		p += tx_size;
		
		mtree->add(mtree, 1, block->txns[i].txid);
	}
	mtree->recalc(mtree, 0, -1);
	uint256_t merkle_root = mtree->merkle_root;
	uint256_merkle_tree_free(mtree);
	mtree = NULL;
	
	// verify merkle root
	if(0 != memcmp(&block->hdr.merkle_root, &merkle_root, 32))	// not equal
	{
		message_parser_error_handler("Verification of merkle-root failed. There's one or more invalid transactions in the block.");
	}
	
	assert(p <= p_end);
	ssize_t block_size = (p - (unsigned char *)payload);
	assert(block_size <= MAX_BLOCK_SERIALIZED_SIZE);
	
	return block_size;
	
label_error:
	satoshi_block_cleanup(block);
	return -1;
}
void satoshi_block_cleanup(satoshi_block_t * block)
{
	if(NULL == block) return;
	if(block->txns)
	{
		for(ssize_t i = 0; i < block->txn_count; ++i)
		{
			satoshi_tx_cleanup(&block->txns[i]);
		}
		free(block->txns);
		block->txns = NULL;
		block->txn_count = 0;
	}
	return;
}

ssize_t satoshi_block_serialize(const satoshi_block_t * block, unsigned char ** p_data)
{
	ssize_t vint_size = 0;
	ssize_t tx_size = 0;
	
	if(block->txn_count > 0) // full block
	{
		vint_size = varint_calc_size(block->txn_count);
		for(ssize_t i = 0; i < block->txn_count; ++i)
		{
			ssize_t cb = satoshi_tx_serialize(&block->txns[i], NULL);
			assert(cb > 0);
			tx_size += cb;
		}
	}

	ssize_t block_size = sizeof(struct satoshi_block_header)
		+ vint_size
		+ tx_size;
	assert(block_size <= MAX_BLOCK_SERIALIZED_SIZE);
	if(NULL == p_data) return block_size;
	
	unsigned char * payload = *p_data;
	if(NULL == payload)
	{
		payload = malloc(block_size);
		assert(payload);
		*p_data = payload;
	}
	
	unsigned char * p = payload;
	unsigned char * p_end = p + block_size;
	
	// block header
	assert((p + sizeof(struct satoshi_block_header)) <= p_end);
	memcpy(p, &block->hdr, sizeof(struct satoshi_block_header));
	p += sizeof(struct satoshi_block_header);
	if(p == p_end) return sizeof(struct satoshi_block_header);	// block header only
	
	// txns
	assert((p + vint_size) <= p_end);
	if(block->txn_count > 0)
	{
		varint_set((varint_t *)p, block->txn_count);
		p += vint_size;
		
		for(ssize_t i = 0; i < block->txn_count; ++i)
		{
			assert(p < p_end);
			ssize_t cb = satoshi_tx_serialize(&block->txns[i], &p);
			assert(cb > 0);
			p += cb;
		}
	}
	assert(p <= p_end);
	assert((p - payload) == block_size);
	return block_size;
}

void satoshi_block_header_dump(const struct satoshi_block_header * hdr)
{
	printf("================= %s(%p) ======================\n", __FUNCTION__, hdr);
	printf("version: 0x%.8x\n", hdr->version);
	dump_line("prev_hash: ", hdr->prev_hash, 32);
	dump_line("merkel_root: ", hdr->merkle_root, 32);
	printf("timestamp: %ld\n", (long)hdr->timestamp);
	printf("bits: 0x%.8x\n", hdr->bits);
	printf("nonce: 0x%.8x\n", hdr->nonce);
	return;
}

void satoshi_block_dump(const satoshi_block_t * block)
{
	printf("================= %s(%p) ======================\n", __FUNCTION__, block);
	satoshi_block_header_dump(&block->hdr);
	printf("txn_count: %d\n", (int)block->txn_count);
	for(int i = 0; i < block->txn_count; ++i) {
		satoshi_tx_t * tx = &block->txns[i];
		printf("tx[%d]: hash=", i);
		dump(tx->txid, 32);
		printf("\n");
		satoshi_tx_dump(tx);
	}
}
