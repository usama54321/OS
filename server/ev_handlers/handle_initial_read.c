#include "ev_handlers.h"

/*
 * Initial request to read page
 */
comm_ackcode_t handle_initial_read(struct comm_ctx *ctx, unsigned long vaddr,
        pid_t client_pid, pid_t token, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock) {

    //TODO handle if page already locked
    unsigned long pfn;
    struct mapped_page* pf_entry;
    struct client_entry *client;

    pfn = PAGE_MASK & vaddr;
    pf_entry = NULL;

    find_mapped_machines(token, pfn, pf_entry);

    client = make_client_entry(conn_sock, pgd, client_pid);

    if(!client)
        return ACKCODE_OP_FAILURE;

    if (!pf_entry) {
        //first request for page. No updated page to send
        pf_entry = make_mapped_page(pfn, token, false, NULL); 

        if (!pf_entry)
            return ACKCODE_OP_FAILURE;

        pf_entry->clients = client;
        add_mapped_page(pf_entry);
    }else {
        // add to list of reading
        if (pf_entry->locked) {
            kfree(client);
            return ACKCODE_OP_FAILURE;
        }

        add_client_entry(client, pf_entry->clients);
    }

    comm_resume_read(ctx, conn_sock, vaddr, client_pid, pgd, pf_entry->page_addr);
    return ACKCODE_INITIAL_READ;
}

