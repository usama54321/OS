


#ifndef SYMFIND_C
#define SYMFIND_C



#include "symfind.h"



struct my_finder {

	char *name;
	unsigned long long address;
	bool found;

};



static int kall_callback(void *data, const char *name, struct module *mod, unsigned long add) {

	struct my_finder *temp = (struct my_finder*)data;

	if (temp->found)
		return 1;

	if (name && temp->name && strcmp(temp->name, name) == 0) {
		temp->address = add;
		temp->found = true;
		return 1;
	}

	return 0;

}

unsigned long find_sym_address(char *name) {

	struct my_finder finder;
	finder.name = name;
	finder.found = false;

	if (!kallsyms_on_each_symbol(kall_callback, (void*)&finder)){
		printk(KERN_INFO "%s symbol not found for some reason!", finder.name);
		return 0;
	}

	return finder.address;

}



#endif /* SYMFIND_C */



