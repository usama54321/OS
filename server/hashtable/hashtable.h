#include<linux/hashtable>

void init_pid_hashtable();
void init_vaddr_hashtable();
static DEFINE_HASHTABLE(pid_hashtable, 32)
static
