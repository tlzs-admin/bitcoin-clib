#ifndef CHLIB_SATOSHI_TYPES_H_
#define CHLIB_SATOSHI_TYPES_H_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * varint
 * @defgroup varint
 * Integer can be encoded depending on the represented value to save space. 
 * Variable length integers always precede an array/vector of a type of data that may vary in length. 
 * Longer numbers are encoded in little endian.
 * 
 *    Value	Storage length	Format
 *    (< 0xFD)	1	uint8_t
 *    (<= 0xFFFF)	3	0xFD followed by the length as uint16_t
 *    (<= 0xFFFF FFFF)	5	0xFE followed by the length as uint32_t
 *    else	9	0xFF followed by the length as uint64_t
 * 
 * @{
 * @}
 * 
 */
 
 /**
  * @ingroup varint
  */
typedef struct varint
{
	unsigned char vch[1];
}varint_t;
size_t varint_calc_size(uint64_t value);

/**
  * @ingroup varint
  * @{
  */
varint_t * varint_new(uint64_t value);
void varint_free(varint_t * vint);

size_t varint_size(const varint_t * vint);
varint_t * varint_set(varint_t * vint, uint64_t value);
uint64_t varint_get(const varint_t * vint);

/**
 * @}
 */


/**
 * varstr
 * @defgroup varstr
 * 
 * @{
 * @}
 */
 
 
/**
 * @ingroup varstr
 */
typedef struct varstr
{
	unsigned char vch[1];
}varstr_t;
extern const varstr_t varstr_empty[1];

/**
 * @ingroup varstr
 */
varstr_t * varstr_new(const unsigned char * data, size_t length);
void varstr_free(varstr_t * vstr);
varstr_t * varstr_resize(varstr_t * vstr, size_t new_size);
varstr_t * varstr_clone(const varstr_t * vstr);

size_t varstr_size(const varstr_t * vstr);
varstr_t * varstr_set(varstr_t * vstr, const unsigned char * data, size_t length);
//~ size_t varstr_length(const varstr_t * vstr);
size_t varstr_get(const varstr_t * vstr, unsigned char ** p_data, size_t buf_size);

#define varstr_length(vstr)			varint_get((varint_t *)vstr)
#define varstr_getdata_ptr(vstr) 	((unsigned char *)vstr + varint_size((varint_t *)vstr))
/**
 * @}
 */


#ifndef HAS_UINT256
#define HAS_UINT256
typedef struct uint256
{
	uint8_t val[32];
}uint256, uint256_t;
#endif
//~ extern const uint256_t uint256_zero[1];
#define uint256_zero (&(uint256_t){{0}})

void uint256_reverse(uint256_t * u256);
ssize_t uint256_from_string(uint256_t * u256, int from_little_endian, const char * hex_string, ssize_t length);
char * uint256_to_string(const uint256_t * u256, int to_little_endian, char ** p_hex);
void uint256_add(uint256_t * c, const uint256_t *a, const uint256_t *b);

/**
 * @defgroup compact_int
 *  Use a 32-bit integer(little-endian) to approximate a 256-bit integer.
 * 
 * @details 
 *  The Bitcoin network has a global block difficulty. 
 *  Valid blocks must have a hash below this target. 
 *  The target is a 256-bit number (extremely large) that all Bitcoin clients share. 
 *  The dSHA256 hash of a block's header must be lower than or equal to the current target 
 *  for the block to be accepted by the network. 
 *  The lower the target, the more difficult it is to generate a block.
 * 
 *  It should be noted here that, the dSHA256 hash value SHOULD be regarded as little-endian.
 * 	(Perhaps to avoid confusion, the designer specified all integers as little-endian.)
 * 	 
 *  The target value was stored in a compact format(uint32_t) int the 'bit' field of block header. 
 *  --> the lower 24 bits represent an signed integer value, and must be positive (the highest bit cannot be 1).
 *      When converting from an 'uint256_t', if the lower24 bits value is negative, 
 *      then need to add an extra 0 to the hightest byte (need to borrow a 0 from the tailing-zeros of the uint256)
 *  --> the upper 8 bits indicate that how many bytes remained 
 * 		after removing the tailing-0s(if regarded as big-endian) or leading-0s(if regarded as little-endian)
 * 
 *  for example:
 * 	    bits: 0x 1b 0404cb
 * 	    target= (0x0404cb << ((0x1b - 3) * 8))
 * 		TARGET= 0x0000000000 0404CB (000000000000000000000000000000000000000000000000)
 * 		dSHA256=  (000000000000000000000000000000000000000000000000) CB0404 0000000000
 *	 PS. The parts enclosed in parentheses are ignored. (not used when calculating the target)
 * @{
 * @}
 */
union compact_uint256
{
	uint32_t bits;		// 
	struct
	{
		uint8_t mantissa[3]; 
		uint8_t exp;
	}__attribute__((packed));
}__attribute__((packed));

typedef union compact_uint256 compact_uint256_t;
compact_uint256_t uint256_to_compact(const uint256_t * target);
uint256_t compact_to_uint256(const compact_uint256_t * cint);
compact_uint256_t compact_uint256_complement(const compact_uint256_t target);
compact_uint256_t compact_uint256_add(const compact_uint256_t a, const compact_uint256_t b);
double compact_uint256_to_bdiff(const compact_uint256_t * cint);
double compact_uint256_to_pdiff(const compact_uint256_t * cint);
#define compact_uint256_to_difficulty compact_uint256_to_bdiff

//~ #define compact_uint256_zero 	((compact_uint256_t){.bits = 0, })
#define compact_uint256_NaN 	((compact_uint256_t){.bits = 0xffffffff, })	// Not a Number

#define uint256_NaN	((uint256_t){.val = {						\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 		\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,			\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,			\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }})

// The highest possible target (difficulty 1) is defined as 0x1d00ffff
#define compact_uint256_difficulty_one  ((compact_uint256_t){.bits = 0x1d00ffff, })
#define uint256_difficulty_one ((uint256_t){.val = {			\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 		\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,			\
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,			\
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 }})
/**
 * helpler functions to calculate difficulty.
 * 
 * 	compact_int_div(): use to calc bdiff
 * 	uint256_div():     use to calc pdiff
 * 
 * For the explanation of 'bdiff' and 'pdiff', please refer to 'https://en.bitcoin.it/wiki/Difficulty'
 */
double compact_uint256_div(const compact_uint256_t * restrict n, const compact_uint256_t * restrict d);
double uint256_div(const uint256_t * restrict n, const uint256_t * restrict d);

/**
 * compare functions:
 * @return 
 *   <0 if (a  < b);
 *   =0 if (a == b);
 *   >0 if (a  > b);
 * 
 * @{
 */
int compact_uint256_compare(const compact_uint256_t * restrict a, const compact_uint256_t * restrict b);
int uint256_compare(const uint256_t * restrict a, const uint256_t * restrict b);
int uint256_compare_with_compact(const uint256_t * restrict hash, const compact_uint256_t * restrict target);
/**
 * @}
 */

/**
 * @defgroup merkle_tree
 * @{
 * @}
 */
typedef struct uint256_merkle_tree
{
	void * user_data;
	void * priv;
	
	ssize_t max_size;
	ssize_t count;
	uint256 merkle_root;
	uint256 * items;
	int levels;
	
	void (* hash_func)(const void *data, size_t size, uint8_t hash[]);
	int (* add)(struct uint256_merkle_tree * mtree, int count, const uint256 * items);
	int (* remove)(struct uint256_merkle_tree * mtree, int index);
	int (* set)(struct uint256_merkle_tree * mtree, int index, const uint256 item);
	
	int (* recalc)(struct uint256_merkle_tree * mtree, int start_index, int count);
}uint256_merkle_tree_t;
uint256_merkle_tree_t * uint256_merkle_tree_new(ssize_t max_size, void * user_data);
void uint256_merkle_tree_free(uint256_merkle_tree_t * mtree);


/**
 * @ingroup satoshi_tx
 * 
 */
typedef struct satoshi_outpoint
{
	uint8_t prev_hash[32];
	uint32_t index;
}satoshi_outpoint_t;

typedef struct satoshi_txin
{
	satoshi_outpoint_t outpoint;
	int is_coinbase;	// coinbase flag
	int is_p2sh;		// p2sh flag
	
	varstr_t * scripts;
	ssize_t cb_scripts;	// hold scripts length
	uint32_t sequence;
	
	// The following fields will be set during satoshi_script->parse()
	ssize_t num_signatures;	
	varstr_t ** signatures;

	varstr_t * redeem_scripts; 	// { (varint_length) , (pubkey or redeem script) }
	
	/**
	 * redeem_scripts_start_pos: The position after the most recently-executed op_codeseparator.
	 * 
	 * According to 'https://en.bitcoin.it/wiki/Script': 
	 * "All of the signature checking words will only match signatures 
	 * to the data after the most recently-executed OP_CODESEPARATOR."
	 * 
	 * This means if any op_codeseparator exists in the redeem_scripts,  
	 * the data required by the next checksig-ops will start 
	 * from the most recently-executed op_codeseparator.
	 */
	ptrdiff_t redeem_scripts_start_pos;
	
}satoshi_txin_t;

ssize_t satoshi_txin_parse(satoshi_txin_t * txin, ssize_t length, const void * payload);
ssize_t satoshi_txin_serialize(const satoshi_txin_t * txin, unsigned char ** p_data);
void satoshi_txin_cleanup(satoshi_txin_t * txin);
varstr_t * satoshi_txin_get_redeem_scripts(const satoshi_txin_t * txin);
ssize_t satoshi_txin_query_redeem_scripts_data(const satoshi_txin_t * txin, const unsigned char ** p_data);
varstr_t * satoshi_txin_set_redeem_scripts(satoshi_txin_t * txin, const unsigned char * data, size_t length);

enum satoshi_txout_type
{
	satoshi_txout_type_unknown = 0,
	satoshi_txout_type_legacy = 1,
	satoshi_txout_type_segwit = 2,	// native p2wphk or p2wsh
	
	satoshi_txout_type_masks = 0x7FFF,
	satoshi_txout_type_p2sh_segwit_flags = 0x8000,	// (p2sh --> p2wphk or p2wsh)
};

typedef struct satoshi_txout
{
	int64_t value;
	varstr_t * scripts;
	enum satoshi_txout_type flags;			// 0: a legacy-utxo, 1: a segwit-utxo
}satoshi_txout_t;
ssize_t satoshi_txout_parse(satoshi_txout_t * txout, ssize_t length, const void * payload);
ssize_t satoshi_txout_serialize(const satoshi_txout_t * txout, unsigned char ** p_data);
void satoshi_txout_cleanup(satoshi_txout_t * txout);


/*
 * A witness field starts with a var_int to indicate the number of stack items for the txin. 
 * It is followed by stack items, with each item starts with a var_int to indicate the length. 
 * Witness data is NOT script.
 */
typedef struct bitcoin_tx_witness
{
	ssize_t num_items;	//
	varstr_t ** items; 
}bitcoin_tx_witness_t;

typedef struct satoshi_tx
{
	int32_t version;
	int has_flag;	// with witness 
	uint8_t flag[2]; // If present, always 0001, and indicates the presence of witness data
	ssize_t txin_count;
	satoshi_txin_t * txins;
	
	ssize_t txout_count;
	satoshi_txout_t * txouts;
	
	ssize_t cb_witnesses;		// witnesses data serialized length (in bytes)
	bitcoin_tx_witness_t * witnesses;
	uint32_t lock_time;
	
	/*
	 *  Definition of txid remains unchanged: the double SHA256 of the traditional serialization format:
	 *   [nVersion][txins][txouts][nLockTime]
	 */
	uint256_t txid[1];
	
	/*
	 * (https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki)
	 * A new wtxid is defined: the double SHA256 of the new serialization with witness data:
	 * [nVersion][marker][flag][txins][txouts][witness][nLockTime]
	 */
	uint256_t wtxid[1];
}satoshi_tx_t;
ssize_t satoshi_tx_parse(satoshi_tx_t * tx, ssize_t length, const void * payload);
void satoshi_tx_cleanup(satoshi_tx_t * tx);
ssize_t satoshi_tx_serialize(const satoshi_tx_t * tx, unsigned char ** p_data);
void satoshi_tx_dump(const satoshi_tx_t * tx);

struct satoshi_block_header
{
	int32_t version;
	uint256_t prev_hash[1];
	uint256_t merkle_root[1];
	uint32_t timestamp;
	uint32_t bits;
	uint32_t nonce;
	uint8_t txn_count[0];	// place-holder
}__attribute__((packed));
int satoshi_block_header_verify(const struct satoshi_block_header * hdr, uint256_t * p_hash /* (nullable) output of the block_hash */);
void satoshi_block_header_dump(const struct satoshi_block_header * hdr);

typedef struct satoshi_block
{
	struct satoshi_block_header hdr;
	ssize_t txn_count;
	satoshi_tx_t * txns;
	
	uint256_t hash;
}satoshi_block_t;
ssize_t satoshi_block_parse(satoshi_block_t * block, ssize_t length, const void * payload);
void satoshi_block_cleanup(satoshi_block_t * block);
ssize_t satoshi_block_serialize(const satoshi_block_t * block, unsigned char ** p_data);
void satoshi_block_dump(const satoshi_block_t * block);


#ifdef __cplusplus
}
#endif

#endif
