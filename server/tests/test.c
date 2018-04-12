#include "test.h"

int hashtable_tests(void) {
    struct pid_hashtable_entry* found;
    unsigned long token;
    struct pid_hashtable_entry entry = {
        .token=1,
        .pid=1
    };
    
    found = NULL;
    token = 1;

    add_pid_hashtable_entry(&entry);

    foreach_pid_hashtable(test_pid_hashtable_lookup_callback, &found, (void*)&token);
    printk(KERN_INFO "addr %p", found);
    if (!found)
        return 0;
    return 1;
}

void test_pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg) {
    if(((struct pid_hashtable_entry*)current_entry)->token == *((unsigned long*)arg)) {
        struct pid_hashtable_entry** temp = (struct pid_hashtable_entry**)found;
        *temp = (struct pid_hashtable_entry*) current_entry;
    }
}
