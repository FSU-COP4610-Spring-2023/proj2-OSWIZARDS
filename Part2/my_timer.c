#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/string.h>

MODULE_LICENSE("Dual BSD/GPL");

#define BUF_LEN 100

static struct proc_dir_entry* proc_entry;

static char msg[BUF_LEN];
static int procfs_buf_len;

static bool first;
static struct timespec64 time;

static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos)
{
	struct timespec64 ctime;
	long long int elap_sec, elap_nsec;

	if (first) {
		ktime_get_real_ts64(&time);
		sprintf(msg, "current time: %lld.%lld\n", time.tv_sec, time.tv_nsec);
		first = false;
	}
	else {
		ktime_get_real_ts64(&ctime);
		if (ctime.tv_nsec - time.tv_nsec < 0) {
			elap_sec = ctime.tv_sec - time.tv_sec - 1;
			elap_nsec = ctime.tv_nsec - time.tv_nsec + 1000000000;
		}
		else {
			elap_sec = ctime.tv_sec - time.tv_sec;
			elap_nsec = ctime.tv_nsec - time.tv_nsec;
		}
		sprintf(msg, "current time: %lld.%lld\nelapsed time: %lld.%lld\n", ctime.tv_sec, ctime.tv_nsec, elap_sec, elap_nsec);
	}
	
	procfs_buf_len = strlen(msg);
	if (*ppos > 0 || count < procfs_buf_len)
		return 0;
	if (copy_to_user(ubuf, msg, procfs_buf_len))
		return -EFAULT;
	*ppos = procfs_buf_len;

	printk(KERN_INFO "curr: %ld.%ld\ninit: %ld.%ld\ntotl: %ld.%ld", ctime.tv_sec, ctime.tv_nsec, time.tv_sec, time.tv_nsec, elap_sec, elap_nsec);

	return procfs_buf_len;
}


static ssize_t procfile_write(struct file* file, const char * ubuf, size_t count, loff_t* ppos)
{
	//printk(KERN_INFO "proc_write\n");

	if (count > BUF_LEN)
		procfs_buf_len = BUF_LEN;
	else
		procfs_buf_len = count;

	copy_from_user(msg, ubuf, procfs_buf_len);

	//printk(KERN_INFO "got from user: %s\n", msg);

	return procfs_buf_len;
}


static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};

static int my_timer_init(void)
{
	printk(KERN_INFO "inserting my_timer module\n");

	first = true;
	proc_entry = proc_create("timer", 0666, NULL, &procfile_fops);
	strcpy(msg, "");

	if (proc_entry == NULL)
		return -ENOMEM;
	
	return 0;
}

static void my_timer_exit(void)
{	
	printk(KERN_INFO "removing my_timer module\n");
	proc_remove(proc_entry);
	return;
}

module_init(my_timer_init);
module_exit(my_timer_exit);