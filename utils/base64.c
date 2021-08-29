/*
 * base64.c
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
#include <errno.h>

#define MAKE_USHORT_LE(c1, c2) (((uint16_t)(c1) & 0xFF) | (((uint16_t)(c2) & 0xFF) << 8))
#define MAKE_UINT32_LE(c1, c2, c3, c4) \
		(((uint32_t)(c1)  & 0xFF ) | (((uint32_t)(c2) & 0xFF) << 8) \
		| (((uint32_t)(c3) & 0xFF) << 16)| (((uint32_t)(c4) & 0xFF) << 24) \
	)


static const char _b64[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};
static const char _b64_url[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '-', '_'
};
static const unsigned char _b64_digits[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,    // +,/ 
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,    // 0-9
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,	// A-Z
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,    
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,	// a-z
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};
static const unsigned char _b64_url_digits[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,    // -
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,    // 0-9
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,	// A-Z
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,    
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,	// a-z
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,63,	// _
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

ssize_t base64_encode(const void * data, size_t data_len, char ** p_dst)
{
	if(NULL == data || data_len == 0) return 0;
	
	size_t cb = (data_len * 4) / 3 + 3;
	if(NULL == p_dst) return cb + 1;

	char * to = *p_dst;
	if(NULL == to) {
		to = malloc(cb + 1); 
		assert(to);
		*p_dst = to;
	}
	const unsigned char * p_data = (const unsigned char *)data;
	uint32_t * p = (uint32_t *)to;
	size_t i;
	
	size_t len = data_len / 3 * 3;
	size_t cb_left = data_len - len;
	
	for(i = 0; i < len; i += 3)
	{
		*p++ = MAKE_UINT32_LE( _b64[(p_data[i] >> 2) & 0x3F],
						_b64[((p_data[i] &0x03) << 4) | (((p_data[i + 1]) >> 4) & 0x0F)],
						_b64[((p_data[i + 1] & 0x0F) << 2) | ((p_data[i + 2] >> 6) & 0x03)],
						_b64[(p_data[i + 2]) & 0x3F]
						);
	}
	
	if(cb_left == 2)
	{
		*p++ = MAKE_UINT32_LE(_b64[(p_data[i] >> 2) & 0x3F],
							_b64[((p_data[i] &0x03) << 4) | (((p_data[i + 1]) >> 4) & 0x0F)],
							_b64[((p_data[i + 1] & 0x0F) << 2) | 0],
							'=');
		
	}else if(cb_left == 1)
	{
		*p++ = MAKE_UINT32_LE(_b64[(p_data[i] >> 2) & 0x3F],
							_b64[((p_data[i] &0x03) << 4) | 0],
							'=',
							'=');
	}
	
	cb = (char *)p - to;	
	to[cb] = '\0';
	
	return cb;
	
}


ssize_t base64_decode(const char * from, size_t cb_from, unsigned char ** p_dst)
{
	if(NULL == from) return 0;
	if(-1 == cb_from) cb_from = strlen(from);
	if(0 == cb_from) return 0;
	
	if(cb_from % 4) 
	{
		errno = EINVAL;
		return -1;
	}

	size_t dst_size = ((cb_from + 3) / 4 * 3);
	
	if(NULL == p_dst) return dst_size;

	unsigned char * to = *p_dst;
	if(NULL == to) {
		to = malloc(dst_size);
		assert(to);
		*p_dst = to;
	}
	size_t count = cb_from / 4;
	
	const unsigned char * p_from = (const unsigned char *)from;
	const char * p_end = from + cb_from;
	
	unsigned char * p_to = to;
	union
	{
		uint32_t u;
		uint8_t c[4];
	}val;
	
	if(p_end[-1] == '=') count--;
	while(count--)
	{
		//~ val.u = *(uint32_t *)p_from;
		
		val.u = MAKE_UINT32_LE(	_b64_digits[p_from[0]], _b64_digits[p_from[1]],
								_b64_digits[p_from[2]], _b64_digits[p_from[3]]);
		
		if(val.c[0] == 0xff || val.c[1] == 0xff || val.c[2] == 0xff || val.c[3] == 0xff)
		{
			errno = EINVAL;
			return -1;
		}
		
		p_to[0] = (val.c[0] << 2) | ((val.c[1] >> 4) & 0x3);
		p_to[1] = ((val.c[1] & 0x0F) << 4) | ((val.c[2] >> 2) & 0x0F);
		p_to[2] = ((val.c[2] & 0x03) << 6) | (val.c[3] & 0x3F);
		
		p_from += 4;
		p_to += 3;
		
	}
	
	if(p_end[-1] == '=')
	{	
		if(p_end[-2] == '=')	
			val.u = MAKE_UINT32_LE(	_b64_digits[p_from[0]], _b64_digits[p_from[1]], 0, 0);
		else
			val.u = MAKE_UINT32_LE(	_b64_digits[p_from[0]], _b64_digits[p_from[1]], _b64_digits[p_from[2]], 0);
		
		if(val.c[0] == 0xff || val.c[1] == 0xff || val.c[2] == 0xff || val.c[3] == 0xff)
		{
			errno = EINVAL;
			return -1;
		}
		
		p_to[0] = (val.c[0] << 2) | ((val.c[1] >> 4) & 0x3);
		p_to[1] = ((val.c[1] & 0x0F) << 4) | ((val.c[2] >> 2) & 0x0F);
		p_to[2] = ((val.c[2] & 0x03) << 6) | (val.c[3] & 0x3F);
		p_to += 3;
	}
	return (size_t)(p_to - (unsigned char *)to);
}


ssize_t base64_urlencode(const void * data, size_t data_len, char ** p_dst)
{
	if(NULL == data || data_len == 0) return 0;
	
	size_t cb = (data_len * 4) / 3 + 3;
	if(NULL == p_dst) return cb + 1;

	char * to = *p_dst;
	if(NULL == to) {
		to = malloc(cb + 1); 
		assert(to);
		*p_dst = to;
	}
	const unsigned char * p_data = (const unsigned char *)data;
	uint32_t * p = (uint32_t *)to;
	size_t i;
	
	size_t len = data_len / 3 * 3;
	size_t cb_left = data_len - len;
	
	for(i = 0; i < len; i += 3)
	{
		*p++ = MAKE_UINT32_LE( _b64_url[(p_data[i] >> 2) & 0x3F],
						_b64_url[((p_data[i] &0x03) << 4) | (((p_data[i + 1]) >> 4) & 0x0F)],
						_b64_url[((p_data[i + 1] & 0x0F) << 2) | ((p_data[i + 2] >> 6) & 0x03)],
						_b64_url[(p_data[i + 2]) & 0x3F]
						);
	}
	
	if(cb_left == 2)
	{
		*p++ = MAKE_UINT32_LE(_b64_url[(p_data[i] >> 2) & 0x3F],
							_b64_url[((p_data[i] &0x03) << 4) | (((p_data[i + 1]) >> 4) & 0x0F)],
							_b64_url[((p_data[i + 1] & 0x0F) << 2) | 0],
							'=');
		
	}else if(cb_left == 1)
	{
		*p++ = MAKE_UINT32_LE(_b64_url[(p_data[i] >> 2) & 0x3F],
							_b64_url[((p_data[i] &0x03) << 4) | 0],
							'=',
							'=');
	}
	
	cb = (char *)p - to;
	to[cb] = '\0';
	
	return cb;
	
}


ssize_t base64_urldecode(const char * from, size_t cb_from, unsigned char ** p_dst)
{
	if(NULL == from) return 0;
	if(-1 == cb_from) cb_from = strlen(from);
	if(0 == cb_from) return 0;
	
	if(cb_from % 4) 
	{
		errno = EINVAL;
		return -1;
	}

	size_t dst_size = ((cb_from + 3) / 4 * 3);
	
	if(NULL == p_dst) return dst_size;

	unsigned char * to = *p_dst;
	if(NULL == to) {
		to = malloc(dst_size);
		assert(to);
		*p_dst = to;
	}
	size_t count = cb_from / 4;
	
	const unsigned char * p_from = (const unsigned char *)from;
	const char * p_end = from + cb_from;
	
	unsigned char * p_to = to;
	union
	{
		uint32_t u;
		uint8_t c[4];
	}val;
	
	if(p_end[-1] == '=') count--;
	while(count--)
	{
		//~ val.u = *(uint32_t *)p_from;
		
		val.u = MAKE_UINT32_LE(	_b64_url_digits[p_from[0]], _b64_url_digits[p_from[1]],
								_b64_url_digits[p_from[2]], _b64_url_digits[p_from[3]]);
		
		if(val.c[0] == 0xff || val.c[1] == 0xff || val.c[2] == 0xff || val.c[3] == 0xff)
		{
			errno = EINVAL;
			return -1;
		}
		
		p_to[0] = (val.c[0] << 2) | ((val.c[1] >> 4) & 0x3);
		p_to[1] = ((val.c[1] & 0x0F) << 4) | ((val.c[2] >> 2) & 0x0F);
		p_to[2] = ((val.c[2] & 0x03) << 6) | (val.c[3] & 0x3F);
		
		p_from += 4;
		p_to += 3;
		
	}
	
	if(p_end[-1] == '=')
	{	
		if(p_end[-2] == '=')	
			val.u = MAKE_UINT32_LE(	_b64_url_digits[p_from[0]], _b64_url_digits[p_from[1]], 0, 0);
		else
			val.u = MAKE_UINT32_LE(	_b64_url_digits[p_from[0]], _b64_url_digits[p_from[1]], _b64_url_digits[p_from[2]], 0);
		
		if(val.c[0] == 0xff || val.c[1] == 0xff || val.c[2] == 0xff || val.c[3] == 0xff)
		{
			errno = EINVAL;
			return -1;
		}
		
		p_to[0] = (val.c[0] << 2) | ((val.c[1] >> 4) & 0x3);
		p_to[1] = ((val.c[1] & 0x0F) << 4) | ((val.c[2] >> 2) & 0x0F);
		p_to[2] = ((val.c[2] & 0x03) << 6) | (val.c[3] & 0x3F);
		p_to += 3;
	}
	return (size_t)(p_to - (unsigned char *)to);
}


