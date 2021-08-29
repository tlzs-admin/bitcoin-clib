/*
 * satoshi-datatypes.c
 * 
 * Copyright 2020 Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, 
 * publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 *  in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>

#include "crypto.h"
#include "utils.h"

#include <endian.h>
#include <byteswap.h>
#include "satoshi-types.h"
#include "bitcoin-consensus.h"

/**
 * @ref https://en.bitcoin.it/wiki/Protocol_documentation
 */

/*********************************************************
 * varint
*********************************************************/
size_t varint_calc_size(uint64_t value)
{
	if(value < 0XFD) return 1;
	if(value <= 0xFFFF) return 3;
	if(value <= 0xFFFFFFFF) return 5;
	return 9;
}

static inline void varint_set_data(unsigned char * vch, uint64_t value)
{
	if(value < 0XFD) {
		vch[0] = (unsigned char)value;
	}
	else if(value <= 0xFFFF) {
		vch[0] = 0xFD;
		uint16_t u16 = htole16((uint16_t)value);
		memcpy(&vch[1], &u16, 2);
	}else if (value <= 0xFFFFFFFF)
	{
		uint32_t u32 = htole32((uint32_t)value);
		vch[0] = 0xFE;
		memcpy(&vch[1], &u32, 4);
	}
	else
	{
		uint64_t u64 = htole64(value);
		vch[0] = 0xFF;
		memcpy(&vch[1], &u64, 8);
	}
	return;
}

varint_t * varint_new(uint64_t value)
{
	size_t size = varint_calc_size(value);
	assert(size > 0 && size <= 9);
	
	unsigned char * vch = calloc(1, size);
	varint_set_data(vch, value);
	
	return (varint_t *)vch;
}

void varint_free(varint_t * vint)
{
	free(vint);
}

size_t varint_size(const varint_t * vint)
{
	uint8_t * vch = (uint8_t *)vint;
	if(vch[0] < 0xFD) return 1;
	switch(vch[0]) {
	case 0xFD: return 3;
	case 0xFE: return 5;
	case 0xFF: return 9;
	default: break;
	}
	return -1;
}

varint_t * varint_set(varint_t * vint, uint64_t value)
{
	if(NULL == vint) return varint_new(value);
	
	size_t new_size = varint_calc_size(value);
	assert(new_size > 0 && new_size <= 9);
	
	//~ size_t old_size = varint_size(vint);
	//~ if(old_size != new_size) {
		//~ varint_free(vint);
		//~ return varint_new(value);
	//~ }
	
	varint_set_data(vint->vch, value);
	return vint;
}

uint64_t varint_get(const varint_t * vint)
{
	assert(vint);
	
	uint64_t value = 0;
	const uint8_t * vch = vint->vch;
	
	if(vch[0] < 0xFD) return vch[0];
	
	if(vch[0] == 0xFD) {
		uint16_t u16;
		memcpy(&u16, &vch[1], 2);
		value = le16toh(u16);		
	}else if(vch[0] == 0xFE)
	{
		uint32_t u32;
		memcpy(&u32, &vch[1], 4);
		value = le32toh(u32);
	}else {
		uint64_t u64;
		memcpy(&u64, &vch[1], 8);
		value = le64toh(u64);
	}
	return value;
}



/*********************************************************
 * varstr
*********************************************************/
const varstr_t varstr_empty[1];	// {{ .vch = {0} }

varstr_t * varstr_new(const unsigned char * data, size_t length)
{
	size_t vint_size = varint_calc_size(length);
	assert(vint_size > 0 && vint_size <= 9 && ((vint_size + length) > length));
	
	
	uint8_t * vch = calloc(1, vint_size + length);
	assert(vch);
	varint_set_data(vch, length);
	
	if(data && (length > 0) )
	{
		memcpy(vch + vint_size, data, length);
	}
	return (varstr_t *)vch;
}
void varstr_free(varstr_t * vstr)
{
	if(vstr != varstr_empty) free(vstr);
}

varstr_t * varstr_clone(const varstr_t * vstr)
{
	if(NULL == vstr) return NULL;
	if(vstr == varstr_empty) return (varstr_t *)varstr_empty;
	
	ssize_t size = varstr_size(vstr);
	assert(size > 0);
	
	varstr_t * dst = malloc(size);
	assert(dst);
	memcpy(dst, vstr, size);
	return dst;
}

varstr_t * varstr_resize(varstr_t * vstr, size_t new_len)
{
	if(NULL == vstr || vstr == varstr_empty) return varstr_new(NULL, new_len);
	
	size_t old_len = varstr_length(vstr);
	if(old_len == new_len) return vstr;
	
	varstr_t * new_str = varstr_new(NULL, new_len);
	assert(new_str);
	
	size_t len = (old_len < new_len)?old_len:new_len;
	if(len > 0)
	{
		unsigned char * old_data = vstr->vch + varint_size((varint_t *)vstr);
		unsigned char * new_data = new_str->vch + varint_size((varint_t *)new_str);
		assert(old_data && new_data);
		memcpy(new_data, old_data, len);
	}
	
	varstr_free(vstr);
	return new_str;
}

size_t varstr_size(const varstr_t * vstr)
{
	size_t vint_size = varint_size((varint_t *)vstr);
	size_t data_len = varint_get((varint_t *)vstr);
	
	return  (vint_size + data_len);
}

varstr_t * varstr_set(varstr_t * vstr, const unsigned char * data, size_t length)
{
	if(NULL == vstr || vstr == varstr_empty) return varstr_new(data, length);
	
	unsigned char * p = (unsigned char *)vstr;
	varint_set((varint_t *)p, length);
	p += varint_size((varint_t *)p);
	
	if(data) memcpy(p, data, length);
	return vstr;
}

size_t varstr_get(const varstr_t * vstr, unsigned char ** p_data, size_t buf_size)
{
	assert(vstr);
	ssize_t data_len = varint_get((varint_t *)vstr);
	if(data_len <= 0) return 0;

	if(NULL == p_data) return data_len;	// return buffer size
	
	unsigned char * dst = *p_data;
	if(dst) {
		if(buf_size == 0 || buf_size > data_len) buf_size = data_len;
		
	}else
	{
		dst = malloc(data_len + 1);
		assert(dst);
		*p_data = dst;
		
		buf_size = data_len;
		dst[data_len] = '\0';
	}
	
	const unsigned char * src = vstr->vch + varint_size((varint_t *)vstr);
	assert(src);
	
	memcpy(dst, src, buf_size);		// truncate to buf_size
	return buf_size;
}

/******************************
 * uint256
*****************************/
void uint256_reverse(uint256_t * u256)
{
	uint64_t * u64 = (uint64_t *)u256->val;
	uint64_t tmp;
	
	tmp = bswap_64(u64[0]);
	u64[0] = bswap_64(u64[3]);
	u64[3] = tmp;
	
	tmp = bswap_64(u64[1]);
	u64[1] = bswap_64(u64[2]);
	u64[2] = tmp;
	return;
}

char * uint256_to_string(const uint256_t * u256, int to_little_endian, char ** p_hex)
{
	assert(u256);
	char * hex = p_hex?*p_hex:NULL;
	if(NULL == hex) hex = calloc(1, 33);
	
	uint256_t val = *u256;
	if(to_little_endian) uint256_reverse(&val);
	
	bin2hex(&val, sizeof(val), &hex);
	return hex;
}

ssize_t uint256_from_string(uint256_t * u256, int from_little_endian, const char * hex, ssize_t length)
{
	// big-endian
	assert(hex);
	ssize_t cb = length;
	if(cb <= 0) cb = strlen(hex);
	assert(cb <= 64 && !(cb & 0x01));
	
	unsigned char data[32] = { 0 };
	void * p_data = data;
	cb = hex2bin(hex, cb, &p_data);
	assert(cb >= 0 && cb <= 32);
	
	memset(u256->val, 0, 32);
	if(cb > 0)
	{
		if(from_little_endian)
		{
			memcpy(&u256->val[0], data, cb);
			uint256_reverse(u256);
		}else
		{
			memcpy(&u256->val[32 - cb], data, cb);
		}
	}
	return cb;
}

/************************************************************
 * @ingroup satoshi_tx
 * 
 */
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

static const satoshi_outpoint_t s_coinbase_outpoint[1] = {{
	.prev_hash = {0},
	.index = 0xffffffff
}};

varstr_t * satoshi_txin_get_redeem_scripts(const satoshi_txin_t * txin)
{
	if(NULL == txin || NULL == txin->redeem_scripts) return NULL;
	
	const unsigned char * scripts_data = varstr_getdata_ptr(txin->redeem_scripts);
	const unsigned char * p_end = scripts_data + varstr_length(txin->redeem_scripts);
	
	scripts_data += txin->redeem_scripts_start_pos;
	if(scripts_data >= p_end) return NULL;
	
	return varstr_new(scripts_data, (p_end - scripts_data));
}

ssize_t satoshi_txin_query_redeem_scripts_data(const satoshi_txin_t * txin, const unsigned char ** p_data)
{
	if(NULL == txin || NULL == txin->redeem_scripts) return -1;
	varstr_t * redeem_scripts = txin->redeem_scripts;
	
	const unsigned char * p = varstr_getdata_ptr(redeem_scripts);
	const unsigned char * p_end = p + varstr_length(redeem_scripts);
	
	p += txin->redeem_scripts_start_pos;
	if(p > p_end) return -1;
	
	*p_data = p;
	return p_end - p;
}

varstr_t * satoshi_txin_set_redeem_scripts(satoshi_txin_t * txin, const unsigned char * data, size_t length)
{
	if(NULL == txin) return NULL;
	
	varstr_t * redeem_scripts = varstr_new(data, length);
	varstr_free(txin->redeem_scripts);
	txin->redeem_scripts = redeem_scripts;
	txin->redeem_scripts_start_pos = 0;
	return redeem_scripts;
}


ssize_t satoshi_txin_parse(satoshi_txin_t * txin, ssize_t length, const void * payload)
{
	assert(txin && (length > 0) && payload);

	const unsigned char * p = payload;
	const unsigned char * p_end = p + length;
	
	// step 1. parse outpoint
	if((p + sizeof(struct satoshi_outpoint)) > p_end) {
		message_parser_error_handler("%s", "parse outpoint failed.");
	}
	memcpy(&txin->outpoint, p, sizeof(struct satoshi_outpoint));
	p += sizeof(struct satoshi_outpoint);
	
	txin->is_coinbase = (0 == memcmp(&txin->outpoint, s_coinbase_outpoint, sizeof(txin->outpoint)));
	
	// step2. parse scripts
	ssize_t vstr_size = varstr_size((varstr_t *)p);
	if((p + vstr_size) > p_end) message_parser_error_handler("parse sig_scripts failed: %s", "invalid payload length.");
	
	txin->scripts = varstr_clone((varstr_t *)p);
	assert(txin->scripts && varstr_size(txin->scripts) == vstr_size);
	
	txin->cb_scripts = varstr_length(txin->scripts);
	p += vstr_size;

	// step 3. parse sequence
	if((p + sizeof(uint32_t)) > p_end) {
		message_parser_error_handler("parse sequence failed: %s.", "invalid payload length");
	}
	txin->sequence = *(uint32_t *)p;
	p += sizeof(uint32_t);
	
	return (p - (unsigned char *)payload);
label_error:
	satoshi_txin_cleanup(txin);
	return -1;
}

ssize_t satoshi_txin_serialize(const satoshi_txin_t * txin, unsigned char ** p_data)
{
	assert(txin->scripts);
	ssize_t vstr_size = varstr_size(txin->scripts);
	assert(vstr_size > 0);
	
	ssize_t cb_payload 							// payload length
		= sizeof(struct satoshi_outpoint) 
		+ vstr_size
		+ sizeof(uint32_t);				// sizeof(sequence)

	if(NULL == p_data) return cb_payload;
	
	assert(cb_payload > 0);
	unsigned char * payload = *p_data;
	if(NULL == payload) {
		payload = malloc(cb_payload);
		assert(payload);
		*p_data = payload;
	}

	unsigned char * p = payload;
	unsigned char * p_end = p + cb_payload;
	
	// step 1. write outpoint
	memcpy(p, &txin->outpoint, sizeof(struct satoshi_outpoint));
	p += sizeof(struct satoshi_outpoint);
	
	// step 2. write sig_scripts
	memcpy(p, txin->scripts, vstr_size);
	p += vstr_size;
		
	// step 3. write sequence
	assert((p + sizeof(uint32_t)) <= p_end);
	*(uint32_t *)p = txin->sequence;
	p += sizeof(uint32_t);
	
	assert(p == p_end);
	return cb_payload;
}

void satoshi_txin_cleanup(satoshi_txin_t * txin)
{
	if(txin) {
		if(txin->scripts)
		{
			varstr_free(txin->scripts);
			txin->scripts = NULL;
		}
		
		if(txin->signatures)
		{
			for(ssize_t i = 0; i < txin->num_signatures; ++i)
			{
				varstr_free(txin->signatures[i]);
			}
			free(txin->signatures);
		}
		
		if(txin->redeem_scripts)
		{
			varstr_free(txin->redeem_scripts);
			txin->redeem_scripts = NULL;
		}
	}
	return;
}

//~ typedef struct satoshi_txout
//~ {
	//~ int64_t value;
	//~ ssize_t cb_script;
	//~ unsigned char * scripts;
//~ }satoshi_txout_t;
ssize_t satoshi_txout_parse(satoshi_txout_t * txout, ssize_t length, const void * payload)
{
	assert(txout && (length > 0) && payload);
	const unsigned char * p = (unsigned char *) payload;
	const unsigned char * p_end = p + length;
	
	// parse value
	if(length < sizeof(int64_t)) message_parser_error_handler("parse value failed: %s", "invalid payload length.");
	txout->value = *(int64_t *)p;
	p += sizeof(int64_t); 
	
	if((p + 1) > p_end) message_parser_error_handler("parse pk_script failed: %s", "no varint data.");
	
	// parse pk_scripts
	ssize_t vstr_size = varstr_size((varstr_t *)p);
	if((p + vstr_size) > p_end) message_parser_error_handler("%s", "invalid varstr size or payload length.");
	
	txout->scripts = varstr_clone((varstr_t *)p);
	assert(txout->scripts && varstr_size(txout->scripts) == vstr_size);
	p += vstr_size;
	
	/**
	 * check Witness flags: 
	 * A scriptPubKey (or redeemScript as defined in BIP16/P2SH) that 
	 * consists of a 1-byte push opcode (for 0 to 16) 
	 * followed by a data push between 2 and 40 bytes gets a new special meaning. 
	 * The value of the first push is called the "version byte". 
	 * The following byte vector pushed is called the "witness program".
	*/
	unsigned char * scripts_data = varstr_getdata_ptr(txout->scripts);
	txout->flags = (scripts_data[0] <= 16)?satoshi_txout_type_segwit:satoshi_txout_type_legacy;
	
	return (p - (unsigned char *)payload);
label_error:
	satoshi_txout_cleanup(txout);
	return -1;
}

ssize_t satoshi_txout_serialize(const satoshi_txout_t * txout, unsigned char ** p_data)
{
	ssize_t	scripts_size = varstr_size(txout->scripts);
	ssize_t cb_payload = sizeof(int64_t) + scripts_size;
	
	if(NULL == p_data) return cb_payload;
	
	unsigned char * payload = *p_data;
	if(NULL == payload)
	{
		payload = malloc(cb_payload);
		assert(payload);
		*p_data = payload;
	}
	
	unsigned char * p_end = payload + cb_payload;
	// write value
	assert((payload + sizeof(int64_t)) < p_end);
	*(int64_t *)payload = txout->value;
	payload += sizeof(int64_t);
	
	// write pk_scripts 
	memcpy(payload, txout->scripts, scripts_size);
	payload += scripts_size;
	
	assert(payload == p_end);
	return cb_payload;
}
void satoshi_txout_cleanup(satoshi_txout_t * txout)
{
	if(txout && txout->scripts)
	{
		varstr_free(txout->scripts);
		txout->scripts = NULL;
	}
}




#if defined(_TEST_SATOSHI_TYPES) && defined(_STAND_ALONE)
int main(int argc, char ** argv)
{
	
	return 0;
}
#endif
