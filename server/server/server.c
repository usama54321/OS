#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "server.h"
#include "../pgtable/pgtable.h"


/*
 * Mock function to handle data passed from socket from client
 *
 */

void handle_request(struct comm_message *msg) {
    if (msg->hdr.mcode.op.code == OPCODE_REQUEST_WRITE.code) {
        handle_request_write(msg);
    }else if(msg->hdr.mcode.op.code == OPCODE_INITIAL_READ.code) {
        handle_initial_read(msg);    
    }else if(msg->hdr.mcode.op.code == OPCODE_COMMIT_PAGE.code) {
        handle_commit_page(msg);
    }
}

/*
 * Initial request to read page
 */
int handle_initial_read(struct comm_message *msg) {
    //TODO handle if page already locked
    pid_t local_pid;
    unsigned long pfn_addr;
    struct vaddr_hashtable_entry* htable_entry;
    struct task_struct* task;
    struct comm_message* res;

    local_pid = msg->hdr.server_pid;
    pfn_addr = PAGE_MASK & msg->hdr.vaddr;

    task = find_task_by_vpid(local_pid);

    if(!task)
        return 0;

    htable_entry = NULL;
    find_mapped_machines(local_pid, pfn_addr, htable_entry);

    if (!htable_entry) {
        //first request for page
        struct vaddr_hashtable_entry* entry = make_vaddr_hashtable_entry(pfn_addr, local_pid, false, 0, msg->hdr.pgd); 
        add_vaddr_hashtable_entry(entry);

    }else {
        // add to list of reading
        struct client_hashtable_entry* client = kmalloc(sizeof(*client), GFP_KERNEL);
        add_client_hashtable_entry(client, htable_entry->clients);
    }

    res = kmalloc(sizeof(*res) + PAGE_SIZE, GFP_KERNEL); 

    res->hdr.mcode.ack = ACKCODE_INITIAL_READ;
    res->hdr.payload_len = PAGE_SIZE;
    res->hdr.pid = msg->hdr.pid;
    get_page_data(task->mm->pgd, msg->hdr.vaddr, msg->data.payload);

    send_response(res);
    return 1;
}

int handle_commit_page(struct comm_message *msg) {
    pid_t local_pid;
    unsigned long pfn_addr;
    struct vaddr_hashtable_entry *htable_entry;
    struct task_struct* task;
    struct client_hashtable_entry *clients;

    task = NULL;
    local_pid = msg->hdr.server_pid; 
    pfn_addr = PAGE_MASK & msg->hdr.vaddr;

    task = find_task_by_vpid(local_pid);
    if(!task)
        return 0;

    //write page
    set_page_data(task->mm->pgd, msg->hdr.vaddr, msg->data.payload);

    //find hashtable entry
    find_mapped_machines(local_pid, pfn_addr, htable_entry);

    lock_vaddr_hashtable();
    htable_entry->locked = false;

    //send resume read requests
    list_for_each_entry(clients, &(htable_entry->clients->list), list) {
        send_resume_read_request(clients);
    }

    unlock_vaddr_hashtable();
    return 1;
}

int handle_request_write(struct comm_message *msg) {
    //perform server side validation? Already done on client
    pid_t local_pid;
    unsigned long pfn_addr;
    struct vaddr_hashtable_entry *htable_entry;
    struct client_hashtable_entry *clients;

    local_pid = msg->hdr.server_pid; 
     
    pfn_addr = PAGE_MASK & msg->hdr.vaddr;

    htable_entry = NULL;
    find_mapped_machines(local_pid, pfn_addr, htable_entry); 
    
    //page not requested before
    if(!htable_entry) {
        // TODO set ip
        struct vaddr_hashtable_entry* entry = make_vaddr_hashtable_entry(pfn_addr, local_pid, true, 0, msg->hdr.pgd); 
        add_vaddr_hashtable_entry(entry);
        send_allow_write_request(entry->clients);
        return 1;
    }

    // TODO Locking necessary on read?
    lock_vaddr_hashtable();
    htable_entry->locked = true; 

    list_for_each_entry(clients, &(clients->list), list) {
        send_read_lock_request(clients);
    }

    unlock_vaddr_hashtable();
    return 1;
}

void send_resume_read_request(struct client_hashtable_entry *client) {
    //TCP code
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

void send_response(struct comm_message *res) {
    //TCP code here
}
/*
 * will later be replaced to use actual hashtable instead of list

static void find_mapped_pid(unsigned long token, struct pid_hashtable_entry* entry) {
    foreach_pid_hashtable(pid_hashtable_lookup_callback, (void*)entry, (void*)(&token));
}
*/

static void find_mapped_machines(pid_t pid, unsigned long vaddr, struct vaddr_hashtable_entry* entry) {
    struct vaddr_arg argument = {
        .vaddr=vaddr,
        .pid=pid
    };
    foreach_vaddr_hashtable(vaddr_hashtable_lookup_callback, (void*) (&entry), (void*) (&argument)); 
}

void vaddr_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    struct vaddr_hashtable_entry* curr = (struct vaddr_hashtable_entry*)current_entry;
    struct vaddr_arg* argument = (struct vaddr_arg*)arg;

    if(curr->pfn_addr == argument->vaddr && curr->pid == argument->pid) {
        struct vaddr_hashtable_entry** temp = (struct vaddr_hashtable_entry**)found;
        *temp = (struct vaddr_hashtable_entry*) current_entry;
        //mark page as read locked
    }
}

/*
void pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    if(((struct pid_hashtable_entry*)current_entry)->token == *((unsigned long*)arg)) {
        struct pid_hashtable_entry** temp = (struct pid_hashtable_entry**)found;
        *temp = (struct pid_hashtable_entry*) current_entry;

        found = current_entry;
    }
}
*/
