


/*
 *
 * DESCRIPTION:
 *    Retrieve process info from it's task struct.
 *    To be removed after PID identification is
 *    fully implemented.
 *
 * TODO:
 *    Implement PID identification on the server
 *    side and remove this for good.
 *
 */



#ifndef TASK_FUNCS_C
#define TASK_FUNCS_C



#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pgtable_types.h>

#include "../common/hga_defs.h"
#include "../task_funcs/task_funcs.h"



static char *target_procnames[] = {
	/* List the target process names here */
	"fptrtest",
};



static char *last_chunk(char *str, char delim);
static int get_process_binary_name(struct mm_struct *mm, char namebuf[]);



/* Returns 1 if the process belonged to the targeted list, 0 if not and -1 on error */
int task_targeted(struct task_struct *task) {

	char proc_name[PROCNAME_MAXLEN];
	int i, name_matched, n_targetprocs;

	n_targetprocs = sizeof(target_procnames) / sizeof(*target_procnames);

	if ( get_process_binary_name(task->mm, proc_name) < 0 )
		/* Anonymous process? */
		return -1;

	/* Search for this name in the target list */
	name_matched = 0;
	for ( i = 0; i < n_targetprocs; i++ ) {
		if ( strcmp(proc_name, target_procnames[i]) == 0 ) {
			name_matched = 1;
			break;
		}
	}

	return name_matched;

}

/* Returns 0 if the name was successfully retrieved or -1 on error */
int task_get_name(struct task_struct *task, char *name) {

	if ( get_process_binary_name(task->mm, name) < 0 )
		return -1;

	return 0;

}



static char *last_chunk(char *str, char delim) {

	char *last = str, curr;

	while ( (curr = *(str++)) )
		if ( curr == delim )
			last = str;

	return last;

}

static int get_process_binary_name(struct mm_struct *mm, char namebuf[]) {

	char *p, *pathbuf;

	if ( !mm )
		return -1;

	down_read(&mm->mmap_sem);

	if ( !mm->exe_file ) {
		up_read(&mm->mmap_sem);
		return -1;
	}

	pathbuf = kmalloc(PATH_MAX, GFP_ATOMIC);
	if ( pathbuf )
		p = d_path(&mm->exe_file->f_path, pathbuf, PATH_MAX);
	else {
		up_read(&mm->mmap_sem);
		return -1;
	}

	up_read(&mm->mmap_sem);
	strcpy(namebuf, last_chunk(p, '/'));
	kfree(pathbuf);

	return 0;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* TASK_FUNCS_C */



