/*
 * block_hdrs-db.c
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
#include <db.h>

#include "block_hdrs-db.h"
#include "satoshi-types.h"
#include "utils.h"
#include <stdint.h>

static int headers_db_open(struct block_headers_db * db);
static int headers_db_close(struct block_headers_db * db);
static ssize_t headers_db_get(struct block_headers_db * db, const uint256_t * hash, struct block_header_record * record);
static ssize_t headers_db_put(struct block_headers_db * db, const uint256_t * hash, const struct block_header_record * record);
static ssize_t headers_db_del(struct block_headers_db * db, const uint256_t * hash);

static ssize_t headers_db_get_by_height(struct block_headers_db * db, uint32_t height, struct block_header_record ** p_records);
	
static int headers_db_move_to(struct block_headers_db * db, int height);
static int headers_db_first(struct block_headers_db * db);
static int headers_db_prior(struct block_headers_db * db);
static int headers_db_next(struct block_headers_db * db);
static int headers_db_last(struct block_headers_db * db);

typedef struct db_private
{
	block_headers_db_t * db;
	
	DB_ENV * db_env;
	DB * dbp;
	DB * sdbp_height;
	
	DBC * cursorp;
}db_private_t;

static int associate_by_height(DB * sdbp, const DBT * key, const DBT * value, DBT * skey)
{
	struct block_header_record * record = (struct block_header_record *)value->data;
	memset(skey, 0, sizeof(*skey));
	skey->data = &record->height;
	skey->size = sizeof(record->height);
	return 0;
}

static int compare_hash(DB * dbp, const DBT * key1, const DBT * key2)
{
	const uint256_t * hash1 = key1->data;
	const uint256_t * hash2 = key2->data;
	
	return uint256_compare(hash1, hash2);
}

static int compare_height(DB * dbp, const DBT * key1, const DBT * key2)
{
	int32_t h1 = *(int32_t *)key1->data;
	int32_t h2 = *(int32_t *)key2->data;
	return (h1 - h2);
}

static db_private_t * db_private_new(block_headers_db_t * db, DB_ENV * db_env)
{
	db_private_t * priv = calloc(1, sizeof(*priv));
	assert(priv);
	priv->db = db;
	db->priv = priv;
	
	priv->db_env = db_env;
	
	DB * dbp = NULL;
	DB * sdbp = NULL;
	
	u_int32_t flags = DB_CREATE;
	int rc = 0;
	
	rc = db_create(&dbp, db_env, 0);
	assert(0 == rc);
	rc = dbp->set_bt_compare(dbp, compare_hash);
	
	rc = dbp->open(dbp, NULL, "headers.db", NULL, DB_BTREE, flags, 0);
	if(rc) {
		// dump err_msg
		assert(0 == rc);
	}
	
	/* heights db */
	rc = db_create(&sdbp, db_env, 0);
	assert(0 == rc);
	rc = sdbp->set_flags(sdbp, DB_DUPSORT);
	assert(0 == rc);
	rc = sdbp->set_bt_compare(sdbp, compare_height);
	assert(0 == rc);
	
	rc = sdbp->open(sdbp, NULL, "headers-height.db", NULL, DB_BTREE, flags, 0);
	assert(0 == rc);
	
	rc = dbp->associate(dbp, NULL, sdbp, associate_by_height, 0);
	assert(0 == rc);
	
	db->limits = 2016;
	db->offset = 0;
	
	db->hashes = calloc(db->limits, sizeof(*db->hashes));
	db->records = calloc(db->limits, sizeof(*db->records));
	
	assert(db->hashes && db->records);
	
	priv->dbp = dbp;
	priv->sdbp_height = sdbp;
	
	return priv;
}

static void db_private_free(db_private_t * priv)
{
	if(NULL == priv) return;
	
	if(priv->cursorp) {
		priv->cursorp->close(priv->cursorp);
		priv->cursorp = NULL;
	}
	
	if(priv->sdbp_height) {
		priv->sdbp_height->close(priv->sdbp_height, 0);
		priv->sdbp_height = NULL;
	}
	
	if(priv->dbp) {
		priv->dbp->close(priv->dbp, 0);
		priv->dbp = NULL;
	}
	free(priv);
}

block_headers_db_t * block_headers_db_init(block_headers_db_t * db, void * db_env, void * user_data)
{
	if(NULL == db) db = calloc(1, sizeof(*db));
	else memset(db, 0, sizeof(*db));
	
	db->user_data = user_data;
	db->open  = headers_db_open;
	db->close = headers_db_close;
	db->get   = headers_db_get;
	db->put   = headers_db_put;
	db->del   = headers_db_del;
	db->get_by_height = headers_db_get_by_height;
	
	db->move_to = headers_db_move_to;
	db->first = headers_db_first;
	db->prior = headers_db_prior;
	db->next = headers_db_next;
	db->last = headers_db_last;
	
	assert(db);
	db_private_t * priv = db_private_new(db, db_env);
	assert(priv && db->priv == priv);

	return db;
}

void block_headers_db_cleanup(block_headers_db_t * db)
{
	if(NULL == db) return;
	db_private_free(db->priv);
	
	if(db->hashes) free(db->hashes);
	if(db->records) free(db->records);
	
	return;
}



static int headers_db_open(struct block_headers_db * db)
{
	return -1;
}
static int headers_db_close(struct block_headers_db * db)
{
	return -1;
}

static ssize_t headers_db_get(struct block_headers_db * db, const uint256_t * _hash, struct block_header_record * record)
{
	assert(db && db->priv);
	assert(_hash);
	
	db_private_t * priv = db->priv;
	DB * dbp = priv->dbp;
	assert(dbp);
	
	uint256_t hash = *_hash;
	
	int rc = 0;
	struct block_header_record record_buffer[1];
	if(NULL == record) {
		memset(record_buffer, 0, sizeof(record_buffer));
		record = record_buffer;
	}
	
	DBT key, value;
	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
	
	key.data = &hash;
	key.size = sizeof(hash);
	
	value.data = record;
	value.ulen = sizeof(*record);
	value.flags = DB_DBT_USERMEM;
	
	rc = dbp->get(dbp, NULL, &key, &value, DB_READ_COMMITTED);
	
	if(rc && rc != DB_NOTFOUND) {
		dbp->err(dbp, rc, "%s() failed.", __FUNCTION__);
		return -1;
	}
	if(rc == DB_NOTFOUND) return 0;
	return 1;
}

static ssize_t headers_db_put(struct block_headers_db * db, const uint256_t * _hash, const struct block_header_record * _record)
{
	assert(db && db->priv);
	assert(_hash && _record);
	
	db_private_t * priv = db->priv;
	DB * dbp = priv->dbp;
	assert(dbp);
		
	uint256_t hash = *_hash;
	struct block_header_record record = *_record;
	
	int rc = 0;
	DBT key, value;
	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
	
	key.data = &hash;
	key.size = sizeof(hash);

	value.data = &record;
	value.size = sizeof(record);
	
	rc = dbp->put(dbp, NULL, &key, &value, 0);
	
	if(rc) {
		dbp->err(dbp, rc, "%s() failed.", __FUNCTION__);
		return -1;
	}
	return 1;
}

static ssize_t headers_db_del(struct block_headers_db * db, const uint256_t * hash)
{
	assert(db && db->priv);
	assert(hash);
	
	db_private_t * priv = db->priv;
	DB * dbp = priv->dbp;
	assert(dbp);
	
	int rc = 0;
	DBT key;
	memset(&key, 0, sizeof(key));
	
	key.data = (void *)hash;
	key.size = sizeof(*hash);
	dbp->del(dbp, NULL, &key, 0);
	
	if(rc && rc != DB_NOTFOUND) {
		dbp->err(dbp, rc, "%s() failed.", __FUNCTION__);
		return -1;
	}
	if(rc == DB_NOTFOUND) return 0;
	return 1;
}

static ssize_t headers_db_get_by_height(struct block_headers_db * db, uint32_t height, struct block_header_record ** p_records)
{
	return -1;
}


static int db_cursor_pget(struct block_headers_db * db, u_int32_t flags)
{
	db_private_t * priv = db->priv;
	int rc = 0;
	assert(priv && priv->dbp && priv->sdbp_height);
	DBC * cursorp = NULL;
	DB * sdbp = priv->sdbp_height;
	
	db->num_records = 0;
	memset(db->hashes, 0, sizeof(*db->hashes) * db->limits);
	memset(db->records, 0, sizeof(*db->records) * db->limits);
	
	DBT skey, key, value;
	memset(&key, 0, sizeof(key));
	memset(&skey, 0, sizeof(skey));
	memset(&value, 0, sizeof(value));
	
	skey.data = &db->offset;
	skey.ulen = sizeof(uint32_t);
	skey.flags = DB_DBT_USERMEM;
	
	key.data = &db->hashes[0];
	key.ulen = sizeof(db->hashes[0]);
	key.flags = DB_DBT_USERMEM;
	
	value.data = &db->records[0];
	value.ulen = sizeof(db->records[0]);
	value.flags = DB_DBT_USERMEM;
	

	rc = sdbp->cursor(sdbp, NULL, &cursorp, DB_READ_COMMITTED);
	if(rc) {
		sdbp->err(sdbp, rc, "%s() create cursor failed.", __FUNCTION__);
		abort();
	}
	
	switch(flags)
	{
	case DB_FIRST: 
		db->offset = 0; 
		break;
	case DB_PREV: 
		db->offset -= db->limits; 
		if(db->offset < 0) db->offset = 0; 
		break;
	case DB_NEXT: 
		++db->offset; 
		break;
	default:
		break;
	}
	

	rc = cursorp->pget(cursorp, &skey, &key, &value, DB_SET);
	if(rc) {
		if(rc != DB_NOTFOUND || flags == DB_SET) {
			sdbp->err(sdbp, rc, "%s(): cursorp->pget(flags=%u) failed.", __FUNCTION__, flags);
			return rc;
		}
	}else {
		++db->num_records;
	}
	
	for(int i = db->num_records; i < db->limits; ++i, ++db->num_records) {
		key.data = &db->hashes[i];
		value.data = &db->records[i];
		
		rc = cursorp->pget(cursorp, &skey, &key, &value, DB_NEXT);
		if(rc) break;
	}
	
	if(rc && rc != DB_NOTFOUND) {
		sdbp->err(sdbp, rc, "%s(): cursorp->pget(flags=%u) failed.", __FUNCTION__, flags);
		abort();
	}
	
	fprintf(stderr, "%s(): offset: %d, num_records=%d, rc = %d\n", 
		__FUNCTION__, 
		db->offset, db->num_records, 
		rc);
	
	cursorp->close(cursorp);
	return rc;
}


static int headers_db_move_to(struct block_headers_db * db, int height)
{
	db->offset = height;
	return db_cursor_pget(db, DB_SET);
}
static int headers_db_first(struct block_headers_db * db)
{
	return db_cursor_pget(db, DB_FIRST);
}
static int headers_db_prior(struct block_headers_db * db)
{
	return db_cursor_pget(db, DB_PREV);
}
static int headers_db_next(struct block_headers_db * db)
{
	return db_cursor_pget(db, DB_NEXT);
}
static int headers_db_last(struct block_headers_db * db)
{
	return db_cursor_pget(db, DB_LAST);
}
