#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/string.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_LEN 600
static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int procfs_buf_len;

#define DEBUG 1

static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos) {
    if (DEBUG) {
        printk(KERN_INFO "procfile_read\n");
    }

    sprintf(msg, "Waiter state: -1\nCurrent table: -1\nElapsed time: -1 seconds\nCurrent occupancy:- 1\n");
    sprintf(msg, "Bar status: -1\nNumber of customers waiting: -1\nNumber of groups waiting: -1\nContents of queue:\n-1");
    sprintf(msg, "Number of customers services: -1\n\n\n[*] Table 4: -1\n[ ] Table 3: -1\n[ ] Table 2: -1\n[ ] Table 1: -1\n");
    
    procfs_buf_len = strlen(msg);
	if (*ppos > 0 || count < procfs_buf_len) {
		return 0;
    }
	if (copy_to_user(ubuf, msg, procfs_buf_len)) {
		return -EFAULT;
    }
	*ppos = procfs_buf_len;
    return procfs_buf_len;
}

static ssize_t procfile_write(struct file* file, const char * ubuf, size_t count, loff_t* ppos) {
    if (DEBUG) {
        printk(KERN_INFO "procfile_write\n");
    }

	if (count > BUF_LEN) {
		procfs_buf_len = BUF_LEN;
    }
	else {
		procfs_buf_len = count;
    }

	copy_from_user(msg, ubuf, procfs_buf_len);

	return procfs_buf_len;
}

static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};

extern int (*STUB_initialize_bar)(void);
int initialize_bar(void) {
    if (DEBUG) {
        printk(KERN_INFO "initialize_bar\n");
    }

	return 0;
}

extern int (*STUB_customer_arrival)(int, int);
int customer_arrival(int number_of_customers, int type) {
    if (DEBUG) {
        printk(KERN_INFO "customer_arrival");
    }

    if (number_of_customers > 8) {
        printk(KERN_INFO "invalid customer number %i\n", number_of_customers);
        return 1;
    }
    
    char c;
    switch (type) {
        case 0:
            c = 'F';
            break;
        case 1:
            c = 'O';
            break;
        case 2:
            c = 'J';
            break;
        case 3:
            c = 'S';
            break;
        case 4:
            c = 'P';
            break;
        default:
            printk(KERN_INFO "invalid customer type %i\n", type);
            c = 'X';
            return 1;

    }

    if (DEBUG) {
        printk(KERN_INFO "customer_arrival: %i %c\n", number_of_customers, c);
    }

    return 0;
}

extern int (*STUB_close_bar)(void);
int close_bar(void) {
    if (DEBUG) {
        printk(KERN_INFO "close_bar\n");
    }

	return 0;
}

static int barstool_init(void) {
    if (DEBUG) {
        printk(KERN_INFO "barstool_init\n");
    }

    proc_entry = proc_create("majorsbar", 0666, NULL, &procfile_fops);
	strcpy(msg, "");

    STUB_close_bar = close_bar;
    STUB_customer_arrival = customer_arrival;
    STUB_initialize_bar = initialize_bar;

	if (proc_entry == NULL) {
		return -ENOMEM;
    }
    
    return 0;
}

static void barstool_exit(void)
{	
    if (DEBUG) {
        printk(KERN_INFO "barstool_exit\n");
    }

    STUB_close_bar = NULL;
    STUB_customer_arrival = NULL;
    STUB_initialize_bar = NULL;

	proc_remove(proc_entry);
	return;
}

module_init(barstool_init);
module_exit(barstool_exit);