#include <linux/hashtable.h>
#include "../../comm/comm.h"

/*
 * Mock function to handle data passed from socket from client
 *
 */

struct comm_msg* handle_request(struct comm_msg *msg) {
    struct comm_msg* res;
    response = (struct comm_msg*) kmalloc(sizeof(struct comm_msg), GFP_KERNEL);

    if (msg->mcode == OPCODE_REQUEST_WRITE) {
        handle_write_request(response);
    }

    return response;
}

/*
 * lookup remote pid in hashtable
 */

pid_t unmap_pid(pid_t remote_pid) {
    return 0;
}

int handle_write_request(struct comm_msg *res) {
    //perform server side validation? Already done on client
    int mapped,
    struct task_struct *task,
    pid_t local_pid,
    const unsigned long pf_vaddr,
    const int *ips;

    pf_vaddr = res->vaddr
    local_pid = unmap_pid(res->pid); 

    if (!local_pid)
        return 0;
    
    task = find_task_by_vpid(local_pid);
    
    if(!task)
        return 0;
     
    ips = find_mapped_machines(task, res->vaddr); 
}
