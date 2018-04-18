#include "server.h"

int init_server(void) {
    struct comm_ctx *ctx = comm_ctx_new();

    if(!ctx)
        return 0;
     
    attach_handlers(ctx);

    if(!comm_run(ctx))
        return 0;

    return 1;
}

void attach_handlers(struct comm_ctx* ctx) {
    comm_register_handler(ctx, OPCODE_INITIAL_READ, handle_initial_read, NULL);
}


/*
 * will later be replaced to use actual hashtable instead of list

static void find_mapped_pid(unsigned long token, struct pid_hashtable_entry* entry) {
    foreach_pid_hashtable(pid_hashtable_lookup_callback, (void*)entry, (void*)(&token));
}
*/



/*
void pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    if(((struct pid_hashtable_entry*)current_entry)->token == *((unsigned long*)arg)) {
        struct pid_hashtable_entry** temp = (struct pid_hashtable_entry**)found;
        *temp = (struct pid_hashtable_entry*) current_entry;

        found = current_entry;
    }
}
*/
