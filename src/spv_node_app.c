/*
 * spv_node_app.c
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

#include <signal.h>
#include <errno.h>

#include "spv-node.h"
#include "app.h"
#include "utils.h"
#include <db.h>

void on_signal(int sig);
int main(int argc, char **argv)
{
	signal(SIGINT, on_signal);
	signal(SIGUSR1, on_signal);
	
	int rc = 0;
	app_context_t * app = app_context_new(NULL);
	assert(app);
	
	rc = app_context_parse_args(app, argc, argv);
	assert(0 == rc);
	
	rc = app_run(app);

	app_context_free(app);
	return rc;
}

static int on_add_block(struct blockchain * chain, 
	const uint256_t * block_hash, const int height, 
	const struct satoshi_block_header * hdr,
	void * user_data);
static int on_remove_block(struct blockchain * chain, 
		const uint256_t * block_hash, 
		const int height, void * user_data);
app_context_t * app_context_init(app_context_t * app, void * user_data)
{
	if(NULL == app) app = calloc(1, sizeof(*app));
	else memset(app, 0, sizeof(*app));
	assert(app);
	
	spv_node_context_t * spv = spv_node_context_init(app->spv, app);
	assert(spv);
	
	const char * db_home = "data";
	DB_ENV * db_env = NULL;
	int rc = db_env_create(&db_env, 0);
	assert(0 == rc);
	
	u_int32_t flags = DB_INIT_MPOOL 
		//~ | DB_INIT_CDB
		| DB_INIT_LOCK
		| DB_INIT_LOG
		//~ | DB_INIT_REP
		| DB_INIT_TXN
		//~ | DB_REGISTER
		| DB_RECOVER
		| DB_THREAD
		| DB_CREATE
		| 0;
	rc = db_env->open(db_env, db_home, flags, 0666);
	if(rc) {
		db_env->err(db_env, rc, "db_env->open()");
		abort();
	}
	app->db_env = db_env;
	
	block_headers_db_t * hdrs_db = block_headers_db_init(app->hdrs_db, db_env, app);
	assert(hdrs_db);
		
	blockchain_t * chain = spv->chain;
	assert(chain);
	chain->on_add_block = on_add_block;
	chain->on_remove_block = on_remove_block;

	return app;
}

void app_context_cleanup(app_context_t * app)
{
	if(NULL == app) return;
	spv_node_context_cleanup(app->spv);
	block_headers_db_cleanup(app->hdrs_db);
	
	if(app->db_env) {
		DB_ENV * db_env = app->db_env;
		app->db_env = NULL;
		
		db_env->close(db_env, 0);
	}
	return;
}

int app_context_parse_args(app_context_t * app, int argc, char ** argv)
{
	int rc = spv_node_parse_args(app->spv, argc, argv);
	assert(0 == rc);

	return rc;
}


static int on_message_verack(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_inv(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getdata(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_notfound(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getblocks(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_getheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_tx(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int on_message_headers(struct spv_node_context * spv, const bitcoin_message_t * in_msg);
static int custom_init(spv_node_context_t * spv)
{
	spv_node_message_callback_fn * callbacks = spv->msg_callbacks;
	assert(callbacks);

	callbacks[bitcoin_message_type_verack]     = on_message_verack;
	callbacks[bitcoin_message_type_inv]        = on_message_inv;
	callbacks[bitcoin_message_type_getdata]    = on_message_getdata;
	callbacks[bitcoin_message_type_notfound]   = on_message_notfound;
	callbacks[bitcoin_message_type_getblocks]  = on_message_getblocks;
	callbacks[bitcoin_message_type_getheaders] = on_message_getheaders;
	callbacks[bitcoin_message_type_tx]         = on_message_tx;
	callbacks[bitcoin_message_type_block]      = on_message_block;
	callbacks[bitcoin_message_type_headers]    = on_message_headers;

	return 0; 
}

extern volatile int g_quit;
int app_run(app_context_t * app)
{
	int rc = 0;
	
	spv_node_context_t * spv = app->spv;
	block_headers_db_t * db = app->hdrs_db;	
	blockchain_t * chain = spv->chain;
	assert(spv && db && chain);
	
	// set the context for on_add_block/on_remove_block callbacks
	chain->user_data = db;
	custom_init(spv);
	
	// load block headers from local db
	while(!g_quit && 0 == db->next(db))
	{
		assert(db->hashes && db->records);
		uint256_t * hashes = db->hashes;
		struct block_header_record * records = db->records;
		
		for(int i = 0; i < db->num_records; ++i) 
		{
			rc = chain->add(chain, &db->hashes[i], &records[i].hdr);
			if(rc == -1) { // duplicated found, should never happen
				fprintf(stderr, "DUP FOUND: hash=");
				dump2(stderr, &hashes[i], 32);
				fprintf(stderr, "\n");
				abort();
			}
		}
	}
	
	fprintf(stderr, "latest height: %ld\n", (long)chain->height);
	
	rc = spv_node_run(app->spv, 0);
	return rc;
}

int app_stop(app_context_t * app)
{
	on_signal(SIGUSR1);
	return 0;
}




#define COLOR_RED "\e[31m"
#define COLOR_DEFAULT "\e[39m"

static int on_remove_block(struct blockchain * chain, 
		const uint256_t * block_hash, 
		const int height, void * user_data)
{
	fprintf(stderr, COLOR_RED "== %s(height=%d): hash=", __FUNCTION__, height);
	dump2(stderr, block_hash, sizeof(*block_hash));
	fprintf(stderr, COLOR_DEFAULT "\n");
	
	block_headers_db_t * db = user_data;
	assert(user_data);
	
	ssize_t count = db->del(db, block_hash);
	assert(count == 1);
	return 0;
}

static int on_add_block(struct blockchain * chain, 
	const uint256_t * block_hash, const int height, 
	const struct satoshi_block_header * hdr,
	void * user_data)
{
	fprintf(stderr, COLOR_RED"== %s(height=%d): hash=", __FUNCTION__, height);
	dump2(stderr, block_hash, sizeof(*block_hash));
	fprintf(stderr, COLOR_DEFAULT"\n");
	
	block_headers_db_t * db = user_data;
	assert(user_data);
	
	const struct block_header_record record = {
		.height = height,
		.hdr = *hdr,
	};
	
	ssize_t count = db->put(db, block_hash, &record);
	assert(count == 1);
	return 0;
}


static int bitcoin_message_getheaders_set(struct bitcoin_message_getheaders * getheaders,
	uint32_t version,
	ssize_t hash_count,
	const uint256_t * known_hashes,
	const uint256_t * hash_stop)
{
	if(hash_count <= 0 || hash_count > 2000) return -1;

	assert(known_hashes);
	
	uint256_t * hashes = realloc(getheaders->hashes, sizeof(uint256_t) * hash_count);
	assert(hashes);
	getheaders->hashes = hashes;
	
	getheaders->version = version;
	getheaders->hash_count = hash_count;
	memcpy(hashes, known_hashes, sizeof(uint256_t) * hash_count);
	if(hash_stop) memcpy(&getheaders->hash_stop, hash_stop, sizeof(*hash_stop));
	
	return 0;
}


static int on_message_verack(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	// send test data when 'verack' msg received
	bitcoin_message_header_dump(in_msg->msg_data);
	
	struct bitcoin_message * getheaders_msg = bitcoin_message_new(NULL, 
		in_msg->msg_data->magic, 
		bitcoin_message_type_getheaders, 
		spv);
	struct bitcoin_message_getheaders * getheaders = bitcoin_message_get_object(getheaders_msg);
	assert(getheaders);
	
	uint32_t version = spv->peer_version;
	if(0 == version || version > spv->protocol_version) version = spv->protocol_version;
	
	
	uint256_t * hashes = NULL;
	blockchain_t * chain = spv->chain;
	assert(chain && chain->add);
	
	ssize_t count = blockchain_get_known_hashes(chain, 0, &hashes);
	assert(count > 0 && hashes);
	
	int rc = bitcoin_message_getheaders_set(getheaders, version, count, hashes, NULL);
	if(0 == rc) {
		if(spv->send_message) spv->send_message(spv, getheaders_msg);
	}
	free(hashes);
	bitcoin_message_free(getheaders_msg);
	return rc;
}

static int on_message_inv(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_inv * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_inv_dump(msg);
	
	/** 
	 * getdata is used in response to inv, to retrieve the content of a specific object, 
	 * and is usually sent after receiving an inv packet, after filtering known elements. 
	*/
	struct bitcoin_message * getdata_msg = bitcoin_message_new(NULL, 
		in_msg->msg_data->magic, 
		bitcoin_message_type_getdata, 
		spv);

	assert(getdata_msg);
	struct bitcoin_message_getdata * getdata = bitcoin_message_get_object(getdata_msg);
	
	///< @todo : filter-out known elements
	getdata->count = msg->count;
	getdata->invs = (struct bitcoin_inventory *)msg->invs;

	bitcoin_message_getdata_dump(getdata);
	assert(spv->send_message);
	
	if(spv->send_message) spv->send_message(spv, getdata_msg);
	
	// clear data
	getdata->count = 0;
	getdata->invs = NULL;
	
	bitcoin_message_free(getdata_msg);
	return 0;
}

static int on_message_getdata(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_getdata * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_getdata_dump(msg);
	return 0;
}

static int on_message_notfound(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_getdata * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_getdata_dump(msg);
	return 0;
}

static int on_message_getblocks(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_getblocks * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_getblocks_dump(msg);
	
	return 0;
}

static int on_message_getheaders(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_getheaders * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_getheaders_dump(msg);
	return 0;
}

static int on_message_tx(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	bitcoin_message_tx_t * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_tx_dump(msg);
	
//	exit(0);
	return 0;
}

static int on_message_block(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	bitcoin_message_block_t * msg = bitcoin_message_get_object(in_msg);
	bitcoin_message_block_dump(msg);
	
//	exit(0);
	return 0;
}

static int on_message_headers(struct spv_node_context * spv, const bitcoin_message_t * in_msg)
{
	struct bitcoin_message_block_headers * msg = bitcoin_message_get_object(in_msg);
	if(NULL == msg || msg->count == 0) return 0;
	if(msg->count < 0) return -1;
	
	bitcoin_message_block_headers_dump(msg);

	blockchain_t * chain = spv->chain;
	block_headers_db_t * db = chain->user_data;
	
	assert(chain && chain->add && db);
	int rc = 0;
	
	for(int i = 0; i < msg->count; ++i) {	
		rc = chain->add(chain, NULL, &msg->hdrs[i].hdr);
		if(rc) break;	
	}
	
	ssize_t height = chain->height;
	fprintf(stderr, "\e[32m" "current height: %ld" "\e[39m" "\n", (long)height);
	
	// pull more headers
	struct bitcoin_message * getheaders_msg = bitcoin_message_new(NULL, 
		in_msg->msg_data->magic, 
		bitcoin_message_type_getheaders, 
		spv);
	struct bitcoin_message_getheaders * getheaders = bitcoin_message_get_object(getheaders_msg);
	assert(getheaders);
	
	uint32_t version = spv->peer_version;
	if(0 == version || version > spv->protocol_version) version = spv->protocol_version;
	
	uint256_t * hashes = NULL;
	ssize_t count = blockchain_get_known_hashes(chain, 100, &hashes);
	assert(count > 0 && hashes);
	
	rc = bitcoin_message_getheaders_set(getheaders, version, count, hashes, NULL);
	if(0 == rc) {
		if(spv->send_message) spv->send_message(spv, getheaders_msg);
	}
	free(hashes);
	bitcoin_message_free(getheaders_msg);
	
	assert(0 == rc);
	return rc;
}
