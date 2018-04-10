#include <linux/hashtable.h>
#include <linux/list.h>

typedef void (*callBackFunc)(void*, void*);

struct pid_hashtable_entry {
    struct list_head list;
    unsigned long token;
    pid_t pid;
};

struct vaddr_hashtable_entry {
    struct list_head list;
    unsigned long pte_addr;
};

static pid_hashtable_entry *p_hash;
static vaddr_hashtable_entry *vaddr_hash;

/*
 * locks for hashtables
 */
static spinlock_t *pid_lock;
static spinlock_t *vaddr_lock;

int init();
void add_pid_hashtable_entry(struct pid_hashtable_entry* entry);
void add_vaddr_hashtable_entry(struct vaddr_hashtable_entry* entry);
void search_pid_hashtable(callBackFunc, void* arg);
void search_vaddr_hashtable(callBackFunc, void* arg);
