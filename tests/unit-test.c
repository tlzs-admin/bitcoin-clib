/*
 * unit-test.c
 * 
 * Copyright 2020 Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "satoshi-types.h"

void test_uint256(void);
void test_parse_blocks(void);

int main(int argc, char **argv)
{
	test_uint256();
	test_parse_blocks();
	return 0;
}

#define dump_line(prefix, data, length) do {							\
		printf(prefix); dump(data, length); printf("\e[39m\n");	\
	} while(0)

void test_uint256(void)
{
	uint256_t hash[1];
	memset(hash, 0, sizeof(hash));
	uint256_from_string(hash, 1, "01020304", -1);
	dump_line("\e[33m[little endian::01020304]", hash, 32);
	
	memset(hash, 0, sizeof(hash));
	uint256_from_string(hash, 0, "01020304", -1);
	dump_line("\e[33m[   big endian::01020304]", hash, 32);
	return;
} 

void test_parse_blocks(void)
{
	// TEST1: Parse Legacy Block
	// block_height: 100000 
	// block_hash  : LE"000000000003ba27aa200b1cecaad478d2b00432346c3f1f3986da1afd33e506"
	const char * hex = 
	"01000000"
	"50120119172a610421a6c3011dd330d9df07b63616c2cc1f1cd0020000000000"
	"6657a9252aacd5c0b2940996ecff952228c3067cc38d4885efb5a4ac4247e9f3"	
	"37221b4d"
	"4c86041b"
	"0f2b5710"
	"04"	// txns_count = 4
		/* txn[0] (coinbase tx)*/
		"01000000"	//version
		"01"		// txins_count
			"0000000000000000000000000000000000000000000000000000000000000000ffffffff"	// outpoint
			"08044c86041b020602"	// sig_scipts
			"ffffffff"				// sequence
		"01"		// txouts_count
			/* txouts[0] */
			"00f2052a01000000"	//value
			"43"	// pk_scripts
				"41041b0e8c2567c12536aa13357b79a073dc4444acb83c4ec7a0e2f99dd7457516c5817242da796924ca4e99947d087fedf9ce467cb9f7c6287078f801df276fdf84ac"
			"00000000"	// lock_time
			
		/* txn[1] */	
		"01000000"
		"01"	// txins_count
			"032e38e9c0a84c6046d687d10556dcacc41d275ec55fc00779ac88fdf357a18700000000"
			"8c"	// scripts length
				"49"	// signatures length
					"3046"	// signature
						"022100c352d3dd993a981beba4a63ad15c209275ca9470abfcd57da93b58e4eb5dce82"
						"022100840792bc1f456062819f15d33ee7055cf7b5ee1af1ebcc6028d9cdb1c3af7748"
					"01"	// hash_type
				"41"	// pubkey
					"04f46db5e9d61a9dc27b8d64ad23e7383a4e6ca164593c2527c038c0857eb67ee8e825dca65046b82c9331586c82e0fd1f633f25f87c161bc6f8a630121df2b3d3"
			"ffffffff"	// sequence
		"02"	// txouts_count
			/* txouts[0] */
			"00e3232100000000"	// value
			"19"	// pk_scripts
				"76a914c398efa9c392ba6013c5e04ee729755ef7f58b3288ac"
			/* txouts[1] */
			"000fe20801000000"	// value
			"19"	// pk_scripts
				"76a914948c765a6914d43f2a7ac177da2c2f6b52de3d7c88ac"
			"00000000" // lock_time
			
		/* txn[2] */
		"01000000"
		"01"
			"c33ebff2a709f13d9f9a7569ab16a32786af7d7e2de09265e41c61d078294ecf01000000"
			"8a"
				"47"
					"3044"
						"0220032d30df5ee6f57fa46cddb5eb8d0d9fe8de6b342d27942ae90a3231e0ba333e"
						"02203deee8060fdc70230a7f5b4ad7d7bc3e628cbe219a886b84269eaeb81e26b4fe"
					"01"
				"41"
					"04ae31c31bf91278d99b8377a35bbce5b27d9fff15456839e919453fc7b3f721f0ba403ff96c9deeb680e5fd341c0fc3a7b90da4631ee39560639db462e9cb850"
			"fffffffff"
		"02"
			"40420f0000000000"
			"1976a914b0dcbf97eabf4404e31d952477ce822dadbe7e1088ac"
			"c060d21100000000"
			"1976a9146b1281eec25ab4e1e0793ff4e08ab1abb3409cd988ac"
		"00000000"
		
		/* txn[3] */
		"01000000"
		"01"
			"0b6072b386d4a773235237f64c1126ac3b240c84b917a3909ba1c43ded5f51f400000000"
			"8c"
				"49"
					"3046"
						"022100bb1ad26df930a51cce110cf44f7a48c3c561fd977500b1ae5d6b6fd13d0b3f4a"
						"022100c5b42951acedff14abba2736fd574bdb465f3e6f8da12e2c5303954aca7f78f3"
					"01"
				"41"
					"04a7135bfe824c97ecc01ec7d7e336185c81e2aa2c41ab175407c09484ce9694b44953fcb751206564a9c24dd094d42fdbfdd5aad3e063ce6af4cfaaea4ea14fbb"
			"ffffffff"
		"01"
			"40420f0000000000"
			"1976a91439aa3d569e06a1d7926dc4be1193c99bf2eb9ee088ac"
		"00000000";
	
	unsigned char * block_data = NULL;
	ssize_t cb_block = hex2bin(hex, strlen(hex), (void **)&block_data);
	assert(cb_block > 0 && block_data);
	
	satoshi_block_t block[1];
	memset(block, 0, sizeof(block));
	
	ssize_t cb = satoshi_block_parse(block, cb_block, block_data);
	assert(cb == cb_block);
	
	unsigned char * output = NULL;
	cb = satoshi_block_serialize(block, &output);
	assert(cb == cb_block);
	
	char * output_hex = calloc(cb * 2 + 1, 1);
	assert(output_hex);
	
	cb = bin2hex(output, cb_block, &output_hex);
	assert(cb == (cb_block * 2));
	
	assert(0 == strcasecmp(hex, output_hex));
	
	dump_line("block_hash(big-endian)", &block->hash, 32); 
	
	free(block_data);
	free(output);
	free(output_hex);
	
	satoshi_block_cleanup(block);
	
	return;
}