#ifndef BTC_TRADER_JSON_RESPONSE_H_
#define BTC_TRADER_JSON_RESPONSE_H_

#include <stdio.h>
#include <json-c/json.h>
#include <curl/curl.h>

#include "auto_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct json_response_context
{
	auto_buffer_t buf[1];
	json_object * jresponse;
	
	int auto_parse;
	int err_code;
	long response_code;
	
	json_tokener * jtok;
	enum json_tokener_error jerr;
};

struct json_response_context * json_response_context_init(struct json_response_context * ctx, int auto_parse);
void json_response_context_clear(struct json_response_context * ctx);
void json_response_context_cleanup(struct json_response_context * ctx);

struct http_json_context
{
	void * priv;
	void * user_data;
	
	// public properties
	struct curl_slist * headers;
	struct json_response_context response[1]; 

	// private data
	CURL * curl;
	
	// virtual callback
	size_t (* on_response)(char * ptr, size_t size, size_t n, void * user_data);
	
	// public methods
	int (* add_header)(struct http_json_context * http, const char * key, char * value, int cb_value);
	void (* clear_headers)(struct http_json_context * http);
	
	json_object * (*send)(struct http_json_context * http, const char * method, const char * url, const char * body, ssize_t length);
	json_object * (* get)(struct http_json_context * http, const char * url);
	json_object * (* post)(struct http_json_context	* http, const char * url, const char * body, ssize_t length);
	json_object * (* delete)(struct http_json_context * http, const char * url, const char * body, ssize_t length);
};

struct http_json_context * http_json_context_init(struct http_json_context * http, void * user_data);
void http_json_context_cleanup(struct http_json_context * http);

#ifdef __cplusplus
}
#endif
#endif
