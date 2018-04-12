#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "server.h"


/*
 * Mock function to handle data passed from socket from client
 *
 */

void handle_request(struct comm_message *msg) {
    if (msg->hdr.mcode.op.code == OPCODE_REQUEST_WRITE.code) {
        handle_request_write(msg);
    }else if(msg->hdr.mcode.op.code == OPCODE_ALLOW_WRITE.code) {
        handle_allow_write(msg);
    }
}

int handle_allow_write(struct comm_message *msg) {
    return 1;
}

int handle_request_write(struct comm_message *msg) {
    //perform server side validation? Already done on client
    struct task_struct *task;
    pid_t local_pid;
    unsigned long pfn_addr;
    struct client_hashtable_entry *clients;
    struct pid_hashtable_entry* pid_entry;

    pid_entry = NULL;

    find_mapped_pid(msg->hdr.token, pid_entry);

    if (!pid_entry)
        return 0;

    local_pid = pid_entry->pid; 
    task = find_task_by_vpid(local_pid);
    
    if(!task)
        return 0;
     
    pfn_addr = PAGE_MASK & msg->hdr.vaddr;
    find_mapped_machines(pid_entry->pid, pfn_addr, clients); 
    
    //page not requested before
    if(!clients) {
        // TODO set ip
        struct vaddr_hashtable_entry* entry = make_vaddr_hashtable_entry(pfn_addr, local_pid, 1, 0, msg->hdr.pgd); 
        add_vaddr_hashtable_entry(entry);
        send_allow_write_request(entry->clients);
        return 1;
    }

    list_for_each_entry(clients, &(clients->list), list) {
        send_read_lock_request(clients);
    }

    return 1;
}


void send_allow_write_request(struct client_hashtable_entry *client) {
    //TCP code
}

/*
 * Send read lock requests. Without TCP assumption that locks have been acquired after
 * return of this function
 */
void send_read_lock_request(struct client_hashtable_entry* client) {
    //TCP code
}

/*
 * will later be replaced to use actual hashtable instead of list
 */

static void find_mapped_pid(unsigned long token, struct pid_hashtable_entry* entry) {
    foreach_pid_hashtable(pid_hashtable_lookup_callback, (void*)entry, (void*)(&token));
}

static void find_mapped_machines(pid_t pid, unsigned long vaddr, struct client_hashtable_entry* entry) {
    struct vaddr_arg argument = {
        .vaddr=vaddr,
        .pid=pid
    };
    foreach_vaddr_hashtable(vaddr_hashtable_lookup_callback, (void*) entry, (void*) (&argument)); 
}

void vaddr_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    struct vaddr_hashtable_entry* curr = (struct vaddr_hashtable_entry*)current_entry;
    struct vaddr_arg* argument = (struct vaddr_arg*)arg;

    if(curr->pfn_addr == argument->vaddr && curr->pid == argument->pid) {
        struct vaddr_hashtable_entry** temp = (struct vaddr_hashtable_entry**)found;
        *temp = (struct vaddr_hashtable_entry*) current_entry;
        //mark page as read locked
        (*temp)->locked = true;
    }
}

void pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    if(((struct pid_hashtable_entry*)current_entry)->token == *((unsigned long*)arg)) {
        struct pid_hashtable_entry** temp = (struct pid_hashtable_entry**)found;
        *temp = (struct pid_hashtable_entry*) current_entry;

        found = current_entry;
    }
}
