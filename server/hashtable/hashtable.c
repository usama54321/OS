#include "hashtable.h"

/*
 * locks for hashtables
 */
static DEFINE_SPINLOCK(pid_lock);
static DEFINE_SPINLOCK(vaddr_lock);

static struct pid_hashtable_entry *p_hash;
static struct vaddr_hashtable_entry *vaddr_hash;

int hashtables_init(void) {
    p_hash = (struct pid_hashtable_entry*) kmalloc(sizeof(struct pid_hashtable_entry), GFP_KERNEL);
    if (!p_hash)
        return 0;
    INIT_LIST_HEAD(&(p_hash->list));

    vaddr_hash = (struct vaddr_hashtable_entry*) kmalloc(sizeof(struct vaddr_hashtable_entry), GFP_KERNEL);

    if (!vaddr_hash)
        return 0;
    INIT_LIST_HEAD(&(vaddr_hash->list));

    return 1;
}

void add_pid_hashtable_entry(struct pid_hashtable_entry* entry) {
    spin_lock(&pid_lock);
    list_add_tail(&(entry->list), &(p_hash->list));
    spin_unlock(&pid_lock);
}

void add_vaddr_hashtable_entry(struct vaddr_hashtable_entry* entry) {
    spin_lock(&vaddr_lock);
    list_add_tail(&(entry->list), &(vaddr_hash->list));
    spin_unlock(&vaddr_lock);
}

void foreach_pid_hashtable(callBackFunc func, void* entry, void* arg) {
    struct pid_hashtable_entry* temp;
    spin_lock(&pid_lock);
    list_for_each_entry(temp, &(p_hash->list), list) {
        func((void*)temp, entry, arg);
    }
    spin_unlock(&pid_lock);
}

void foreach_vaddr_hashtable(callBackFunc func, void* entry, void* arg) {
    struct vaddr_hashtable_entry* temp;
    spin_lock(&vaddr_lock);
    list_for_each_entry(temp, &(vaddr_hash->list), list) {
        func((void*)temp, entry, arg);
    }
    spin_unlock(&vaddr_lock);
}

struct vaddr_hashtable_entry* make_vaddr_hashtable_entry(unsigned long pfn, pid_t pid, bool locked, unsigned long client_ip, unsigned long client_pgd) {
    struct vaddr_hashtable_entry* entry = 
        kmalloc(sizeof(*entry), GFP_KERNEL);
    entry->pfn_addr = pfn;
    entry->pid = pid;
    entry->locked = true;
    entry->clients = kmalloc(sizeof(struct client_hashtable_entry), GFP_KERNEL);
    entry->clients->ip = client_ip;
    entry->clients->pgd = client_pgd;
    INIT_LIST_HEAD(&(entry->clients->list));
    return entry;
}
