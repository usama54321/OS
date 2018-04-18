#include "ev_handlers.h"

comm_ackcode_t handle_request_write(struct comm_ctx *ctx, unsigned long vaddr, 
        pid_t client_pid, pid_t token, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock)
 {
    //TODO handle locked page
    unsigned long pfn;
    struct mapped_page *pf_entry;
    struct client_entry *client;
     
    pfn = PAGE_MASK & vaddr;

    pf_entry = NULL;
    find_mapped_machines(token, pfn, pf_entry); 

    if(!pf_entry) {
        //must make initial read first
        return ACKCODE_OP_FAILURE;
    } else {
        struct client_entry *temp_client;
        if(pf_entry->locked)
            return ACKCODE_OP_FAILURE;

        client = NULL;

        list_for_each_entry(temp_client, &(pf_entry->list), list) {
            if (temp_client->socket == conn_sock) {
                client = temp_client;
                break;
            }
        }
    }

    if(!client)
        return ACKCODE_OP_FAILURE;

    //mark page as locked
    pf_entry->locked = true; 

    list_for_each_entry(client, &(client->list), list) {
        if (client->socket == conn_sock)
            continue;
        if(comm_lock_read(ctx, client->socket, vaddr, client_pid, pgd) == -1)
            return ACKCODE_OP_FAILURE;
    }

    if(comm_allow_write(ctx, conn_sock, vaddr, client_pid, pgd) == -1)
        return ACKCODE_OP_FAILURE;

    return ACKCODE_REQUEST_WRITE;
}
