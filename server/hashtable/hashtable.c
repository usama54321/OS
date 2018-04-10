#include "hashtable.h"
#include <linux/list.h>

int init() {
    p_hash = struct(pid_hashtable_entry*) kmalloc(sizeof(struct pid_hashtable_entry), GFP_KERNEL);
    if (!p_hash)
        return 0;
    INIT_LIST_HEAD(&p_hash->list);

    vaddr_hash = struct(vaddr_hashtable_entry*) kamlloc(sizeof(struct vaddr_hashtable_entry), GFP_KERNEL);

    if (!vaddr_hash)
        return 0;
    INIT_LIST_HEAD(&vaddr_hash->list);

    spin_lock_init(pid_lock);
    spin_lock_init(vaddr_lock);
    return 1;
}

void add_pid_hashtable_entry(struct pid_hashtable_entry* entry) {
    spin_lock(pid_lock);
    list_add_tail(entry->list, p_hash->list);
    spin_unlock(pid_lock);
}

void add_vaddr_hashtable_entry(struct vaddr_hashtable_entry* entry) {
    spin_lock(vaddr_lock);
    list_add_tail(entry->list, vaddr_hash->list);
    spin_unlock(vaddr_lock);
}

void search_pid_hashtable(callBackFunc, void* arg) {
    struct pid_hashtable_entry* temp;
    spin_lock(pid_lock);
    list_for_each_entry(temp, &(p_hash->list), list) {
        callBackFunc((void*)temp, arg);
    }
    spin_unlock(pid_lock);
}

void search_vaddr_hashtable(callBackFunc, void* arg) {
    struct vaddr_hashtable_entry* temp;
    spin_lock(vaddr_lock);
    list_for_each_entry(temp, &(vaddr_hash->list), list) {
        callBackFunc((void*)temp, arg);
    }
    spin_unlock(vaddr_lock);
}
