#include "ev_handlers.h"

comm_ackcode_t handle_commit_page(struct comm_ctx *ctx, unsigned long vaddr,
        pid_t client_pid, pid_t token, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock) {
    unsigned long pfn;
    struct mapped_page *pf_entry;
    struct client_entry *client;

    pfn = PAGE_MASK & vaddr;

    pf_entry = NULL;
    //find hashtable entry
    find_mapped_machines(token, pfn, pf_entry);

    if (!pf_entry) {
        printk(KERN_ERR "commit mapped page not found");
        return ACKCODE_OP_FAILURE;
    }
    
    if (!pf_entry->locked)
        return ACKCODE_OP_FAILURE;

    if (pf_entry->page_addr)
        kfree(pf_entry->page_addr);

    pf_entry->page_addr = kmalloc(PAGE_SIZE, GFP_KERNEL);

    memcpy(pf_entry->page_addr, pagedata, PAGE_SIZE);

    pf_entry->locked = false;

    //send resume read requests
    list_for_each_entry(client, &(pf_entry->clients->list), list) {
        if (client->socket == conn_sock)
            continue;
        comm_resume_read(ctx, conn_sock, vaddr, client_pid, pgd, pf_entry->page_addr);
    }

    return ACKCODE_COMMIT_PAGE;
}
