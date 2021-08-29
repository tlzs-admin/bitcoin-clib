/*
 * json-response.c
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

#include <json-c/json.h>
#include "json-response.h"

#include "utils.h"

struct json_response_context * json_response_context_init(struct json_response_context * ctx, int auto_parse)
{
	if(NULL == ctx) ctx = malloc(sizeof(*ctx));
	assert(ctx);
	memset(ctx, 0, sizeof(*ctx));
	
	auto_buffer_init(ctx->buf, 0);
	if((ctx->auto_parse = auto_parse)) {
		ctx->jtok = json_tokener_new();
		assert(ctx->jtok);
	}
	return ctx;
}

void json_response_context_clear(struct json_response_context * ctx)
{
	if(NULL == ctx) return;
	if(ctx->jresponse) {
		json_object_put(ctx->jresponse);
		ctx->jresponse = NULL;
	}
	json_tokener_reset(ctx->jtok);
	ctx->jerr = 0;
	
	auto_buffer_t * buf = ctx->buf;
	if(buf->data) {
		buf->length = 0;
		buf->start_pos = 0;
	}
	return;
}

void json_response_context_cleanup(struct json_response_context * ctx)
{
	if(NULL == ctx) return;
	auto_buffer_cleanup(ctx->buf);
	json_response_context_clear(ctx);
	
	if(ctx->jtok) {
		json_tokener_free(ctx->jtok);
		ctx->jtok = NULL;
	}
	return;
}


/****************************************************
 * http_json_context
****************************************************/
__attribute__((constructor)) static void global_init(void) 
{
	curl_global_init(CURL_GLOBAL_ALL);
	return;
}

// virtual function
static size_t http_on_response_default(char * ptr, size_t size, size_t n, struct http_json_context * http);

// public methods
static int http_add_header(struct http_json_context * http, const char * key, char * value, int cb_value);
static void http_clear_headers(struct http_json_context * http);

static json_object * http_send(struct http_json_context * http, const char * method, const char * url, const char * body, ssize_t length);
static json_object * http_get(struct http_json_context * http, const char * url);
static json_object * http_post(struct http_json_context	* http, const char * url, const char * body, ssize_t length);
static json_object * http_delete(struct http_json_context * http, const char * url, const char * body, ssize_t length);

struct http_json_context * http_json_context_init(struct http_json_context * http, void * user_data)
{
	if(NULL == http) http = calloc(1, sizeof(*http));
	
	http->user_data = user_data;
	
	http->add_header = http_add_header;
	http->clear_headers = http_clear_headers;
	http->send = http_send;
	http->get = http_get;
	http->post = http_post;
	http->delete = http_delete;
	
	http->on_response = (void *)http_on_response_default;
	
	CURL * curl = curl_easy_init();
	assert(curl);
	
	http->curl = curl;
	json_response_context_init(http->response, 1);
	return http;
}
void http_json_context_cleanup(struct http_json_context * http)
{
	if(NULL == http) return;
	json_response_context_cleanup(http->response);
	
	if(http->curl) {
		curl_easy_cleanup(http->curl);
		http->curl = NULL;
	}
	
	if(http->headers) {
		curl_slist_free_all(http->headers);
		http->headers = NULL;
	}
	return;
}

static size_t http_on_response_default(char * ptr, size_t size, size_t n, struct http_json_context * http)
{
	assert(http);
	struct json_response_context * response = http->response;
	auto_buffer_t * buf = response->buf;
	
	size_t cb = size * n;
	if(cb == 0) return 0;
	
	auto_buffer_push(buf, ptr, cb);
	if(!response->auto_parse) return cb;
	
	json_tokener * jtok = response->jtok;
	if(NULL == jtok) return 0;
	
	json_object * jobject = json_tokener_parse_ex(jtok, ptr, (int)cb);
	response->jerr = json_tokener_get_error(jtok);
	if(response->jerr == json_tokener_continue) return cb;
	if(response->jerr != json_tokener_success) {
		fprintf(stderr, "%s(%d)::json_token_parse failed: %s\n",
			__FILE__, __LINE__, json_tokener_error_desc(response->jerr));
		fprintf(stderr, "buffer: %*s\n", (int)buf->length, (char *)buf->data);
		return 0;
	}
	response->jresponse = jobject;
	return cb;
}

static int http_add_header(struct http_json_context * http, const char * key, char * value, int cb_value)
{
	assert(http && key);
	int cb_key = strlen(key);
	assert(cb_key > 0);
	if(value && cb_value <= 0) cb_value = strlen(value);
	
	int cb = cb_key + 2;	// "key" + ": "
	if(cb_value > 0) cb += cb_value;
	
	assert(cb > 0);
	char * header = calloc(1, cb + 1);
	assert(header);
	snprintf(header, cb + 1, "%s: %s", key, value?value:"");
	
	http->headers = curl_slist_append(http->headers, header);
	free(header);
	return 0;
}

static void http_clear_headers(struct http_json_context * http)
{
	if(http->headers) {
		curl_slist_free_all(http->headers);
		http->headers = NULL;
	}
	return;
}
static json_object * http_send(struct http_json_context * http, const char * method, const char * url, const char * body, ssize_t length)
{
	assert(method);
	if(strcasecmp(method, "GET") == 0) return http_get(http, url);
	if(strcasecmp(method, "POST") == 0) return http_post(http, url, body, length);
	if(strcasecmp(method, "DELETE") == 0) return http_delete(http, url, body, length);
	
	http->response->err_code = 1;
	http->response->response_code = 501;
	fprintf(stderr, "[ERROR]::Not Implemented");
	
	return NULL;
}
static json_object * http_get(struct http_json_context * http, const char * url)
{
	assert(http && http->curl);
	
	debug_printf("%s(%p, %s) ...", __FUNCTION__, http, url);
	
	json_object * jresponse = NULL;
	CURL * curl = http->curl;
	curl_easy_reset(curl);
	
	struct json_response_context * response = http->response;
	json_response_context_clear(response);
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	if(http->on_response) {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http->on_response);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, http);
	}
	if(http->headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->headers);
	}
	
	CURLcode ret = curl_easy_perform(curl);
	response->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->response_code);
	response->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	
	if(http->headers) {
		curl_slist_free_all(http->headers);
		http->headers = NULL;
	}
	
	if(response->jresponse) jresponse = json_object_get(response->jresponse);
	return jresponse;
}

static json_object * http_post(struct http_json_context	* http, const char * url, const char * body, ssize_t length)
{
	assert(http && http->curl);
	
	json_object * jresponse = NULL;
	CURL * curl = http->curl;
	
	
	struct json_response_context * response = http->response;
	json_response_context_clear(response);
	
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	if(body) {	
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
		if(length > 0) curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, length);
	}
	if(http->on_response) {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http->on_response);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, http);
	}
	if(http->headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->headers);
	}
	
	CURLcode ret = curl_easy_perform(curl);
	response->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->response_code);
	response->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	
	if(http->headers) {
		curl_slist_free_all(http->headers);
		http->headers = NULL;
	}
	
	if(response->jresponse) jresponse = json_object_get(response->jresponse);
	
	curl_easy_reset(curl);
	return jresponse;
}
static json_object * http_delete(struct http_json_context * http, const char * url, const char * body, ssize_t length)
{
	assert(http && http->curl);
	
	json_object * jresponse = NULL;
	CURL * curl = http->curl;
	curl_easy_reset(curl);
	
	struct json_response_context * response = http->response;
	json_response_context_clear(response);
	
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	if(body) {	
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
		if(length > 0) curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, length);
	}
	if(http->on_response) {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http->on_response);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, http);
	}
	if(http->headers) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->headers);
	}
	
	CURLcode ret = curl_easy_perform(curl);
	response->err_code = ret;
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_perform() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	ret = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->response_code);
	response->err_code = ret;
	
	if(ret != CURLE_OK) {
		fprintf(stderr, "%s(%d)::curl_easy_getinfo() failed: %s\n", __FILE__, __LINE__, curl_easy_strerror(ret));
		return NULL;
	}
	
	if(http->headers) {
		curl_slist_free_all(http->headers);
		http->headers = NULL;
	}
	
	if(response->jresponse) jresponse = json_object_get(response->jresponse);
	return jresponse;
}
