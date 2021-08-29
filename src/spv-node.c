/*
 * spv-node.c
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
#include <getopt.h>

#include <unistd.h>
#include <poll.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>

#include "auto_buffer.h"
#include "chains.h"
#include "utils.h"
#include "spv-node.h"
#include "bitcoin.h"

extern spv_node_message_callback_fn s_spv_node_callbacks[/*bitcoin_message_types_count*/];

volatile int g_quit = 0;
void on_signal(int sig)
{
	if(sig == SIGINT || sig == SIGUSR1) g_quit = 1;
	
	return;
}

static int on_read(struct pollfd * pfd, void * user_data);
static int on_write(struct pollfd * pfd, void * user_data);
static json_object * generate_default_config(void)
{
	json_object * jconfig = NULL;
	jconfig = json_object_new_object();
	assert(jconfig);
	json_set_value(jconfig, string, fullnode, "node1.cloud.c-apps.net");
	json_set_value(jconfig, string, port, "8333");
	json_set_value(jconfig, string, network_type, "mainnet");
	return jconfig;
}

int spv_node_parse_args(spv_node_context_t * spv, int argc, char ** argv)
{
	static struct option options[] = {
		{ "conf", required_argument, 0, 'c' },
		{ "fullnode", required_argument, 0, 'n' },
		{ "port", required_argument, 0, 'p'},
		{ "network_type", required_argument, 0, 't' },
		{ "help", no_argument, 0, 'h' }
	};
	
	const char * conf_file = NULL;
	const char * fullnode = NULL;
	const char * port = NULL;
	const char * network_type = NULL;
	while(1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "c:n:p:t:h", options, &option_index);
		if(c == -1) break;
		
		switch(c) {
		case 'c': conf_file = optarg; break;
		case 'n': fullnode = optarg; break;
		case 'p': port = optarg; break;
		case 't': network_type = optarg; break;
		case 'h':
			printf("Usuage: %s [cnpth]\n", argv[0]);
			exit(0);
		default:
			continue;
		}
	}
	
	if(optind < argc) {
		// save unparsed args
		spv->argc = argc - optind;
		spv->argv = &argv[optind];
	}
	
	if(fullnode) spv->host = fullnode;
	if(port) spv->port = port;
	
	json_object * jconfig = NULL;
	if(conf_file) jconfig =json_object_from_file(conf_file);
	if(NULL == jconfig) jconfig = generate_default_config();

	if(fullnode) json_set_value(jconfig, string, fullnode, fullnode);
	if(port) json_set_value(jconfig, string, port, port);
	if(network_type) json_set_value(jconfig, string, network_type, network_type);
	
	return spv_node_load_config(spv, jconfig);
}



/**************************************************
 * message handlers
**************************************************/

int on_message_handler(spv_node_context_t *spv, const struct bitcoin_message * in_msg)
{
	int rc = 0;
	fprintf(stderr, "[MSG]: type=%s, length=%u\n", 
		bitcoin_message_type_to_string(in_msg->msg_type),
		in_msg->msg_data->length);
	
	if(in_msg->msg_type < 0 || in_msg->msg_type >= bitcoin_message_types_count) return -1;
	
	spv_node_message_callback_fn msg_callback = spv->msg_callbacks[in_msg->msg_type];
	if(msg_callback) return msg_callback(spv, in_msg);
	
	return rc;
}

/**************************************************
 * network controller
**************************************************/
static int on_read(struct pollfd * pfd, void * user_data)
{
	spv_node_context_t * spv = user_data;
	auto_buffer_t * in_buf = spv->in_buf;
	
	ssize_t length = 0;
	int rc = 0;
	
	struct bitcoin_message msg[1];
	memset(msg, 0, sizeof(msg));
	
	while(0 == rc) {
		char data[4096] = "";
		length = read(pfd->fd, data, sizeof(data));
		if(length <= 0) {
			if(length < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
			//	fprintf(stderr, "[INFO]: would block\n");
				break;
			}
			perror("read");
			rc = -1;
			break;
		}
		auto_buffer_push(in_buf, data, length);
		if(in_buf->length < sizeof(struct bitcoin_message_header)) continue;
		
		const unsigned char * p_start = auto_buffer_get_data(in_buf);
		const unsigned char * p_end = p_start + in_buf->length;
		
		while(p_start < p_end) {
			struct bitcoin_message_header * msg_hdr = (struct bitcoin_message_header * )p_start;
			if((p_start + sizeof(*msg_hdr) + msg_hdr->length) > p_end) break;
			
			if(msg_hdr->magic != spv->magic) {
				fprintf(stderr, "\e[31m" "[FATAL ERROR]: invalid network magic" "\e[39m" "\n");
				rc = -1;
				break;
			}
			bitcoin_message_cleanup(msg);
			rc = bitcoin_message_parse(msg, msg_hdr, msg_hdr->payload, msg_hdr->length);
			if(rc) break;
			
			rc = on_message_handler(spv, msg);
			p_start = msg_hdr->payload + msg_hdr->length;
		}
		
		if(p_start < p_end) memmove(in_buf->data, p_start, p_end - p_start);
		in_buf->start_pos = 0;
		in_buf->length = p_end - p_start;
	}
	
	if(0 == rc && spv->out_buf->length > 0) {
		spv->pfd->events |= POLLOUT;
	}
	
	bitcoin_message_cleanup(msg);
	return rc;
}

static int on_write(struct pollfd * pfd, void * user_data)
{
	assert(pfd && user_data);
	spv_node_context_t * spv = user_data;
	
	int rc = 0;
	ssize_t cb = 0;

	auto_buffer_t * out_buf = spv->out_buf;
	const unsigned char * data = auto_buffer_get_data(out_buf);
	ssize_t length = out_buf->length;
	
	while(length > 0) {
		debug_printf("%s(length=%ld)", __FUNCTION__, (long)length); 
		cb = write(pfd->fd, data, length);
		if(cb <= 0) {
			if(length < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
			
			perror("write");
			rc = -1;
			break;
		}
		data += cb;
		length -= cb;
	}
	
	if(length > 0) {
		memmove(out_buf->data, data, length);
	}
	out_buf->length = length;
	out_buf->start_pos = 0;
	
	if(spv->out_buf->length <= 0) {
		spv->pfd->events &= ~POLLOUT;
	}
	return rc;
}

/**************************************************
 * spv context
**************************************************/
static int add_message_version(spv_node_context_t * spv, int protocol_version)
{
	fprintf(stderr, "==== %s() ====\n", __FUNCTION__);
	struct bitcoin_message_version msg_ver[1];
	memset(msg_ver, 0, sizeof(msg_ver));
	
	struct timespec timestamp = {.tv_sec = 0};
	clock_gettime(CLOCK_REALTIME, &timestamp);
	
	msg_ver->version = protocol_version?protocol_version:70012;
	msg_ver->services = bitcoin_message_service_type_node_network 
		| bitcoin_message_service_type_node_network_limited
		| bitcoin_message_service_type_node_witness
		| 0;
	msg_ver->timestamp = timestamp.tv_sec;
	
	// addr_recv
	msg_ver->addr_recv.services = msg_ver->services;
	
	printf("host: %s\n", spv->host);
	
	if(spv->addr_len == sizeof(struct sockaddr_in6)) {
		memcpy(msg_ver->addr_recv.ip, &((struct sockaddr_in6 *)spv->addr)->sin6_addr, 16);
	}else {
		//~ inet_pton(AF_INET, ctx->host, &msg_ver->addr_recv.ip[12]);
		
		memcpy(&msg_ver->addr_recv.ip[12], &((struct sockaddr_in *)spv->addr)->sin_addr, 16);
		msg_ver->addr_recv.ip[10] = 0xff;
		msg_ver->addr_recv.ip[11] = 0xff;
		
	}
	uint16_t port = atoi(spv->port);
	msg_ver->addr_recv.port = htobe16(port);
	
	msg_ver->nonce = (int64_t)timestamp.tv_sec * 1000000 + timestamp.tv_nsec / 1000;
	static const char * user_agent = "/spv-node:0.1.0(protocol=70012)/";
	ssize_t cb = strlen(user_agent);
	msg_ver->user_agent = varstr_set(NULL, (const unsigned char *)user_agent, cb);
	msg_ver->start_height = spv->chain->height;
	msg_ver->relay = 1;
	
	auto_buffer_t * out_buf = spv->out_buf;
	unsigned char * payload = NULL;
	cb = bitcoin_message_version_serialize(msg_ver, &payload);
	assert(cb > 0 && payload);
	
	struct bitcoin_message_header hdr[1] = {{
		.magic = spv->magic,
		.command = "version",
	}};
	hdr->length = cb;
	unsigned char hash[32];
	hash256(payload, cb, hash);
	memcpy(&hdr->checksum, hash, 4);
	
	auto_buffer_push(out_buf, hdr, sizeof(hdr));
	auto_buffer_push(out_buf, payload, cb);
	
	free(payload);
	payload = NULL;
	
	dump_line("raw data: ", out_buf->data, out_buf->length);

	dump_line("version: ", &msg_ver->version, 4);
	dump_line("services: ", &msg_ver->services, 8);
	dump_line("timestamp: ", &msg_ver->timestamp, 8);
	dump_line("addr_recv: ", &msg_ver->addr_recv, sizeof(msg_ver->addr_recv));
	dump_line("addr_from: ", &msg_ver->addr_from, sizeof(msg_ver->addr_from));
	dump_line("nonce: ", &msg_ver->nonce, 8);
	dump_line("user_agent: ", msg_ver->user_agent, varstr_size(msg_ver->user_agent));
	dump_line("start_height: ", &msg_ver->start_height, 4);
	dump_line("relay: ", &msg_ver->relay, 1);
	//~ exit(0);
	
	bitcoin_message_version_cleanup(msg_ver);
	return 0;
}

static int spv_node_send_message(spv_node_context_t * spv, bitcoin_message_t * msg)
{
	ssize_t cb_payload = 0;
	cb_payload = bitcoin_message_serialize(msg, NULL);
	if(cb_payload <= 0) return -1;
	
	int rc = 0;
	long length = 0;
	pthread_mutex_lock(&spv->out_mutex);
	rc = auto_buffer_push(spv->out_buf, msg->msg_data, cb_payload);
	length = spv->out_buf->length;
	pthread_mutex_unlock(&spv->out_mutex);
	
	(void)(length);	// unused
	
	fprintf(stderr, "==== %s() ====\n", __FUNCTION__);
	bitcoin_message_header_dump(msg->msg_data);
	return rc;
}

spv_node_context_t * spv_node_context_init(spv_node_context_t * spv, void * user_data)
{
	if(NULL == spv) spv = calloc(1, sizeof(*spv));
	else memset(spv, 0, sizeof(*spv));
	
	assert(spv);
	spv->user_data = user_data;
	spv->max_retries = 5;
	spv->protocol_version = 70015;
	
	pthread_mutex_init(&spv->in_mutex, NULL);
	pthread_mutex_init(&spv->out_mutex, NULL);
	auto_buffer_init(spv->in_buf, 0);
	auto_buffer_init(spv->out_buf, 0);

	avl_tree_t * tree = avl_tree_init(spv->addrs_list, spv);
	tree->on_free_data = free;
	
	// set default msg_handler_callbacks
	memcpy(spv->msg_callbacks, s_spv_node_callbacks, sizeof(spv->msg_callbacks));
	spv->send_message = spv_node_send_message;
	
	// disable some handlers and use local implementation
	//~ spv->msg_callbacks[bitcoin_message_type_version] = NULL;
	//~ spv->msg_callbacks[bitcoin_message_type_ping] = NULL;
	
	blockchain_t * chain = blockchain_init(spv->chain, 
		NULL, NULL, // mainet default
		spv);
	assert(chain && chain == spv->chain);
	
	return spv;
}

void spv_node_context_cleanup(spv_node_context_t * spv)
{
	if(NULL == spv) return;
	auto_buffer_cleanup(spv->in_buf);
	auto_buffer_cleanup(spv->out_buf);
	
	if(spv->jconfig) {
		json_object_put(spv->jconfig);
		spv->jconfig = NULL;
	}
	
	avl_tree_cleanup(spv->addrs_list);
	
	blockchain_cleanup(spv->chain);
	
	pthread_mutex_destroy(&spv->in_mutex);
	pthread_mutex_destroy(&spv->out_mutex);
	return;
}

int spv_node_load_config(spv_node_context_t * spv, json_object * jconfig)
{
	if(NULL == jconfig) jconfig = generate_default_config();
	assert(jconfig);
	
	spv->jconfig = jconfig;
	spv->host = json_get_value(jconfig, string, fullnode);
	spv->port = json_get_value(jconfig, string, port);
	
	const char * network_type = json_get_value(jconfig, string, network_type);
	if(NULL == network_type) network_type = "mainnet";
	
	int max_retries = json_get_value(jconfig, int, max_retries);
	if(max_retries > 0) spv->max_retries = max_retries;
	
	enum bitcoin_network_type type = bitcoin_network_type_from_string(network_type);
	assert(type != -1);
	
	const char * default_port = "8333";
	switch(type)
	{
	case bitcoin_network_type_default:
	case bitcoin_network_type_mainnet:
		spv->magic = BITCOIN_MESSAGE_MAGIC_MAINNET;
		break;
	case bitcoin_network_type_testnet:
		spv->magic = BITCOIN_MESSAGE_MAGIC_TESTNET3;
		default_port = "18333";
		break;
	case bitcoin_network_type_regtest:
		spv->magic = BITCOIN_MESSAGE_MAGIC_REGTEST;
		default_port = "18444";
		break;
	default:
		return -1;
	}
	if(NULL == spv->port) spv->port = default_port;

	return 0;
}

int spv_node_run(spv_node_context_t * spv, int async_mode)
{
	assert(spv && spv->host && spv->port);
	int rc = 0;
	
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGPIPE);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGINT);

	const struct timespec timeout[1] = {{
		.tv_sec = 0,
		.tv_nsec = 100 * 1000000,
	}};
	
	struct pollfd *pfd = spv->pfd;
	memset(pfd, 0, sizeof(*pfd));
	pfd->events = POLLIN | POLLHUP | POLLRDHUP;

	for(int retries = 0; !g_quit && (retries < spv->max_retries); ++retries) 
	{
		int fd = connect2(spv->host, spv->port, spv->addr, &spv->addr_len);
		if(fd < 0) {
			fprintf(stderr, "[WARNING]: retries: %d, max_retries: %d\n", retries + 1, spv->max_retries);
			sleep(1);
			continue;
		}
		pfd->fd = fd;
		spv->fd = fd;
		
		auto_buffer_cleanup(spv->in_buf);
		auto_buffer_cleanup(spv->out_buf);
		add_message_version(spv, 0);
		pfd->events |= POLLOUT;
		
		while(!g_quit) {
			rc = 0;
			if(spv->out_buf->length > 0) pfd->events |= POLLOUT;
			int n = ppoll(pfd, 1, timeout, &sigs);
			if(n == -1) {
				rc = errno;
				break;
			}
			if(n == 0) // timeout
			{
				if(g_quit || pfd[0].fd < 0) break;
				continue;
			}
			
			if(pfd[0].revents & POLLIN) {
				rc = on_read(pfd, spv);
			}
			if(0 == rc && (pfd[0].revents & POLLOUT)) {
				rc = on_write(pfd, spv);
			}
			// printf("rc = %d, revents = %.8x\n", rc, pfd[0].revents);
			
			if(rc < 0 
				|| (pfd[0].revents & POLLERR) 
				|| (pfd[0].revents & POLLHUP)
				|| (pfd[0].revents & POLLRDHUP)
				|| 0)
			{
				fprintf(stderr, "error: rc = %d, revents = 0x%.8x\n", rc, (unsigned int)pfd[0].revents);
				if(pfd[0].fd >= 0) {
					close(pfd[0].fd);
					pfd[0].fd = -1;
				}
				fd = -1;
				spv->fd = -1;
			}
		}
	
	}
	
	return 0;
}
