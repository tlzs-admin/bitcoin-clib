/*
 * clib-stack.c
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

#include "clib-stack.h"
#include "utils.h"

/*************************************
 * stack
 ************************************/
struct clib_node
{
	void * data;
	struct clib_node * next;
};

static void * stack_pop(struct clib_stack_or_queue * sq)
{
	struct clib_node * node = sq->top;
	if(NULL == node) return NULL;

	sq->top = node->next;
	void * data = node->data;
	free(node);
	--sq->count;
	
	//~ debug_printf("data=%ld, count=%d, top=%p", (long)data, sq->count, sq->top);
	return data;
}

static int stack_push(struct clib_stack_or_queue * sq, void * data)
{
	struct clib_node * node = calloc(1, sizeof(*node));
	assert(node);
	node->data = data;
	node->next = sq->top;
	sq->top = node;
	
	++sq->count;
	
	//~ debug_printf("data=%ld, count=%d, top=%p", (long)data, sq->count, sq->top);
	return 0;
}

static void * queue_pop(struct clib_stack_or_queue * sq)
{
	struct clib_node * node = sq->top;
	if(NULL == node) return NULL;
	
	sq->top = node->next;
	if(NULL == sq->top) sq->bottom = NULL;
	
	void * data = node->data;
	free(node);
	
	--sq->count;
	
	//~ debug_printf("data=%ld, count=%d, top=%p", (long)data, sq->count, sq->top);
	return data;
}

static int queue_push(struct clib_stack_or_queue * sq, void * data)
{
	struct clib_node * node = calloc(1, sizeof(*node));
	assert(node);
	node->data = data;
	
	if(NULL == sq->top) sq->top = sq->bottom = node;
	else {
		sq->bottom->next = node;
		sq->bottom = node;
	}
	++sq->count;
	//~ debug_printf("data=%ld, count=%d, top=%p", (long)data, sq->count, sq->top);
	return 0;
}


struct clib_stack_or_queue * clib_stack_or_queue_init(struct clib_stack_or_queue * sq, int is_queue) 
{
	if(NULL == sq) sq = calloc(1, sizeof(*sq));
	else memset(sq, 0, sizeof(*sq));
	
	sq->push = is_queue?queue_push:stack_push;
	sq->pop = is_queue?queue_pop:stack_pop;
	
	return sq;
}

void clib_stack_or_queue_cleanup(struct clib_stack_or_queue * sq)
{
	struct clib_node * node = NULL;
	while((node = sq->top)) {
		sq->top = node->next;
		if(sq->on_free_data) sq->on_free_data(node->data);
		free(node);
	}
	sq->bottom = NULL;
	sq->count = 0;
}

#if defined(_TEST_CLIB_STACK) && defined(_STAND_ALONE)
int main(int argc, char ** argv)
{
	void * data = NULL;
	clib_stack_t stack[1], queue[1];
	clib_stack_init(stack);
	clib_queue_init(queue);
	
	// the index MUST NOT start from 0, since (0) cannot be distinguished from (NULL) in this test
	// test stack: 
	
	#define NUM_ITEMS (10)
	printf("\n== push to stack: [ ");
	for(int i = 1; i <= NUM_ITEMS; ++i) {	
		int rc = stack->push(stack, (void *)(long)i);
		assert(0 == rc);
		printf("%d%s", i, (i<NUM_ITEMS)?",":" ");
	}
	printf("]\n");
	
	printf("== verify stack ==\n");
	int value = NUM_ITEMS;	// last item
	while((data = stack->pop(stack))) {
		printf("expect: %.2d, data: %.2d ==> [%s]\n", 
			value, 
			(int)(long)data, 
			(value == (int)(long)data)?"\e[32mOK\e[39m":"\e[34m[NG]\e[39m"
		);
		assert(value == (int)(long)data);
		--value;
	}
	assert(stack->count == 0 && NULL == stack->top);
	
	// test queue
	printf("\n== push to queue: [ ");
	for(int i = 1; i <= NUM_ITEMS; ++i) {
		int rc = queue->push(queue, (void *)(long)i);
		assert(0 == rc);
		printf("%d%s", i, (i<NUM_ITEMS)?",":" ");
	}
	printf("]\n");
	
	printf("== verify queue ==\n");
	value = 1;	// first item
	while((data = queue->pop(queue))) {
		printf("expect: %.2d, data: %.2d ==> [%s]\n", 
			value, 
			(int)(long)data, 
			(value == (int)(long)data)?"\e[32mOK\e[39m":"\e[34m[NG]\e[39m"
		);
		assert(value == (int)(long)data);
		++value;
	}
	assert(queue->count == 0 && NULL == queue->top);
	
	return 0;
}
#endif
