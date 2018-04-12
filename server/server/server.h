#ifndef HGA_SERVER
#define HGA_SERVER

#include <linux/slab.h>
#include <linux/list.h>
#include "../../comm/comm.h"
#include "../hashtable/hashtable.h"


struct vaddr_arg {
        unsigned long vaddr;
        pid_t pid;
};


void handle_request(struct comm_message *);
int handle_request_write(struct comm_message *res);
int handle_initial_read(struct comm_message *msg);
int handle_commit_page(struct comm_message *msg);

void send_resume_read_request(struct client_hashtable_entry *client);
void send_allow_write_request(struct client_hashtable_entry *client);
void send_read_lock_request(struct client_hashtable_entry* ip);

void send_response(struct comm_message*);
static void find_mapped_machines(pid_t pid, unsigned long vaddr, struct vaddr_hashtable_entry* entry);
//static void find_mapped_pid(unsigned long token, struct pid_hashtable_entry* entry);
void vaddr_hashtable_lookup_callback(void* current_entry, void* entry, void* arg);
//void pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg);

#endif
