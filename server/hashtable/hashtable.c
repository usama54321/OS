#include "hashtable.h"

/*
 * locks for hashtables
static DEFINE_SPINLOCK(pid_lock);
static DEFINE_SPINLOCK(vaddr_lock);
 */

static struct mapped_page *pf_list;

int hashtable_init(void) {
    pf_list = (struct mapped_page*) kmalloc(sizeof(struct mapped_page), GFP_KERNEL);

    if (!pf_list)
        return 0;
    memset(pf_list, 0, sizeof(struct mapped_page));
    INIT_LIST_HEAD(&(pf_list->list));

    return 1;
}

void add_mapped_page(struct mapped_page* entry) {
    list_add_tail(&(entry->list), &(pf_list->list));
}

void add_client_entry(struct client_entry* entry, struct client_entry* existing) {
    list_add_tail(&(entry->list), &(existing->list));
    return;
}

void foreach_mapped_page(callBackFunc func, void* entry, void* arg) {
    struct mapped_page* temp;
    list_for_each_entry(temp, &(pf_list->list), list) {
        func((void*)temp, entry, arg);
    }
}

struct client_entry* make_client_entry(struct socket *sock, pgd_t *pgd, pid_t pid_client) {
    struct client_entry *entry = 
        kmalloc(sizeof(struct client_entry), GFP_KERNEL);

    if(!entry) {
        printk(KERN_ERR "failed to make client entry");
        return NULL;
    }

    INIT_LIST_HEAD(&(entry->list));
    entry->socket = sock;
    entry->pgd = pgd;
    entry->pid = pid_client;
    return entry;
}

struct mapped_page* make_mapped_page(unsigned long pfn, pid_t token, bool locked, void* page_addr) {
    struct mapped_page* entry = 
        kmalloc(sizeof(*entry), GFP_KERNEL);
    memset(entry, 0, sizeof(struct mapped_page));
    
    if(!entry) {
        printk(KERN_ERR "failed to make page entry");
        return NULL;
    }

    INIT_LIST_HEAD(&(entry->list));
    entry->pfn = pfn;
    entry->locked = locked;
    entry->token = token;
    entry->page_addr = page_addr;
    return entry;
}
