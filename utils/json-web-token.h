#ifndef CHLIB_JSON_WEB_TOKEN_H_
#define CHLIB_JSON_WEB_TOKEN_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include <gnutls/gnutls.h>


#define JSON_WEB_TOKEN_MAX_BUFFER_SIZE (65536)
typedef struct json_web_token
{
	json_object * jwt_token;
	json_object * jheader;
	json_object * jclaims;
	json_object * jsignature;
	
	int states;
	int err_code;
	
	struct {
		const char * typ;	// "JWT"
		const char * alg;	// "HS256" or "RS256"
	};
	
	// private data
	unsigned char * secret;
	size_t cb_secret;
	gnutls_privkey_t privkey;
	gnutls_pubkey_t pubkey;
	gnutls_datum_t signature;
	
	// public::read data
	char b64_data[JSON_WEB_TOKEN_MAX_BUFFER_SIZE];
	ssize_t cb_data;
	
	// public functions
	int (* set_algorithm)(struct json_web_token * jwt, const char * algo);
	int (* add_claims)(struct json_web_token * jwt, const char * key1, const char * value1, ...) __attribute__ ((sentinel(0)));
	
	int (* set_secret)(struct json_web_token * jwt, unsigned char * secret, size_t size); // for HMAC-SHAxxx
	
	int (* set_privkey)(struct json_web_token * jwt, const char * privkey_pem, size_t size);
	int (* sign)(struct json_web_token * jwt);
	
	int (* set_pubkey)(struct json_web_token * jwt,  const char * pubkey_pem, size_t size);
	int (* verify)(struct json_web_token * jwt);
	
	ssize_t (* serialize)(struct json_web_token * jwt, char ** p_output);
	
}json_web_token_t;
json_web_token_t * json_web_token_init(json_web_token_t * jwt, void * user_data);
void json_web_token_cleanup(json_web_token_t * jwt);

#ifdef __cplusplus
}
#endif
#endif
