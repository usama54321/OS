#ifndef HGA_TESTS
#define HGA_TESTS

#include "../hashtable/hashtable.h"
#include <linux/kernel.h>

int hashtable_tests(void);
void test_pid_hashtable_lookup_callback(void* current_entry, void* found, void* arg);
#endif
