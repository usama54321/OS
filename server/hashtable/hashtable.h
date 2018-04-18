#ifndef HGA_HASH
#define HGA_HASH

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/types.h>

typedef void (*callBackFunc)(void*, void*, void* );

struct client_entry {
    struct list_head list;
    struct socket *socket;
    pgd_t* pgd;
    pid_t pid;
};

struct mapped_page {
    struct list_head list;
    unsigned long pfn;
    void *page_addr;
    pid_t token;
    struct client_entry* clients; //list of mapped clients
    bool locked;
};

int hashtable_init(void);

void add_mapped_page(struct mapped_page* entry);
void add_client_entry(struct client_entry* entry, struct client_entry* existing);

void foreach_mapped_page(callBackFunc, void*, void*);
struct mapped_page* make_mapped_page(unsigned long pfn, pid_t pid, bool locked, void *page_addr);
struct client_entry* make_client_entry(struct socket *sock, pgd_t *pgd, pid_t client_pid);
#endif
