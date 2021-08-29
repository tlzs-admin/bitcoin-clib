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

#include "utils.h"

compact_uint256_t uint256_to_compact(const uint256_t * target)
{
	compact_uint256_t cint = { .bits = 0 };
	
	int num_zeros = 0;
	const unsigned char * p_end = (const unsigned char *)target + 32;
	
	// count the number of 0s at the end
	while(num_zeros < 32 && (p_end[-1] == 0)) { --p_end; ++num_zeros;}
	
	//~ debug_printf("num_zeros: %d", num_zeros);
	if(num_zeros == 32) { 
		return (compact_uint256_t){0};
	}

	// make sure that the mantissa represents a positive value 
	if((p_end[-1]  & 0x80)) { // (int)mantissa < 0
		if(num_zeros == 0) { 
			/*
			 * out of the range that a compact_uint256 can represent.
			 * (the highest bit of the mantissa is '1' and cannot borrow '0' from uint256)
			 */
			return compact_uint256_NaN; 
		}
		++p_end;
		--num_zeros;	// borrow a '0' from uint256
	}
	
	int num_bytes = 3;	// 24 bits
	cint.exp = 32 - num_zeros;
	debug_printf("exp: %d (0x%.2x)", (int)cint.exp, (int)cint.exp);
	
	if((cint.exp + num_bytes) >= 32) num_bytes = 32 - cint.exp; 
	if((num_zeros + num_bytes) >= 32) num_bytes = 32 - num_zeros;
	
	if(num_bytes > 0) {
		memcpy(cint.mantissa, (p_end - num_bytes), num_bytes);
	}
	return cint;
}
uint256_t compact_to_uint256(const compact_uint256_t * cint)
{
	uint256_t target = { 0 };
	if(cint->exp > 32) return uint256_NaN;

	int num_bytes = 3;
	int num_zeros = 32 - cint->exp;
	
	if(cint->exp < 3) num_bytes = cint->exp;
	if((num_zeros + num_bytes) >= 32) num_bytes = 32 - num_zeros;
//	debug_printf("num_zeros: %d, num_bytes: %d, exp: 0x%.2x", num_zeros, num_bytes, (int)cint->exp);
	
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

double compact_uint256_div(const compact_uint256_t * restrict n, const compact_uint256_t * restrict d)
{
	if((0 == (d->bits & 0x0FFFFFF)) || (d->exp >= 32) || (d->exp == 0)) return NAN;	
	if((0 == (n->bits & 0x0FFFFFF)) || (n->exp >= 32) || (n->exp == 0)) return 0.0;
	
	double a = (double)( n->bits & 0x0FFFFFF);
	double b = (double)(d->bits & 0x0FFFFFF);

	int exponent_diff = ((int)n->exp - (int)d->exp) * 8;	// bytes to bits
	double coef = pow(2.0, exponent_diff);
	
	return a / b * coef;
}

double uint256_div(const uint256_t * restrict n, const uint256_t * restrict d)
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

int compact_uint256_compare(const compact_uint256_t * restrict a, const compact_uint256_t * restrict b)
{
	int value_a = a->bits & 0x0ffffff;
	int value_b = b->bits & 0x0ffffff;
	int exp_a = a->exp;
	int exp_b = b->exp;
	
	// check whether or not a '0' was appended to the compact_uint
	if(a->mantissa[2] == 0 && a->mantissa[1] & 0x80)
	{
		value_a <<= 8;
		--exp_a;
	}
	if(b->mantissa[2] == 0 && b->mantissa[1] & 0x80)
	{
		value_b <<= 8;
		--exp_b;
	}
	
	if(exp_a == exp_b) return value_a - value_b;
	return exp_a - exp_b;
}

int uint256_compare(const uint256_t * restrict  _a, const uint256_t * restrict _b)
{
	// treat uint256 as little-endian
	uint64_t * a = (uint64_t *)_a;
	uint64_t * b = (uint64_t *)_b;
	for(int i = 3; i >= 0; --i)
	{
		if(a[i] == b[i]) continue;
		return (a[i] > b[i])?1:-1;
	}
	return 0;
}

int uint256_compare_with_compact(const uint256_t * restrict hash, const compact_uint256_t * restrict _target)
{
	uint256_t target = compact_to_uint256(_target);
	return uint256_compare(hash, &target);
}


#define UINT256_BYTE_ORDER (1) 	// big-endian
void uint256_add(uint256_t * c, const uint256_t *a, const uint256_t *b)
{
	assert(a && b && c);
	memset(c, 0, sizeof(*c));
	
	mpz_t ma, mb, mc;
	mpz_inits(ma, mb, mc, NULL);
	
	mpz_import(ma, 1, UINT256_BYTE_ORDER, sizeof(*a), 0, 0, a);
	mpz_import(mb, 1, UINT256_BYTE_ORDER, sizeof(*b), 0, 0, b);
	mpz_add(mc, ma, mb);
	
	size_t count = 0;
	mpz_export(c, &count, UINT256_BYTE_ORDER, sizeof(*c), 0, 0, mc);
	assert(count == 1);
	return;
}


compact_uint256_t compact_uint256_add(const compact_uint256_t a, const compact_uint256_t b)
{
	uint256_t ua, ub, uc;
	memset(&uc, 0, sizeof(uc));
	
	ua = compact_to_uint256(&a);
	ub = compact_to_uint256(&b);
	
	dump_line("ua: ", &ua, sizeof(ua));
	dump_line("ub: ", &ub, sizeof(ub));
	
	uint256_add(&uc, &ua, &ub);
	dump_line("uc: ", &uc, sizeof(uc));
	
	return uint256_to_compact(&uc);
} 

/**
 * https://en.bitcoin.it/wiki/Difficulty
**/

static inline double calc_difficulty(const compact_uint256_t * cint, const uint256_t * difficulty_one)
{
	uint256_t u_target = compact_to_uint256(cint);
	double difficulty = 1.0;
	
	//~ dump_line("one   : ", difficulty_one, 32);
	//~ dump_line("target: ", &u_target, 32);
	
	mpz_t m_one, m_target;
	mpz_inits(m_one, m_target, NULL);
	mpz_import(m_one, 1, -1, sizeof(uint256_t), 0, 0, difficulty_one);
	mpz_import(m_target, 1, -1, sizeof(uint256_t), 0, 0, &u_target);
	
	mpf_t one, target, bdiff;
	mpf_inits(one, target, bdiff, NULL); 
	mpf_set_z(one, m_one);
	mpf_set_z(target, m_target);
	mpf_div(bdiff, one, target);
	
	difficulty = mpf_get_d(bdiff);
	
	mpf_clears(one, target, bdiff, NULL);
	mpz_clears(m_one, m_target, NULL);
	return difficulty;
}

double compact_uint256_to_bdiff(const compact_uint256_t * cint)
{
	static const uint256_t bitcoin_difficulty_one = {{
		0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0xFF, 0xFF,  0x00, 0x00, 0x00, 0x00, 
	}};
	
	debug_printf("==== %s() ====", __FUNCTION__);
	return calc_difficulty(cint, &bitcoin_difficulty_one);
	
}
double compact_uint256_to_pdiff(const compact_uint256_t * cint)
{
	static const uint256_t pool_difficulty_one = {{
		0XFF, 0XFF, 0XFF, 0XFF,  0XFF, 0XFF, 0XFF, 0XFF, 
		0XFF, 0XFF, 0XFF, 0XFF,  0XFF, 0XFF, 0XFF, 0XFF, 
		0XFF, 0XFF, 0XFF, 0XFF,  0XFF, 0XFF, 0XFF, 0XFF, 
		0xFF, 0xFF, 0xFF, 0xFF,  0x00, 0x00, 0x00, 0x00, 
	}};
	
	debug_printf("==== %s() ====", __FUNCTION__);
	return calc_difficulty(cint, &pool_difficulty_one);
}

#undef UINT256_BYTE_ORDER
