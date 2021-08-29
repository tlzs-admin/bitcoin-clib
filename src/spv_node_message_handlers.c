/*
 * spv_node_message_handlers.c
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

#include "utils.h"
#include "spv-node.h"
#include "avl_tree.h"

static int on_message_unknown(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_version(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_verack(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_addr(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_inv(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getdata(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_notfound(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getblocks(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_tx(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_headers(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getaddr(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_mempool(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_checkorder(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_submitorder(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_reply(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_ping(struct spv_node_context * spv, const bitcoin_message_t * in_msg); 
static int on_message_pong(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_reject(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_filterload(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_filteradd(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_filterclear(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_merkle_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_alert(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_sendheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_feefilter(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_sendcmpct(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_cmpctblock(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getblocktxn(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_blocktxn(struct spv_node_context * spv, const bitcoin_message_t * in_msg);

spv_node_message_callback_fn s_spv_node_callbacks[bitcoin_message_types_count] = 
{
	[bitcoin_message_type_unknown] = on_message_unknown,
	[bitcoin_message_type_version] = on_message_version,
	[bitcoin_message_type_verack] = on_message_verack,
	[bitcoin_message_type_addr] = on_message_addr,
	[bitcoin_message_type_inv] = on_message_inv,
	[bitcoin_message_type_getdata] = on_message_getdata,
	[bitcoin_message_type_notfound] = on_message_notfound,
	[bitcoin_message_type_getblocks] = on_message_getblocks,
	[bitcoin_message_type_getheaders] = on_message_getheaders,
	[bitcoin_message_type_tx] = on_message_tx,
	[bitcoin_message_type_block] = on_message_block,
	[bitcoin_message_type_headers] = on_message_headers,
	[bitcoin_message_type_getaddr] = on_message_getaddr,
	[bitcoin_message_type_mempool] = on_message_mempool,
	[bitcoin_message_type_checkorder] = on_message_checkorder,
	[bitcoin_message_type_submitorder] = on_message_submitorder,
	[bitcoin_message_type_reply] = on_message_reply,
	[bitcoin_message_type_ping] = on_message_ping, 
	[bitcoin_message_type_pong] = on_message_pong,
	[bitcoin_message_type_reject] = on_message_reject,
	[bitcoin_message_type_filterload] = on_message_filterload,
	[bitcoin_message_type_filteradd] = on_message_filteradd,
	[bitcoin_message_type_filterclear] = on_message_filterclear,
	[bitcoin_message_type_merkleblock] = on_message_merkle_block,
	[bitcoin_message_type_alert] = on_message_alert,
	[bitcoin_message_type_sendheaders] = on_message_sendheaders,
	[bitcoin_message_type_feefilter] = on_message_feefilter,
	[bitcoin_message_type_sendcmpct] = on_message_sendcmpct,
	[bitcoin_message_type_cmpctblock] = on_message_cmpctblock,
	[bitcoin_message_type_getblocktxn] = on_message_getblocktxn,
	[bitcoin_message_type_blocktxn] = on_message_blocktxn,
};

static int send_message_verack(spv_node_context_t * spv, const struct bitcoin_message * in_msg)
{
	static struct bitcoin_message_header hdr[1] = {{
		.magic = BITCOIN_MESSAGE_MAGIC_MAINNET,
		.command = "verack",
		.length = 0,
		.checksum = 0xe2e0f65d
	}};
	hdr->magic = in_msg->msg_data->magic;
	
	auto_buffer_push(spv->out_buf, hdr, sizeof(*hdr));
	return 0;
}

static int send_message_pong(spv_node_context_t * spv, const struct bitcoin_message * in_msg)
{
	const struct bitcoin_message_header * msg_data = in_msg->msg_data;
	assert(msg_data->length == sizeof(uint64_t));
	
	struct 
	{
		struct bitcoin_message_header hdr;
		uint64_t nonce;
	} msg_pong = {
		.hdr.magic = msg_data->magic,
		.hdr.command = "pong",
		.hdr.length = sizeof(uint64_t),
		.hdr.checksum = msg_data->checksum,
		.nonce = *(uint64_t *)msg_data->payload
	};
	
	auto_buffer_push(spv->out_buf, &msg_pong, sizeof(msg_pong));
	return 0;
}

static int on_message_unknown(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return -1;
}

static int on_message_version(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	bitcoin_message_version_dump(in_msg->msg_object);
	
	const struct bitcoin_message_version * msg_ver = bitcoin_message_get_object(in_msg);
	assert(msg_ver);
	spv->peer_version = msg_ver->version;
	spv->peer_height = msg_ver->start_height;
	
	return send_message_verack(spv, in_msg);
}

static int on_message_verack(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}


static int network_address_compare(const void * a, const void * b)
{
	const struct bitcoin_network_address * addr1 = a;
	const struct bitcoin_network_address * addr2 = b;
	return memcmp(addr1->ip, addr2->ip, sizeof(addr1->ip) + sizeof(addr1->port));
}

static int on_message_addr(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	assert(spv && in_msg);
	
	struct bitcoin_message_addr * msg_addr = in_msg->msg_object;
	assert(msg_addr);
	debug_printf("%s(num_addrs=%ld)", __FUNCTION__, (long)msg_addr->count);
	
	avl_tree_t * addrs_list = spv->addrs_list;
	for(int i = 0; i < msg_addr->count; ++i) 
	{
		struct bitcoin_network_address * addr = calloc(1, sizeof(*addr));
		assert(addr);
		memcpy(addr, &msg_addr->addrs[i], sizeof(*addr));
		
		fprintf(stderr, "[addrs:%.4d]: ", i);
		dump2(stderr, addr, sizeof(*addr));
		fprintf(stderr, "\n"); 

		void * p_node = avl_tree_add(addrs_list, addr, network_address_compare);
		assert(p_node);
		
		struct bitcoin_network_address * item = *(void **)p_node;
		if(item != addr) { // update existing item
			item->time = addr->time;
			item->services = addr->services;
			free(addr);
		}
	}
	
	debug_printf("saved address count: %ld", (long)addrs_list->count);
	return 0;
}

static int on_message_inv(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	bitcoin_message_inv_dump(in_msg->msg_object);
	return 0;
}

static int on_message_getdata(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}
static int on_message_notfound(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_getblocks(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}
static int on_message_getheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	bitcoin_message_getheaders_dump(in_msg->msg_object);
	return 0;
}

static int on_message_tx(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}
static int on_message_headers(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_getaddr(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_mempool(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_checkorder(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	fprintf(stderr, "[WARNING]: %s(): %s\n", 
		__FUNCTION__,
		"This message was used for IP Transactions. As IP transactions have been deprecated, it is no longer used."
	);
	return 0;
}

static int on_message_submitorder(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	fprintf(stderr, "[WARNING]: %s(): %s\n", 
		__FUNCTION__,
		"This message was used for IP Transactions. As IP transactions have been deprecated, it is no longer used."
	);
	return 0;
}

static int on_message_reply(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	fprintf(stderr, "[WARNING]: %s(): %s\n", 
		__FUNCTION__,
		"This message was used for IP Transactions. As IP transactions have been deprecated, it is no longer used."
	);
	return 0;
}

static int on_message_ping(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return send_message_pong(spv, in_msg);
}
 
static int on_message_pong(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_reject(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_filterload(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_filteradd(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_filterclear(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_merkle_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_alert(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_sendheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	spv->send_headers_flag = 1;
	return 0;
}

static int on_message_feefilter(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_sendcmpct(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_cmpctblock(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_getblocktxn(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}

static int on_message_blocktxn(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	debug_printf("%s(%p)", __FUNCTION__, in_msg);
	return 0;
}
