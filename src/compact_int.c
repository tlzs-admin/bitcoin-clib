/*
 * compact_int.c
 * 
 * Copyright 2020 Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "satoshi-types.h"
#include <math.h>
#include <gmp.h>

#ifndef debug_printf 
#define debug_printf(fmt, ...) do { \
		printf("\e[33m" "%s()::"fmt "\e[39m""\n", __FUNCTION__, ##__VA_ARGS__);	\
	} while(0)
#endif

compact_uint256_t uint256_to_compact(const uint256_t * target)
{
	compact_uint256_t cint = { .bits = 0 };
	
	int num_zeros = 0;
	const unsigned char * p_end = (const unsigned char *)target + 32;
	
	// count the number of 0s at the end
	while(num_zeros < 32 && (p_end[-1] == 0)) { --p_end; ++num_zeros;}
	
	debug_printf("num_zeros: %d", num_zeros);
	
	// make sure that the mantissa represents a positive value 
	if((p_end[-1]  & 0x80)) {
		if(num_zeros == 0) { // out of the range that a compact_uint256 can represent
			return compact_uint256_NaN;
		}
		++p_end;
		--num_zeros;
	}
	
	int num_bytes = 3;	// 24 bits
	cint.exp = 32 - num_zeros;
	debug_printf("exp: %d (0x%.2x)", (int)cint.exp, (int)cint.exp);
	
	if((cint.exp + num_bytes) >= 32) num_bytes = 32 - cint.exp; 
	if((num_zeros + num_bytes) >= 32) num_bytes = 32 - num_zeros;
	
	
	
	
	
	memcpy(cint.mantissa, (p_end - num_bytes), num_bytes);
	return cint;
}
uint256_t compact_to_uint256(compact_uint256_t * cint)
{
	uint256_t target = *uint256_zero;
	if(cint->exp >= 32) return target;

	int num_bytes = 3;
	int num_zeros = 32 - cint->exp;
	
	if((cint->exp + num_bytes) >= 32) num_bytes = 32 - cint->exp; 
	if((num_zeros + num_bytes) >= 32) num_bytes = 32 - num_zeros;
	debug_printf("num_zeros: %d, num_bytes: %d, exp: 0x%.2x", num_zeros, num_bytes, (int)cint->exp);
	
	unsigned char * p = (unsigned char *)&target + cint->exp - num_bytes;
	memcpy(p, cint->mantissa, num_bytes);

	return target;
}

/**
 * helpler functions to calculate difficulty.
 * 
 * 	compact_int_div(): use to calc bdiff
 * 	uint256_div():     use to calc pdiff
 * 
 * For the explanation of 'bdiff' and 'pdiff', please refer to 'https://en.bitcoin.it/wiki/Difficulty'
 */

double compact_uint256_div(const compact_uint256_t * n, const compact_uint256_t * d)
{
	if((0 == (d->bits & 0x0FFFFFF)) || (d->exp >= 32) || (d->exp == 0)) return NAN;	
	if((0 == (n->bits & 0x0FFFFFF)) || (n->exp >= 32) || (n->exp == 0)) return 0.0;
	
	double a = (double)( n->bits & 0x0FFFFFF);
	double b = (double)(d->bits & 0x0FFFFFF);

	int exponent_diff = ((int)n->exp - (int)d->exp) * 8;	// bytes to bits
	double coef = pow(2.0, exponent_diff);
	
	return a / b * coef;
}

double uint256_div(const uint256_t * n, const uint256_t * d)
{
	mpz_t divident, divisor;
	mpz_inits(divident, divisor, NULL);
	mpz_import(divident, 1, -1, 32, 
		-1, 
		0, n);
	mpz_import(divisor, 1, -1, 32, 
		-1, 
		0, d);
	
	mpf_t a, b, rop;
	mpf_set_default_prec(256);
	
	mpf_inits(a, b, rop, NULL);
	mpf_set_z(a, divident);
	mpf_set_z(b, divisor);
	mpf_div(rop, a, b);
	
	double result = mpf_get_d(rop);
	mpz_clears(divident, divisor, NULL);
	mpf_clears(a, b, rop, NULL);
	return result;
}


#if defined(_TEST_COMPACT_INT) && defined(_STAND_ALONE)

#ifndef dump_line
#define dump_line(prefix, data, size) do { 	\
		fprintf(stderr, "%s", prefix);		\
		for(ssize_t i = 0; i < size; ++i) fprintf(stderr, "%.2x", ((unsigned char*)data)[i]);	\
		fprintf(stderr, "\n");				\
	} while(0)
#endif

#include <stdint.h>
int main(int argc, char **argv)
{
	// (000000000000000000000000000000000000000000000000) CB0404 0000000000
	static const uint256_t TARGET = {
		.val = {
			[24] = 0xcb,
			[25] = 0x04,
			[26] = 0x04,	
		},
	};
	static const uint32_t BITS = 0x1b0404cb;
	compact_uint256_t cint = {.bits = 0x1b0404cb };
	
	printf("== test1: compact_int_to_uint256() ...\n");
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	
	uint256_t target = compact_to_uint256(&cint);
	dump_line("\ttarget: ", &target, 32);
	dump_line("\tTARGET: ", &TARGET, 32);
	
	assert(0 == memcmp(&target, &TARGET, 32));
	
	printf("== test2: uint256_to_compact_int() ...\n");
	cint = uint256_to_compact(&TARGET);
	
	dump_line("\tTARGET: ", &TARGET, 32);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	
	assert(cint.bits == BITS);
	
	
	static const uint32_t BITS_test_3_4 = 0x1e0004cb;
	static const uint256_t TARGET_test_3_4 = {
		.val = {
			[28] = 0xcb,
			[29] = 0x04,
		},
	};
	printf("== test3: compact_int_to_uint256(cint=0x%.8x) ...\n", BITS_test_3_4);
	cint.bits = 0;
	cint.exp = 30;
	cint.mantissa[0] = 0xcb;
	cint.mantissa[1] = 0x04;
	
	target = compact_to_uint256(&cint);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	dump_line("\ttarget: ", &target, 32);
	assert(0 == memcmp(&target, &TARGET_test_3_4, 32));

	printf("== test4: uint256_to_cint() ==> cint=0x%.8x ...\n", BITS_test_3_4);
	cint.bits = 0;
	cint = uint256_to_compact(&target);
	dump_line("\ttarget: ", &target, 32);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	assert(cint.bits == BITS_test_3_4);
	
	
	static const uint32_t BITS_test_5_6 = 0x020004cb;
	static const uint256_t TARGET_test_5_6 = {
		.val = {
			[0] = 0xcb,
			[1] = 0x04,
		},
	};
	
	printf("== test5: compact_int_to_uint256(cint=0x%.8x) ...\n", BITS_test_5_6);
	cint.bits = 0;
	cint.exp = 2;
	cint.mantissa[0] = 0xcb;
	cint.mantissa[1] = 0x04;
	
	target = compact_to_uint256(&cint);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	dump_line("\ttarget: ", &target, 32);
	assert(0 == memcmp(&target, &TARGET_test_5_6, 32));

	
	printf("== test6: uint256_to_cint() ==> cint=0x%.8x...\n", BITS_test_5_6);
	cint.bits = 0;
	cint = uint256_to_compact(&target);
	dump_line("\ttarget: ", &target, 32);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	assert(cint.bits == BITS_test_5_6);
	
	
	static const uint32_t BITS_test_7_8 = 0x1d00ffff;	// difficulty one
	static const uint256_t TARGET_test_7_8 = {
		.val = {
			[26] = 0xff,
			[27] = 0xff,
		},
	};
	printf("== test7: compact_int_to_uint256(cint=0x%.8x) ...\n", BITS_test_7_8);
	cint.bits = BITS_test_7_8;
	target = compact_to_uint256(&cint);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	dump_line("\ttarget: ", &target, 32);
	assert(0 == memcmp(&target, &TARGET_test_7_8, 32));

	
	printf("== test8: uint256_to_cint() ==> cint=0x%.8x...\n", BITS_test_7_8);
	cint = uint256_to_compact(&target);
	dump_line("\ttarget: ", &target, 32);
	printf("\tcint    : 0x%.8x\n", cint.bits); 
	dump_line("\tcintdata:   ", &cint, sizeof(cint));
	assert(cint.bits == BITS_test_7_8);
	
	/* 
	 * test bdiff
	 * 0x00000000FFFF0000000000000000000000000000000000000000000000000000 / 0x00000000000404CB000000000000000000000000000000000000000000000000 
	 * = 16307.420938523983 (bdiff)
	 */
	cint.bits = BITS;	// 0x1b0404cb;
	
	double result = compact_uint256_div(&compact_uint256_difficulty_one, &cint);
	printf("bdiff: %.8f\n", result);
	
	
	/*
	 * test pdiff: 
	 * 0x00000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF / 0x00000000000404CB000000000000000000000000000000000000000000000000 
	 * = 16307.669773817162 (pdiff)
	 */
	target = TARGET;
	uint256_t difficulty_one = uint256_difficulty_one;
	result = uint256_div(&difficulty_one, &target);
	dump_line("\tDIFF_ONE: ", &difficulty_one, 32);
	dump_line("\tTARGET: ", &target, 32);
	printf("pdiff: %.8f\n", result);
	
	return 0;
}
#endif

