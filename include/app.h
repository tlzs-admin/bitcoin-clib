#ifndef SPV_NODE_APP_H_
#define SPV_NODE_APP_H_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif


#include "spv-node.h"
#include "block_hdrs-db.h"
#include "chains.h"

typedef struct app_context
{
	void * priv;
	void * user_data;
	
	void * db_env;
	spv_node_context_t spv[1];
	block_headers_db_t hdrs_db[1];
	
}app_context_t;

app_context_t * app_context_init(app_context_t * app, void * user_data);
void app_context_cleanup(app_context_t * app);
#define app_context_new(user_data) app_context_init(NULL, user_data)
#define app_context_free(app) do { if(app) { app_context_cleanup(app); free(app); } } while(0)

int app_context_parse_args(app_context_t * app, int argc, char ** argv);
int app_run(app_context_t * app);
int app_stop(app_context_t * app);

#ifdef __cplusplus
}
#endif
#endif
