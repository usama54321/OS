#ifndef HGA_HASH
#define HGA_HASH

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/types.h>

typedef void (*callBackFunc)(void*, void*, void* );

struct pid_hashtable_entry {
    struct list_head list;
    unsigned long token;
    pid_t pid;
};

struct client_hashtable_entry {
    struct list_head list;
    unsigned long ip;
    unsigned long pgd;
};

struct vaddr_hashtable_entry {
    struct list_head list;
    unsigned long pfn_addr;
    pid_t pid;
    struct client_hashtable_entry* clients;
    bool locked;
};

int hashtables_init(void);
void add_pid_hashtable_entry(struct pid_hashtable_entry* entry);
void add_vaddr_hashtable_entry(struct vaddr_hashtable_entry* entry);
void foreach_pid_hashtable(callBackFunc, void*, void* );
void foreach_vaddr_hashtable(callBackFunc, void*, void*);
struct vaddr_hashtable_entry* make_vaddr_hashtable_entry(unsigned long pfn, pid_t pid, bool locked, unsigned long client_ip, unsigned long client_pgd);

#endif
