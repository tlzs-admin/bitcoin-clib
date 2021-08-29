#ifndef BITCOIN_MESSAGE_H_
#define BITCOIN_MESSAGE_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "satoshi-types.h"

// https://en.bitcoin.it/wiki/Protocol_documentation
#define BITCOIN_MESSAGE_MAX_PAYLOAD_ENTRIES (50000)

#define BITCOIN_MESSAGE_MAGIC_MAINNET 	0xD9B4BEF9
#define BITCOIN_MESSAGE_MAGIC_TESTNET 	0xDAB5BFFA
#define BITCOIN_MESSAGE_MAGIC_REGTEST	BITCOIN_MESSAGE_MAGIC_TESTNET
#define BITCOIN_MESSAGE_MAGIC_TESTNET3 	0x0709110B
#define BITCOIN_MESSAGE_MAGIC_NAMECOIN	0xFEB4BEF9

enum bitcoin_message_type	// command
{
	bitcoin_message_type_unknown = 0,
	bitcoin_message_type_version = 1,
	bitcoin_message_type_verack,
	bitcoin_message_type_addr,
	bitcoin_message_type_inv,
	bitcoin_message_type_getdata,
	bitcoin_message_type_notfound,
	bitcoin_message_type_getblocks,
	bitcoin_message_type_getheaders,
	bitcoin_message_type_tx,
	bitcoin_message_type_block,
	bitcoin_message_type_headers,
	bitcoin_message_type_getaddr,
	bitcoin_message_type_mempool,
	bitcoin_message_type_checkorder,
	bitcoin_message_type_submitorder,
	bitcoin_message_type_reply,
	bitcoin_message_type_ping, 
	bitcoin_message_type_pong,
	bitcoin_message_type_reject,
	bitcoin_message_type_filterload,
	bitcoin_message_type_filteradd,
	bitcoin_message_type_filterclear,
	bitcoin_message_type_merkleblock,
	bitcoin_message_type_alert,
	bitcoin_message_type_sendheaders,
	bitcoin_message_type_feefilter,
	bitcoin_message_type_sendcmpct,
	bitcoin_message_type_cmpctblock,
	bitcoin_message_type_getblocktxn,
	bitcoin_message_type_blocktxn,
	//
	bitcoin_message_types_count
};
const char * bitcoin_message_type_to_string(enum bitcoin_message_type msg_type);
enum bitcoin_message_type bitcoin_message_type_from_string(const char * command);


/******************************************
 * bitcoin_message_header
******************************************/
struct bitcoin_message_header
{
	uint32_t magic;
	char command[12];
	uint32_t length;
	uint32_t checksum;
	uint8_t payload[0];
}__attribute__((packed));
void bitcoin_message_header_dump(const struct bitcoin_message_header * msg_hdr);

typedef struct bitcoin_message
{
	void * user_data;
	
	struct bitcoin_message_header * msg_data;	// raw_data or serialized payload
	enum bitcoin_message_type msg_type;
	
	struct bitcoin_message_header hdr[1];
	void * msg_object;
	
	int (* parse)(struct bitcoin_message * msg, const struct bitcoin_message_header * hdr, const void * payload, size_t length);
	
	int (* attach)(struct bitcoin_message * msg, uint32_t network_magic, enum bitcoin_message_type msg_type, void * msg_object);
	enum bitcoin_message_type (* detach)(struct bitcoin_message * msg, void ** p_msg_object);

	void (* clear)(struct bitcoin_message * msg);
}bitcoin_message_t;

bitcoin_message_t * bitcoin_message_new(bitcoin_message_t * msg, uint32_t network_magic, enum bitcoin_message_type type, void * user_data);
void bitcoin_message_cleanup(bitcoin_message_t * msg);
int bitcoin_message_parse(bitcoin_message_t * msg, const struct bitcoin_message_header * hdr, const void * payload, size_t length);
void bitcoin_message_clear(bitcoin_message_t * msg);

ssize_t bitcoin_message_serialize(struct bitcoin_message * msg, unsigned char ** p_data);
#define bitcoin_message_get_object(msg) (msg)->msg_object
#define bitcoin_message_free(msg) do { if(msg) { bitcoin_message_cleanup(msg); free(msg); } } while(0)


/******************************************
 * bitcoin_network_address
******************************************/
struct bitcoin_network_address_legacy
{
	uint64_t services;
	char ip[16];
	uint16_t port;
}__attribute__((packed));

struct bitcoin_network_address
{
	uint32_t time; 		// if protocol.version >= 31402
	uint64_t services;	// same service(s) listed in version
	char ip[16];	// IPv6 or IPv4-mapped IPv6 address
	uint16_t port;	// network byte order
}__attribute__((packed));

/******************************************
 * bitcoin_inventory
******************************************/
enum bitcoin_inventory_type
{
	bitcoin_inventory_type_error              = 0,
	bitcoin_inventory_type_msg_tx             = 1,
	bitcoin_inventory_type_msg_block          = 2,
	bitcoin_inventory_type_msg_filtered_block = 3,
	bitcoin_inventory_type_msg_cmpct_block    = 4,
	
	bitcoin_inventory_type_msg_witness_flag   = 0x40000000,
	bitcoin_inventory_type_msg_witness_tx     = 0x40000001,
	bitcoin_inventory_type_msg_witness_block  = 0x40000002,
	bitcoin_inventory_type_msg_witness_filtered_witness_block = 0x40000003,
	
	bitcoin_inventory_type_size = UINT32_MAX
};


/******************************************
 * bitcoin_message_version
******************************************/
/** @todo: BIP: 152
 * https://github.com/bitcoin/bips/blob/master/bip-0152.mediawiki
  Layer: Peer Services
  Title: Compact Block Relay
*/
enum bitcoin_message_service_type
{
	bitcoin_message_service_type_node_network = 1,	// full node
	bitcoin_message_service_type_node_getutxo = 2,	// bip 0064
	bitcoin_message_service_type_node_bloom   = 4,	// bip 0111
	bitcoin_message_service_type_node_witness = 8, 	// bip 0144
	bitcoin_message_service_type_node_network_limited = 1024, // bip 0159
	
	bitcoin_message_service_type_size = UINT64_MAX,	// place holder
};
struct bitcoin_message_version
{
	int32_t version;
	uint64_t services;	// enum bitcoin_message_service_type
	int64_t timestamp;
	struct bitcoin_network_address_legacy addr_recv;
// Fields below require version ≥ 106
	struct bitcoin_network_address_legacy addr_from;
	uint64_t nonce;

// variable length data
	varstr_t * user_agent; // (0x00 if string is 0 bytes long)
	int32_t start_height;	// The last block received by the emitting node
// Fields below require version ≥ 70001
	uint8_t relay;	// bool
};
struct bitcoin_message_version * bitcoin_message_version_parse(struct bitcoin_message_version * msg, const unsigned char * payload, size_t length);
void bitcoin_message_version_cleanup(struct bitcoin_message_version * msg);
ssize_t bitcoin_message_version_serialize(const struct bitcoin_message_version *msg, unsigned char ** p_data);
void bitcoin_message_version_dump(const struct bitcoin_message_version * msg);


/******************************************
 * bitcoin_message_addr
******************************************/
struct bitcoin_message_addr
{
	// varint_t * count;
	ssize_t count;	// <-- varint 
	struct bitcoin_network_address * addrs;
};
struct bitcoin_message_addr * bitcoin_message_addr_parse(struct bitcoin_message_addr * msg, const unsigned char * payload, size_t length);
void bitcoin_message_addr_cleanup(struct bitcoin_message_addr * msg);
ssize_t bitcoin_message_addr_serialize(const struct bitcoin_message_addr * msg, unsigned char ** p_data);


/******************************************
 * bitcoin_message_inv
******************************************/

struct bitcoin_inventory
{
	uint32_t type;
	uint8_t hash[32];
}__attribute__((packed));

struct bitcoin_message_inv
{
	ssize_t count;
	struct bitcoin_inventory * invs;
};
struct bitcoin_message_inv * bitcoin_message_inv_parse(struct bitcoin_message_inv * msg, const unsigned char * payload, size_t length);
void bitcoin_message_inv_cleanup(struct bitcoin_message_inv * msg);
ssize_t bitcoin_message_inv_serialize(const struct bitcoin_message_inv * msg, unsigned char ** p_data);
void bitcoin_message_inv_dump(const struct bitcoin_message_inv * msg);

/******************************************
 * bitcoin_message_getdata
******************************************/
struct bitcoin_message_getdata
{
	ssize_t count;
	struct bitcoin_inventory * invs;
};
struct bitcoin_message_getdata * bitcoin_message_getdata_parse(struct bitcoin_message_getdata * msg, const unsigned char * payload, size_t length);
void bitcoin_message_getdata_cleanup(struct bitcoin_message_getdata * msg);
ssize_t bitcoin_message_getdata_serialize(const struct bitcoin_message_getdata * msg, unsigned char ** p_data);
void bitcoin_message_getdata_dump(const struct bitcoin_message_getdata * msg);

/******************************************
 * bitcoin_message_notfound
******************************************/
struct bitcoin_message_notfound
{
	ssize_t count;
	struct bitcoin_inventory * invs;
};
struct bitcoin_message_notfound * bitcoin_message_notfound_parse(struct bitcoin_message_notfound * msg, const unsigned char * payload, size_t length);
void bitcoin_message_notfound_cleanup(struct bitcoin_message_notfound * msg_notfound);
ssize_t bitcoin_message_notfound_serialize(const struct bitcoin_message_notfound * msg, unsigned char ** p_data);
void bitcoin_message_notfound_dump(const struct bitcoin_message_notfound * msg);

/******************************************
 * bitcoin_message_getblocks
******************************************/
struct bitcoin_message_getblocks
{
	uint32_t version;
	ssize_t hash_count;
	uint256_t * hashes;
	uint256_t hash_stop;
};
struct bitcoin_message_getblocks * bitcoin_message_getblocks_parse(struct bitcoin_message_getblocks * msg, const unsigned char * payload, size_t length);
void bitcoin_message_getblocks_cleanup(struct bitcoin_message_getblocks * msg);
ssize_t bitcoin_message_getblocks_serialize(const struct bitcoin_message_getblocks * msg, unsigned char ** p_data);
void bitcoin_message_getblocks_dump(const struct bitcoin_message_getblocks * msg);

/******************************************
 * bitcoin_message_getheaders
******************************************/
struct bitcoin_message_getheaders
{
	uint32_t version;
	ssize_t hash_count;
	uint256_t * hashes;
	uint256_t hash_stop;
};
struct bitcoin_message_getheaders * bitcoin_message_getheaders_parse(struct bitcoin_message_getheaders * msg, const unsigned char * payload, size_t length);
void bitcoin_message_getheaders_cleanup(struct bitcoin_message_getheaders * msg);
ssize_t bitcoin_message_getheaders_serialize(const struct bitcoin_message_getheaders * msg, unsigned char ** p_data);
void bitcoin_message_getheaders_dump(const struct bitcoin_message_getheaders * msg);

/******************************************
 * bitcoin_message_tx_t
******************************************/
typedef struct satoshi_tx bitcoin_message_tx_t;
bitcoin_message_tx_t * bitcoin_message_tx_parse(bitcoin_message_tx_t * msg, const unsigned char * payload, size_t length);
void bitcoin_message_tx_cleanup(bitcoin_message_tx_t * msg);
ssize_t bitcoin_message_tx_serialize(const bitcoin_message_tx_t * msg, unsigned char ** p_data);
void bitcoin_message_tx_dump(const bitcoin_message_tx_t * msg);

/******************************************
 * bitcoin_message_block_t
******************************************/
typedef struct satoshi_block bitcoin_message_block_t;
bitcoin_message_block_t * bitcoin_message_block_parse(bitcoin_message_block_t * msg, const unsigned char * payload, size_t length);
void bitcoin_message_block_cleanup(bitcoin_message_block_t * msg);
ssize_t bitcoin_message_block_serialize(const bitcoin_message_block_t * msg, unsigned char ** p_data);
void bitcoin_message_block_dump(const bitcoin_message_block_t * msg);

/******************************************
 * struct bitcoin_message_block_headers
******************************************/
struct bitcoin_message_block_header
{
	struct satoshi_block_header hdr;
	ssize_t txn_count;
};
struct bitcoin_message_block_headers
{
	ssize_t count;
	/*
	 * Note that the block headers in this packet include a transaction count 
	 * (a var_int, so there can be more than 81 bytes per header) 
	 * as opposed to the block headers that are hashed by miners.
	*/
	struct bitcoin_message_block_header * hdrs;
};
struct bitcoin_message_block_headers * bitcoin_message_block_headers_parse(struct bitcoin_message_block_headers * msg, 
	const unsigned char * payload, size_t length);
void bitcoin_message_block_headers_cleanup(struct bitcoin_message_block_headers * msg);
ssize_t bitcoin_message_block_headers_serialize(const struct bitcoin_message_block_headers * msg, unsigned char ** p_data);
void bitcoin_message_block_headers_dump(const struct bitcoin_message_block_headers * msg);

#ifdef __cplusplus
}
#endif
#endif
