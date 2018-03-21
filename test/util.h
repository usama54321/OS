#include <linux/types.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>

#include <linux/ftrace.h> // kallsyms_on_each_symbol

struct my_hook {
	char *name;
	unsigned long address;
	bool found;
	bool hooked;
	struct ftrace_ops *ops;
};

void hijack_start(void *, void* new);
void hijack_pause ( void *target );
void hijack_resume ( void *target );
void hijack_stop ( void *target );
inline void restore_wp ( unsigned long cr0 );
inline unsigned long disable_wp ( void );
int kall_callback(void *data, const char *name, struct module *mod, unsigned long add);
int find_sym_address(struct my_hook *hook) asm("find_sym_address");
