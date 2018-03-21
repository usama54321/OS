//#include "common.h"
#include "util.h"

#define HIJACK_SIZE 12


unsigned long current_swap_offset = 1;

char **temp_swap;

struct sym_hook {
    void *addr;
    unsigned char o_code[HIJACK_SIZE];
    unsigned char n_code[HIJACK_SIZE];
    struct list_head list;
};

LIST_HEAD(hooked_syms);

inline unsigned long disable_wp ( void )
{
    unsigned long cr0;

    preempt_disable();
    barrier();

    cr0 = read_cr0();
    write_cr0(cr0 & ~X86_CR0_WP);
    return cr0;
}

inline void restore_wp ( unsigned long cr0 )
{
    write_cr0(cr0);

    barrier();
    preempt_enable();
}
void hijack_stop ( void *target )
{
    struct sym_hook *sa;

    list_for_each_entry ( sa, &hooked_syms, list )
        if ( target == sa->addr )
        {
            unsigned long o_cr0 = disable_wp();
            memcpy(target, sa->o_code, HIJACK_SIZE);
            restore_wp(o_cr0);

            list_del(&sa->list);
            kfree(sa);
            break;
        }
}

void hijack_resume ( void *target )
{
    struct sym_hook *sa;

    list_for_each_entry ( sa, &hooked_syms, list )
        if ( target == sa->addr )
        {
            unsigned long o_cr0 = disable_wp();
            memcpy(target, sa->n_code, HIJACK_SIZE);
            restore_wp(o_cr0);
        }
}

void hijack_pause ( void *target )
{
    struct sym_hook *sa;

    list_for_each_entry ( sa, &hooked_syms, list )
        if ( target == sa->addr )
        {
            unsigned long o_cr0 = disable_wp();
            memcpy(target, sa->o_code, HIJACK_SIZE);
            restore_wp(o_cr0);
        }
}

void hijack_start(void *target, void *new) {
	struct sym_hook *sa;
    unsigned char o_code[HIJACK_SIZE], n_code[HIJACK_SIZE];
    unsigned long o_cr0;

    // mov rax, $addr; jmp rax
    memcpy(n_code, "\x48\xb8\x00\x00\x00\x00\x00\x00\x00\x00\xff\xe0", HIJACK_SIZE);
    *(unsigned long *)&n_code[2] = (unsigned long)new;


    memcpy(o_code, target, HIJACK_SIZE);

    o_cr0 = disable_wp();
    memcpy(target, n_code, HIJACK_SIZE);
    restore_wp(o_cr0);

    sa = kmalloc(sizeof(*sa), GFP_KERNEL);
    if ( ! sa )
        return;

    sa->addr = target;
    memcpy(sa->o_code, o_code, HIJACK_SIZE);
    memcpy(sa->n_code, n_code, HIJACK_SIZE);

    list_add(&sa->list, &hooked_syms);
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


