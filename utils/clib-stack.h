#ifndef CHLIB_STACK_H_
#define CHLIB_STACK_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
struct clib_node;
#define clib_node_get_data(p_node) *(void **)(p_node)

typedef struct clib_stack_or_queue
{
	struct clib_node * top;
	struct clib_node * bottom;
	int count;
	
	int (* push)(struct clib_stack_or_queue * sq, void * data);
	void * (* pop)(struct clib_stack_or_queue * sq);
	
	// cleanup callback
	void (* on_free_data)(void * node_data);
}clib_stack_t, clib_queue_t;
struct clib_stack_or_queue * clib_stack_or_queue_init(struct clib_stack_or_queue * sq, int is_queue);
void clib_stack_or_queue_cleanup(struct clib_stack_or_queue * sq);

#define clib_stack_init(sq) 	clib_stack_or_queue_init(sq, 0)
#define clib_queue_init(sq) 	clib_stack_or_queue_init(sq, 1)
#define clib_stack_cleanup(sq) 	clib_stack_or_queue_cleanup(sq)
#define clib_queue_cleanup(sq) 	clib_stack_or_queue_cleanup(sq)



#ifdef __cplusplus
}
#endif
#endif

