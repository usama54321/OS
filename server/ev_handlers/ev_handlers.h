#include "../comm/comm.h"
#include "../hashtable/hashtable.h"

struct vaddr_arg {
        unsigned long pfn;
        pid_t token;
};

comm_ackcode_t handle_request_write(struct comm_ctx *ctx, unsigned long vaddr,
        pid_t client_pid, pid_t server_pid, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock);

comm_ackcode_t handle_initial_read(struct comm_ctx *ctx, unsigned long vaddr,
        pid_t client_pid, pid_t server_pid, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock);

comm_ackcode_t handle_commit_page(struct comm_ctx *ctx, unsigned long vaddr,
        pid_t client_pid, pid_t server_pid, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *conn_sock);


void find_mapped_machines(pid_t token, unsigned long vaddr, struct mapped_page* entry);
void mapped_page_lookup_callback(void* current_entry, void* entry, void* arg);
