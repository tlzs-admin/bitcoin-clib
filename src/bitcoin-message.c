/*
 * bitcoin-message.c
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
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>

#include "bitcoin-message.h"
#include "utils.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "unsupport byte-order"
#endif

typedef void * (* parse_payload_fn)(void * object, const void * payload, size_t length);
typedef void  (* cleanup_message_fn)(void * msg);

static const char * s_sz_message_types[bitcoin_message_types_count] = 
{
	[bitcoin_message_type_unknown] = "unknown",
	[bitcoin_message_type_version] = "version",
	[bitcoin_message_type_verack] = "verack",
	[bitcoin_message_type_addr] = "addr",
	[bitcoin_message_type_inv] = "inv",
	[bitcoin_message_type_getdata] = "getdata",
	[bitcoin_message_type_notfound] = "notefound",
	[bitcoin_message_type_getblocks] = "getblocks",
	[bitcoin_message_type_getheaders] = "getheaders",
	[bitcoin_message_type_tx] = "tx",
	[bitcoin_message_type_block] = "block",
	[bitcoin_message_type_headers] = "headers",
	[bitcoin_message_type_getaddr] = "getaddr",
	[bitcoin_message_type_mempool] = "mempool",
	[bitcoin_message_type_checkorder] = "checkorder",
	[bitcoin_message_type_submitorder] = "submitorder",
	[bitcoin_message_type_reply] = "reply",
	[bitcoin_message_type_ping] = "ping", 
	[bitcoin_message_type_pong] = "pong",
	[bitcoin_message_type_reject] = "reject",
	[bitcoin_message_type_filterload] = "filterload",
	[bitcoin_message_type_filteradd] = "filteradd",
	[bitcoin_message_type_filterclear] = "filterclear",
	[bitcoin_message_type_merkleblock] = "merkle_block",
	[bitcoin_message_type_alert] = "alert",
	[bitcoin_message_type_sendheaders] = "sendheaders",
	[bitcoin_message_type_feefilter] = "feefilter",
	[bitcoin_message_type_sendcmpct] = "sendcmpct",
	[bitcoin_message_type_cmpctblock] = "cmpctblock",
	[bitcoin_message_type_getblocktxn] = "getblocktxn",
	[bitcoin_message_type_blocktxn] = "blocktxn",
	//
};
const char * bitcoin_message_type_to_string(enum bitcoin_message_type msg_type)
{
	if(msg_type < 0 || msg_type > bitcoin_message_types_count) return NULL;
	return s_sz_message_types[msg_type];
}
enum bitcoin_message_type bitcoin_message_type_from_string(const char * command)
{
	for(int i = 1; i < bitcoin_message_types_count; ++i) {
		if(strncmp(command, s_sz_message_types[i], 12) == 0) return i;
	}
	return bitcoin_message_type_unknown;
}

cleanup_message_fn s_cleanup_message_func[bitcoin_message_types_count] = {
	[bitcoin_message_type_unknown] =     (cleanup_message_fn)NULL,
	[bitcoin_message_type_version] =     (cleanup_message_fn)bitcoin_message_version_cleanup,
	[bitcoin_message_type_verack] =      NULL,
	[bitcoin_message_type_addr] =        (cleanup_message_fn)bitcoin_message_addr_cleanup,
	[bitcoin_message_type_inv] =         (cleanup_message_fn)bitcoin_message_inv_cleanup,
	[bitcoin_message_type_getdata] =     (cleanup_message_fn)bitcoin_message_getdata_cleanup,
	[bitcoin_message_type_notfound] =    (cleanup_message_fn)bitcoin_message_notfound_cleanup,
	[bitcoin_message_type_getblocks] =   (cleanup_message_fn)bitcoin_message_getblocks_cleanup,
	[bitcoin_message_type_getheaders] =  (cleanup_message_fn)bitcoin_message_getheaders_cleanup,
	[bitcoin_message_type_tx] =          (cleanup_message_fn)bitcoin_message_tx_cleanup,
	[bitcoin_message_type_block] =       (cleanup_message_fn)bitcoin_message_block_cleanup,
	[bitcoin_message_type_headers] =     (cleanup_message_fn)bitcoin_message_block_headers_cleanup,
	[bitcoin_message_type_getaddr] =     NULL,
	[bitcoin_message_type_mempool] =     NULL,
	[bitcoin_message_type_checkorder] =  NULL,
	[bitcoin_message_type_submitorder] = NULL,
	[bitcoin_message_type_reply] =       NULL,
	[bitcoin_message_type_ping] =        NULL,
	[bitcoin_message_type_pong] =        NULL,
	[bitcoin_message_type_reject] =      NULL,
	[bitcoin_message_type_filterload] =  NULL,
	[bitcoin_message_type_filteradd] =   NULL,
	[bitcoin_message_type_filterclear] = NULL,
	[bitcoin_message_type_merkleblock] = NULL,
	[bitcoin_message_type_alert] =       NULL,
	[bitcoin_message_type_sendheaders] = NULL,
	[bitcoin_message_type_feefilter] =   NULL,
	[bitcoin_message_type_sendcmpct] =   NULL,
	[bitcoin_message_type_cmpctblock] =  NULL,
	[bitcoin_message_type_getblocktxn] = NULL,
	[bitcoin_message_type_blocktxn] =    NULL,
};
static inline cleanup_message_fn get_cleanup_function(enum bitcoin_message_type type)
{
	if(type >= 0 && type < bitcoin_message_types_count) return s_cleanup_message_func[type];
	return NULL;
}

static parse_payload_fn s_payload_parsers[bitcoin_message_types_count] = 
{
	[bitcoin_message_type_unknown] =     NULL,
	[bitcoin_message_type_version] =     (parse_payload_fn)bitcoin_message_version_parse,
	[bitcoin_message_type_verack] =      NULL,
	[bitcoin_message_type_addr] =        (parse_payload_fn)bitcoin_message_addr_parse,
	[bitcoin_message_type_inv] =         (parse_payload_fn)bitcoin_message_inv_parse,
	[bitcoin_message_type_getdata] =     (parse_payload_fn)bitcoin_message_getdata_parse,
	[bitcoin_message_type_notfound] =    (parse_payload_fn)bitcoin_message_notfound_parse,
	[bitcoin_message_type_getblocks] =   (parse_payload_fn)bitcoin_message_getblocks_parse,
	[bitcoin_message_type_getheaders] =  (parse_payload_fn)bitcoin_message_getheaders_parse,
	[bitcoin_message_type_tx] =          (parse_payload_fn)bitcoin_message_tx_parse,
	[bitcoin_message_type_block] =       (parse_payload_fn)bitcoin_message_block_parse,
	[bitcoin_message_type_headers] =     (parse_payload_fn)bitcoin_message_block_headers_parse,
	[bitcoin_message_type_getaddr] =     NULL,
	[bitcoin_message_type_mempool] =     NULL,
	[bitcoin_message_type_checkorder] =  NULL,
	[bitcoin_message_type_submitorder] = NULL,
	[bitcoin_message_type_reply] =       NULL,
	[bitcoin_message_type_ping] =        NULL,
	[bitcoin_message_type_pong] =        NULL,
	[bitcoin_message_type_reject] =      NULL,
	[bitcoin_message_type_filterload] =  NULL,
	[bitcoin_message_type_filteradd] =   NULL,
	[bitcoin_message_type_filterclear] = NULL,
	[bitcoin_message_type_merkleblock] = NULL,
	[bitcoin_message_type_alert] =       NULL,
	[bitcoin_message_type_sendheaders] = NULL,
	[bitcoin_message_type_feefilter] =   NULL,
	[bitcoin_message_type_sendcmpct] =   NULL,
	[bitcoin_message_type_cmpctblock] =  NULL,
	[bitcoin_message_type_getblocktxn] = NULL,
	[bitcoin_message_type_blocktxn] =    NULL,
};
static inline parse_payload_fn get_payload_parser(enum bitcoin_message_type type)
{
	if(type >= 0 && type < bitcoin_message_types_count) return s_payload_parsers[type];
	return NULL;
}

void bitcoin_message_clear(bitcoin_message_t * msg)
{
	if(NULL == msg) return;
	if(msg->msg_object) {
		cleanup_message_fn cleanup = get_cleanup_function(msg->msg_type);
		if(cleanup) cleanup(msg->msg_object);
		free(msg->msg_object);
		msg->msg_object = NULL;
	}
	
	if(msg->msg_data) {
		free(msg->msg_data);
		msg->msg_data = NULL;
	}
	memset(msg->hdr, 0, sizeof(msg->hdr));
	
	msg->msg_type = bitcoin_message_type_unknown;
	return;
}

int bitcoin_message_parse(bitcoin_message_t * msg, const struct bitcoin_message_header * hdr, const void * payload, size_t length)
{
	assert(msg && hdr);
	bitcoin_message_clear(msg);	// clear old data
	
	if(0 == length) length = hdr->length;
	assert(length >= hdr->length);
	if(NULL == payload) payload = hdr->payload;
	
	fprintf(stderr, "%s(command=%s, length=%u)\n", __FUNCTION__, hdr->command, hdr->length);
	
	enum bitcoin_message_type type = bitcoin_message_type_from_string(hdr->command);
	if(type == bitcoin_message_type_unknown) return -1;
	
	// verify checksum
	unsigned char hash[32] = { 0 };
	hash256(payload, hdr->length, hash);
	if(memcmp(hash, &hdr->checksum, 4) != 0) return -1;	// invalid checksum
	
	// copy data
	size_t size = sizeof(*hdr) + hdr->length;
	struct bitcoin_message_header * msg_data = malloc(size);
	assert(msg_data);
	if(payload == hdr->payload) memcpy(msg_data, hdr, size);
	else {
		memcpy(msg_data, hdr, sizeof(*hdr));
		if(hdr->length > 0) memcpy(msg_data->payload, payload, hdr->length);
	}
	msg->msg_data = msg_data;
	msg->msg_type = type;
	
	if(msg_data->length > 0) {
		parse_payload_fn parser = get_payload_parser(type);
		if(parser) {
			msg->msg_object = parser(NULL, msg_data->payload, msg_data->length);
			if(NULL == msg->msg_object) return 0;
		}
	}
	return 0;
}

void bitcoin_message_cleanup(bitcoin_message_t * msg)
{
	if(NULL == msg) return;
	if(NULL == msg->clear) msg->clear = bitcoin_message_clear;
	
	msg->clear(msg);
	return;
}

int bitcoin_message_attach(struct bitcoin_message * msg, uint32_t network_magic, enum bitcoin_message_type type, void * msg_object)
{
	assert(msg);
	if(type <= bitcoin_message_type_unknown || type >= bitcoin_message_types_count) return -1;
	
	msg->clear(msg);
	
	msg->msg_type = type;
	msg->msg_object = msg_object;
	msg->hdr->magic = network_magic;
	
	ssize_t cb_payload = bitcoin_message_serialize(msg, NULL);
	if(cb_payload <= 0) return -1;
	return 0;
}

enum bitcoin_message_type bitcoin_message_detech(struct bitcoin_message * msg, void ** p_object)
{
	assert(msg);
	if(NULL == msg->clear) msg->clear = bitcoin_message_clear;
	
	void * msg_object = msg->msg_object;
	enum bitcoin_message_type type = msg->msg_type;
	
	msg->msg_object = NULL;
	msg->clear(msg);
	
	if(p_object) {
		*p_object = msg_object;
		return type;
	}
	
	// auto cleanup
	if(msg_object) {
		cleanup_message_fn cleanup = get_cleanup_function(type);
		if(cleanup) cleanup(msg_object);
		free(msg_object);
		msg_object = NULL;
	}
	return type;
}

bitcoin_message_t * bitcoin_message_new(bitcoin_message_t * msg, uint32_t magic, enum bitcoin_message_type type, void * user_data)
{
	if(NULL == msg) { msg = calloc(1, sizeof(*msg)); assert(msg); }
	else memset(msg, 0, sizeof(*msg));
	
	msg->user_data = user_data;
	msg->parse = bitcoin_message_parse;
	msg->attach = bitcoin_message_attach;
	msg->detach = bitcoin_message_detech;
	msg->clear = bitcoin_message_clear;
	
	msg->msg_type = type;
	if(type <= bitcoin_message_type_unknown || type >= bitcoin_message_types_count) return msg; // an empty context
	
	const char * sz_type = bitcoin_message_type_to_string(type);
	assert(sz_type);
	
	struct bitcoin_message_header * hdr = msg->hdr;
	hdr->magic = magic;
	strncpy(hdr->command, sz_type, sizeof(hdr->command));
	
	void * msg_object = NULL;
	switch(type) {
	case bitcoin_message_type_version:    
		msg_object = calloc(1, sizeof(struct bitcoin_message_version)); 
		break;
	case bitcoin_message_type_verack:     
		break;
	case bitcoin_message_type_addr:       
		msg_object = calloc(1, sizeof(struct bitcoin_message_addr)); 
		break;
	case bitcoin_message_type_inv:        
		msg_object = calloc(1, sizeof(struct bitcoin_message_inv)); 
		break;
	case bitcoin_message_type_getdata:    
		msg_object = calloc(1, sizeof(struct bitcoin_message_getdata)); 
		break;
	case bitcoin_message_type_notfound:   
		msg_object = calloc(1, sizeof(struct bitcoin_message_notfound)); 
		break;
	case bitcoin_message_type_getblocks:  
		msg_object = calloc(1, sizeof(struct bitcoin_message_getblocks)); 
		break;
	case bitcoin_message_type_getheaders: 
		msg_object = calloc(1, sizeof(struct bitcoin_message_getheaders)); 
		break;
	case bitcoin_message_type_tx:         
		msg_object = calloc(1, sizeof(bitcoin_message_tx_t)); 
		break;
	case bitcoin_message_type_block:      
		msg_object = calloc(1, sizeof(bitcoin_message_block_t)); 
		break;
	case bitcoin_message_type_headers:    
		msg_object = calloc(1, sizeof(struct bitcoin_message_block_headers)); 
		break;
	
	case bitcoin_message_type_getaddr:    
	case bitcoin_message_type_mempool:
	case bitcoin_message_type_checkorder:
	case bitcoin_message_type_submitorder:
	case bitcoin_message_type_reply:
	case bitcoin_message_type_ping:
	case bitcoin_message_type_pong:
	case bitcoin_message_type_reject:
	case bitcoin_message_type_filterload:
	case bitcoin_message_type_filteradd:
	case bitcoin_message_type_filterclear:
	case bitcoin_message_type_merkleblock:
	case bitcoin_message_type_alert:
	case bitcoin_message_type_sendheaders:
	case bitcoin_message_type_feefilter:
	case bitcoin_message_type_sendcmpct:
	case bitcoin_message_type_cmpctblock:
	case bitcoin_message_type_getblocktxn:
	case bitcoin_message_type_blocktxn:
	default:
		break;
	}
	
	msg->msg_object = msg_object;
	return msg;
}

static ssize_t message_object_serialize(enum bitcoin_message_type type, void * msg_object, unsigned char ** p_payload)
{
	ssize_t cb_payload = 0;
	switch(type) {
	case bitcoin_message_type_version:    
		cb_payload = bitcoin_message_version_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_verack:     
		break;
	case bitcoin_message_type_addr:       
		cb_payload = bitcoin_message_addr_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_inv:        
		cb_payload = bitcoin_message_inv_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_getdata:    
		cb_payload = bitcoin_message_getdata_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_notfound:   
		cb_payload = bitcoin_message_notfound_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_getblocks:  
		cb_payload = bitcoin_message_getblocks_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_getheaders: 
		cb_payload = bitcoin_message_getheaders_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_tx:         
		cb_payload = bitcoin_message_tx_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_block:      
		cb_payload = bitcoin_message_block_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_headers:    
		cb_payload = bitcoin_message_block_headers_serialize(msg_object, p_payload);
		break;
	case bitcoin_message_type_getaddr:
	case bitcoin_message_type_mempool:
	case bitcoin_message_type_checkorder:
	case bitcoin_message_type_submitorder:
	case bitcoin_message_type_reply:
	case bitcoin_message_type_ping:
	case bitcoin_message_type_pong:
	case bitcoin_message_type_reject:
	case bitcoin_message_type_filterload:
	case bitcoin_message_type_filteradd:
	case bitcoin_message_type_filterclear:
	case bitcoin_message_type_merkleblock:
	case bitcoin_message_type_alert:
	case bitcoin_message_type_sendheaders:
	case bitcoin_message_type_feefilter:
	case bitcoin_message_type_sendcmpct:
	case bitcoin_message_type_cmpctblock:
	case bitcoin_message_type_getblocktxn:
	case bitcoin_message_type_blocktxn:
	default:
		break;
	}
	return cb_payload;
}

ssize_t bitcoin_message_serialize(struct bitcoin_message * msg, unsigned char ** p_data)
{
#define EMPTY_PAYLOAD_CHECKSUM (0xE2E0F65D)
	assert(msg);
	if(NULL == msg->msg_object) return -1;
	if(msg->msg_type <= bitcoin_message_type_unknown || msg->msg_type >= bitcoin_message_types_count) return -1;
	
	ssize_t total_size = 0;
	const char * sz_type = bitcoin_message_type_to_string(msg->msg_type);
	assert(sz_type);
	
	struct bitcoin_message_header * hdr = msg->hdr;
	if(!hdr->command[0]) strncpy(hdr->command, sz_type, sizeof(hdr->command));
	assert(0 == strcmp(hdr->command, sz_type));
	
	unsigned char * payload = NULL;
	ssize_t cb_payload = 0;
	
	if(NULL == msg->msg_data) {
		cb_payload = message_object_serialize(msg->msg_type, msg->msg_object, &payload);
		if(cb_payload < 0) goto label_error;
		
		struct bitcoin_message_header * msg_data = calloc(1, sizeof(struct bitcoin_message_header) + cb_payload);
		assert(msg_data);
		
		memcpy(msg_data, hdr, sizeof(*hdr));
		msg_data->length = cb_payload;
		
		if(cb_payload > 0) {
			assert(payload);
			unsigned char hash[32];
			hash256(payload, cb_payload, hash);
			memcpy(&msg_data->checksum, hash, 4);
			memcpy(msg_data->payload, payload, cb_payload);
		}else {
			msg_data->checksum = EMPTY_PAYLOAD_CHECKSUM;
		}
		msg->msg_data = msg_data;
		if(payload) free(payload);
	}
	
	total_size = sizeof(*hdr) + cb_payload;
	if(NULL == p_data) return total_size;
	
	void * data = *p_data;
	if(NULL == data) {
		data = malloc(total_size);
		assert(data);
		*p_data = data;
	}
	memcpy(data, msg->msg_data, total_size);
	return total_size;
label_error:
	if(payload) free(payload);
	return -1;

#undef EMPTY_PAYLOAD_CHECKSUM
}

void bitcoin_message_header_dump(const struct bitcoin_message_header * hdr)
{
#ifdef _DEBUG
	printf("==== %s() ====\n", __FUNCTION__);
	printf("magic: 0x%.8x\n", hdr->magic);
	printf("command: %.*s\n", 12, hdr->command);
	printf("length: %u\n", hdr->length);
	printf("checksum: 0x%.8x\n", hdr->checksum);
#endif
	return;
}
