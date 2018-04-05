


#ifndef PARTIAL_HOOKER_C
#define PARTIAL_HOOKER_C



#include "partial_hooker.h"

#define PUSH_RAX	"\x50"
#define MOVE_RAX	"\x48\xb8"
#define MOVE_RDI	"\x48\xbf"
#define MOVE_RDX	"\x48\xba"
#define JUMP_RAX	"\xff\xe0"
#define CALL_RAX	"\xff\xd0"
#define FCALL_RAX	"\xff\x10"
#define CALLQ		"\x9a"
#define RETURN		"\xc3"
#define FRETURN		"\xcb"

#define INIT_OPCODES(hkr) {				\
							\
	strcpy((hkr)->move_rdx, MOVE_RDX);		\
	strcpy((hkr)->move_rdi, MOVE_RDI);		\
							\
	strcpy((hkr)->move_rax_saveargs, MOVE_RAX);	\
	strcpy((hkr)->move_rax_prefun, MOVE_RAX);	\
	strcpy((hkr)->move_rax_sandboxer, MOVE_RAX);	\
	strcpy((hkr)->move_rax_postfun, MOVE_RAX);	\
							\
	strcpy((hkr)->call_rax_saveargs, FCALL_RAX);	\
	strcpy((hkr)->call_rax_prefun, CALL_RAX);	\
	strcpy((hkr)->call_rax_sandboxer, CALL_RAX);	\
	strcpy((hkr)->call_rax_postfun, CALL_RAX);	\
							\
	strcpy((hkr)->ret, RETURN);			\
							\
}
#define WITH_WP_DISABLED(statements) {	\
	unsigned long __cr0;		\
	preempt_disable();		\
	barrier();			\
	__cr0 = read_cr0();		\
	write_cr0(__cr0 & ~X86_CR0_WP);	\
	{statements;}			\
	write_cr0(__cr0);		\
	barrier();			\
	preempt_enable();		\
}



typedef char byte_opcode[ 1 ];
typedef char word_opcode[ 2 ];



struct hooking_code {

	/* Store saving buffer pointer to rdx */
	word_opcode move_rdx;
	void *saving_buffer;

	/* Preserve program arguments */
	word_opcode move_rax_saveargs;
	void *saveargs_handler;
	word_opcode call_rax_saveargs;

	/* Call prefun */
	word_opcode move_rax_prefun;
	void *prefun_handler; /* XXX: Registers are preserved but not restored for this function (incorrect preconditions) */
	word_opcode call_rax_prefun;

	/* Return */
	byte_opcode ret;

	/* First argument to sandboxer */
	word_opcode move_rdi;
	void *sandboxer_ctx;

	/* Call sandboxer */
	word_opcode move_rax_sandboxer;
	void *sandboxer_handler;
	word_opcode call_rax_sandboxer;

	/* Call postfun */
	word_opcode move_rax_postfun;
	void *postfun_handler; /* XXX: Registers must be restored after this function (incorrect postconditions) */
	word_opcode call_rax_postfun;

} __attribute__((packed));



static void _stub_(void);
static void __save_args(void *rdi, void *rsi, struct saved_args *saved_args);
static void sandboxer(struct hook_ctx *hctx);
static void tester(void *rdi, void *rsi) asm("tester");
static void print_rdi(void *rdi) {

	printk(KERN_ALERT "RDI value: %p", rdi);

}

unsigned long __save_args_addr = (unsigned long)__save_args;
unsigned long __stub_addr = (unsigned long)_stub_;
unsigned long print_rdi_addr = (unsigned long)print_rdi;

asmlinkage void call_far(void);
asm("	.text");
asm("	.type call_far,@function");
asm("call_far:");
asm("	callq	*__save_args_addr	");
asm("	callq	*__stub_addr		");
// asm("	popq	%rax		");
// asm("	jmpq	*%rax		");
asm("	retf				");
// asm("	mov	%rdi,%rax	");
// asm("	callq	*%rax		");

void hdump(void *buf, int len) {

	int i;
	char hbuf[512];

	if (len > 170)
		len = 170;

	for (i = 0; i < len; i++)
		sprintf((char*)hbuf + 3*i, " %02x", (unsigned char)((char*)buf)[i]);

	printk(KERN_ALERT "Dumping bytes:%s", hbuf);

	return;

}

typedef void (*f1)(void *rdi, void *rsi, struct saved_args *saved_args);
typedef void (*f2)(void);

static void hardswap(void *l1, void *l2) {

	*(char*)l1 ^= *(char*)l2;
	*(char*)l2 ^= *(char*)l1;
	*(char*)l1 ^= *(char*)l2;

	return;

}

static void tester(void *rdi, void *rsi) {

	register struct saved_args *saved_args;
	void *x0;

	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;
	x0 = NULL;

	x0 = __save_args;
	((f1)x0)(rdi, rsi, saved_args);

	x0 = _stub_;
	((f2)x0)();

	printk(KERN_ALERT "x0: %p", &x0);

	return;

}

int hook_routine(struct hook_ctx *hctx, void *target, void *prefun, void *postfun) {

	struct hooking_code *hooker;

	/* Prepare hooking state */
	hctx->target_fun = target;
	hctx->opcode_mem.hook_sz = sizeof(struct hooking_code);
	if ( !(hctx->opcode_mem.original = (void*)__get_free_page(GFP_KERNEL)) ) {
		printk(KERN_INFO "megavm_hga: failed allocation...");
		return -1;
	}
	if ( !(hctx->opcode_mem.overwrite = (void*)__get_free_page(GFP_KERNEL)) ) {
		printk(KERN_INFO "megavm_hga: failed allocation...");
		return -1;
	}

	memcpy(hctx->opcode_mem.original, target, hctx->opcode_mem.hook_sz);

	hdump(target, sizeof(struct hooking_code));

	WITH_WP_DISABLED(

		/* Overwrite part of the target code */

		hooker = (struct hooking_code*)target;

		INIT_OPCODES(hooker);

		hooker->saving_buffer = &(hctx->saved_args);
		hooker->saveargs_handler = __save_args;
		hooker->saveargs_handler = call_far;
		if ( !(hooker->prefun_handler = prefun) )
			hooker->prefun_handler = _stub_;
		hooker->sandboxer_ctx = hctx;
		hooker->sandboxer_handler = sandboxer;
		if ( !(hooker->postfun_handler = postfun) )
			hooker->postfun_handler = _stub_;

	);

	memcpy(hctx->opcode_mem.overwrite, target, hctx->opcode_mem.hook_sz);

	hdump(target, sizeof(struct hooking_code));

	return 0;

}

void unhook_routine(struct hook_ctx *hctx) {

	WITH_WP_DISABLED(
		memcpy(hctx->target_fun, hctx->opcode_mem.original, hctx->opcode_mem.hook_sz);
	);

	free_page((long)hctx->opcode_mem.original);
	free_page((long)hctx->opcode_mem.overwrite);

	return;

}

int kall_callback(void *data, const char *name, struct module *mod, unsigned long add) {

	struct my_hook *temp = (struct my_hook *)data;

	if (temp->found)
		return 1;

	if (name && temp->name && strcmp(temp->name, name) == 0) {
		temp->address = add;
		temp->found = true;
		return 1;
	}

	return 0;

}

int find_sym_address(struct my_hook *hook) {

	if (!kallsyms_on_each_symbol(kall_callback, (void *)hook)){
		printk(KERN_INFO "%s symbol not found for some reason!", hook->name);
		return 0;
	}

	return 1;

}



static void sandboxer(struct hook_ctx *hctx) {

	/*
	 * Run the original trap handler with
	 * the same environment it is expecting
	 */

	void *rdi, *rsi;
	trapfunc target;

	printk(KERN_ALERT "Entered sandboxer");

	target = (trapfunc)(hctx->target_fun);

	/* Prepare environment */
	WITH_WP_DISABLED(
		memcpy(target, hctx->opcode_mem.original, hctx->opcode_mem.hook_sz);
	);
	rdi = hctx->saved_args.arg_rdi;
	rsi = hctx->saved_args.arg_rsi;

	/* Call */
	target((struct pt_regs*)rdi, (long)rsi);

	/* Restore environment */
	WITH_WP_DISABLED(
		memcpy(target, hctx->opcode_mem.overwrite, hctx->opcode_mem.hook_sz);
	);

	printk(KERN_ALERT "Leaving sandboxer");

	return;

}

static void _stub_(void) {

	printk(KERN_ALERT "CALLED STUB FUNCTION");

	return;
}

static void __save_args(void *rdi, void *rsi, struct saved_args *saved_args) {

	printk(KERN_ALERT "Entered __save_args (tester address: %p)", tester);

	saved_args->arg_rdi = rdi;
	saved_args->arg_rsi = rsi;

	printk(KERN_ALERT "Leaving __save_args");

	return;

}



#endif /* PARTIAL_HOOKER_C */



