#ifndef BLOCK_HDRS_DB_H_
#define BLOCK_HDRS_DB_H_

#include <stdio.h>
#include <stdint.h>

#include <db.h>
#include "satoshi-types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_header_record
{
	uint32_t height;
	uint32_t txn_count;
	struct satoshi_block_header hdr;
	
	int64_t file_index;
	int64_t file_offset;
	
	int32_t is_orphan;
}__attribute__((packed));

typedef struct block_headers_db
{
	void * priv;
	void * user_data;
	
	int (* open)(struct block_headers_db * db);
	int (* close)(struct block_headers_db * db);
	
	ssize_t (* get)(struct block_headers_db * db, const uint256_t * hash, struct block_header_record * record);
	ssize_t (* put)(struct block_headers_db * db, const uint256_t * hash, const struct block_header_record * record);
	ssize_t (* del)(struct block_headers_db * db, const uint256_t * hash);
	ssize_t (* get_by_height)(struct block_headers_db * db, uint32_t height, struct block_header_record ** p_records);
	

	/* Pagination */
	int limits;
	int offset;
	int num_records;
	
	uint256_t * hashes;
	struct block_header_record * records;
	
	int (* move_to)(struct block_headers_db * db, int height);
	int (* first)(struct block_headers_db * db);
	int (* prior)(struct block_headers_db * db);
	int (* next)(struct block_headers_db * db);
	int (* last)(struct block_headers_db * db);
}block_headers_db_t;

block_headers_db_t * block_headers_db_init(block_headers_db_t * db, void * db_env, void * user_data);
void block_headers_db_cleanup(block_headers_db_t * db);





#ifdef __cplusplus
}
#endif
#endif
