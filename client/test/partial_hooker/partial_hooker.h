


#ifndef PARTIAL_HOOKER_H
#define PARTIAL_HOOKER_H



#include <linux/types.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>

#include <linux/ftrace.h>



typedef void (*trapfunc)(struct pt_regs*, long);



struct hook_ctx {

	void *target_fun;

	struct saved_args {
		void *arg_rdi;
		void *arg_rsi;
	} saved_args;

	struct opcode_mem {
		int hook_sz;
		void *original;
		void *overwrite;
	} opcode_mem;

};

struct my_hook {

	char *name;
	unsigned long address;
	bool found;
	bool hooked;
	struct ftrace_ops *ops;

};



int hook_routine(struct hook_ctx *hctx, void *target, void *prefun, void *postfun);
void unhook_routine(struct hook_ctx *hctx);
int kall_callback(void *data, const char *name, struct module *mod, unsigned long add);
int find_sym_address(struct my_hook *hook);



#endif /* PARTIAL_HOOKER_H */



