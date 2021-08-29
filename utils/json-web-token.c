/*
 * json-web-token.c
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
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>
#include <gnutls/abstract.h>

#include "json-web-token.h"
#include "base64.h"


/******************************************
 * struct json_web_token
*******************************************/
static int jwt_set_algorithm(struct json_web_token * jwt, const char * algo);
static int jwt_add_claims(struct json_web_token * jwt, const char * key1, const char * value1, ...) __attribute__ ((sentinel(0)));
static int jwt_set_secret(struct json_web_token * jwt, unsigned char * secret, size_t size); 
static int jwt_set_privkey(struct json_web_token * jwt, const char * privkey_pem, size_t cb_key);
static int jwt_sign(struct json_web_token * jwt);
static int jwt_set_pubkey(struct json_web_token * jwt,  const char * pubkey_pem, size_t cb_key);
static int jwt_verify(struct json_web_token * jwt);
static ssize_t jwt_serialize(struct json_web_token * jwt, char ** p_output);
	
json_web_token_t * json_web_token_init(json_web_token_t * jwt, void * user_data)
{
	if(NULL == jwt) jwt = calloc(1, sizeof(*jwt));
	jwt->typ = "JWT";
	jwt->alg = "RS256";
	
	jwt->set_algorithm = jwt_set_algorithm;
	jwt->add_claims = jwt_add_claims;
	jwt->set_secret = jwt_set_secret;

	// for RSA-SHAxxx or ECDSA-SHAxxx
	jwt->set_privkey = jwt_set_privkey;
	jwt->sign = jwt_sign;
	jwt->set_pubkey = jwt_set_pubkey;
	jwt->verify = jwt_verify;
	
	// encode to jwt_stirng
	jwt->serialize  = jwt_serialize;
	
	json_object * jtoken = json_object_new_object();
	json_object * jheader = json_object_new_object();
	json_object * jclaims = json_object_new_object();
	
	json_object_object_add(jtoken, "header", jheader);
	json_object_object_add(jtoken, "claims", jclaims);
	json_object_object_add(jheader, "alg", json_object_new_string(jwt->alg));
	json_object_object_add(jheader, "typ", json_object_new_string(jwt->typ));
	
	jwt->jwt_token = jtoken;
	jwt->jheader = jheader;
	jwt->jclaims = jclaims;

	gnutls_privkey_init(&jwt->privkey);
	gnutls_pubkey_init(&jwt->pubkey);

	return jwt;
}

static int jwt_set_algorithm(struct json_web_token * jwt, const char * algo)
{
	///< @todo
	
	return -1;
}
static int jwt_add_claims(struct json_web_token * jwt, const char * key1, const char * value1, ...)
{
	const char * key = key1;
	const char * value = value1;
	
	assert(jwt && jwt->jclaims);
	json_object * jclaims = jwt->jclaims;
	
	va_list ap;
	va_start(ap, value1);
	while(key) {
		json_object_object_add(jclaims, key, json_object_new_string(value?value:""));
		
		key = va_arg(ap, const char *);
		if(NULL == key) break;
		value = va_arg(ap, const char *);
	}
	va_end(ap);
	return 0;
}
static int jwt_set_secret(struct json_web_token * jwt, unsigned char * secret, size_t size)
{
	return -1;
}
static int jwt_set_privkey(struct json_web_token * jwt, const char * privkey_pem, size_t size)
{
	const gnutls_datum_t key_data = {
		.data = (unsigned char *)privkey_pem,
		.size = size,
	};
	return gnutls_privkey_import_x509_raw(jwt->privkey, &key_data, GNUTLS_X509_FMT_PEM, NULL, 0);
}

static size_t pack_json_object_base64(json_object * jobj, char ** p_b64_string)
{
	static const size_t buf_size = JSON_WEB_TOKEN_MAX_BUFFER_SIZE;
	ssize_t length = 0;
	char * buf = calloc(buf_size, 1);
	assert(buf);
	
	char * p = buf;
	char * p_end = p + buf_size; 
	
	struct json_object_iterator iter = json_object_iter_begin(jobj);
	struct json_object_iterator iter_end = json_object_iter_end(jobj);
	*p++ = '{';
	while(!json_object_iter_equal(&iter, &iter_end)) {
		const char * key_name = json_object_iter_peek_name(&iter);
		int cb_key = strlen(key_name);
		assert(cb_key > 0);
		
		json_object * jvalue = json_object_iter_peek_value(&iter);
		assert(json_object_get_type(jvalue) == json_type_string);
		const char * value = json_object_get_string(jvalue);
		int cb_value = json_object_get_string_len(jvalue);
		
		if(p[-1] != '{') *p++ = ',';
		assert((p + 3 + cb_key + 2 + cb_value) < p_end);
		// pack key_name
		int cb = snprintf(p, p_end - p, "\"%s\":", key_name);
		assert(cb > 0);
		p += cb;
		
		// pack value
		*p++= '"';
		if(value && cb_value > 0) { memcpy(p, value, cb_value); p += cb_value; };
		*p++= '"';
		json_object_iter_next(&iter);
	}
	*p++ = '}'; 
	
	length = base64_urlencode(buf, p - buf, p_b64_string);
	assert(length > 0);
	
	while(length > 0 && (*p_b64_string)[length - 1] == '=') (*p_b64_string)[--length] = '\0';
	free(buf);
	
	return length;
}

static int jwt_sign(struct json_web_token * jwt)
{
	static const size_t buf_size = sizeof(jwt->b64_data);
	// clear old results
	if(jwt->signature.data) {
		gnutls_free(jwt->signature.data);
		jwt->signature.data = NULL;
	}
	jwt->signature.size = 0;
	
	jwt->b64_data[0] = '\0';
	jwt->cb_data = 0;
	
	json_object * jheader = jwt->jheader;
	json_object * jclaims = jwt->jclaims;
	
	char * buf = jwt->b64_data;
	char * p = buf;
	char * p_end = p + buf_size; 
	
	ssize_t cb_header = pack_json_object_base64(jheader, &p);
	assert(cb_header > 0);
	assert((p + cb_header + 1) < p_end);
	p += cb_header;
	
	// pack claims
	*p++ = '.';
	ssize_t cb_claims = pack_json_object_base64(jclaims, &p);
	assert(cb_claims > 0);
	assert((p + cb_claims + 1) <= p_end);
	p += cb_claims;
	
	
	ssize_t length = p - buf;
	int rc = gnutls_privkey_sign_data(jwt->privkey, GNUTLS_DIG_SHA256, 0, 
		&(gnutls_datum_t){.data = (unsigned char *)buf, .size = length}, 
		&jwt->signature);
	assert(0 == rc);
	
	// pack signature
	*p++ = '.';
	ssize_t cb_sig = base64_urlencode(jwt->signature.data, jwt->signature.size, &p);
	assert(cb_sig > 0);
	while(cb_sig > 0 && p[cb_sig - 1] == '=') p[--cb_sig] = '\0';
	
	p += cb_sig;
	assert(p < p_end);
	*p = '\0';
	
	jwt->cb_data = p - buf;
	
	return 0;
}
static int jwt_set_pubkey(struct json_web_token * jwt,  const char * pubkey_pem, size_t cb_key)
{
	return -1;
}
static int jwt_verify(struct json_web_token * jwt)
{
	return -1;
}
static ssize_t jwt_serialize(struct json_web_token * jwt, char ** p_output)
{
	return 0;
}

void json_web_token_cleanup(json_web_token_t * jwt)
{
	if(NULL == jwt) return;
	
	if(jwt->privkey) {
		gnutls_privkey_deinit(jwt->privkey);
		jwt->privkey = NULL;
	}
	if(jwt->pubkey) {
		gnutls_pubkey_deinit(jwt->pubkey);
		jwt->pubkey = NULL;
	}
	if(jwt->signature.data) {
		gnutls_free(jwt->signature.data);
		jwt->signature.data = NULL;
	}
	
	if(jwt->secret) {
		free(jwt->secret);
		jwt->secret = NULL;
	}
	jwt->cb_secret = 0;
	
	if(jwt->jwt_token) {
		json_object_put(jwt->jwt_token);
		jwt->jwt_token = NULL;
	}
	return;
}
