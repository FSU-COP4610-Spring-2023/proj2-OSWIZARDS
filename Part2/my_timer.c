#include <linux/module.h> /* Needed by all modules */ 
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/init.h> 
#include <linux/time64.h> 
#include <linux/proc_fs.h> 
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>

MODULE_LICENSE("Dual BSD/GPL");

// create_proc_entry()  - Creates a proc entry that can read or write
//ceate_proc_read_entry(const char *name,  mode_t mode, struct proc_dir_entry *base,  read_proc_t *read_proc, void * data)
/* name: This the name by which entry will be created under the proc file system.
*  mode: mode sets the permissions for the entry created, if 0 is passed it takes system default settings
*  
*  base: : Base is the base directory in which the entry should be created, this is useful when 
*  you want to create a proc entry under a sub folder in /proc. If you pass NULL it will create it under /proc by default
*  
*  Read_proc: read_proc is a pointer to the function that should be called every time the proc entry is read. The function should be implemented in the
*  driver. It is this functions which we will use to display what ever data  we  want to display to the user.
*  DATA: This is not used by the kernel, but is passed as it is to the read_proc function. The data is used by the driver writer to pass any private data
*  that has to be passed to the read_proc function.  It can be passed as NULL if no data has to be passed.
*/
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
static char msg2[BUF_LEN];
static  struct timespec64 ts;
bool opened = 1;
static int procfs_buf_len;

static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos)
{

	  procfs_buf_len = strlen(msg);

        if (*ppos > 0 || count < procfs_buf_len)
                return 0;

        if (copy_to_user(ubuf, msg, procfs_buf_len))
                return -EFAULT;

        *ppos = procfs_buf_len;
        printk(KERN_INFO "%s\n", msg);
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
	printk(KERN_INFO "%s\n", msg);

	return procfs_buf_len;

}


static int procfile_open(struct inode *sp_inode, struct file *sp_file) {

	 struct timespec64 ts2;
	 if(opened)
	 {  ktime_get_real_ts64(&ts);
 	 }

 	 ktime_get_real_ts64(&ts2);
	 procfs_buf_len = strlen(msg);
	 sprintf(msg,"current time: %lld.%lld\n",ts2.tv_sec, ts2.tv_nsec);

	if(opened == 0)
	{

		 if( (ts2.tv_nsec - ts.tv_nsec)  < 0)
        	{       ts2.tv_sec  = ts2.tv_sec -  ts.tv_sec;
                	ts2.tv_nsec += 1000000000;      //1 sec  = 1*10^9 nsec
                	ts2.tv_nsec = ts2.tv_nsec - ts.tv_nsec;
                	ts2.tv_sec -=1;
        	}

        	else
        	{       ts2.tv_sec  = ts2.tv_sec -  ts.tv_sec;
                	ts2.tv_nsec = ts2.tv_nsec - ts.tv_nsec;
        	}

        	sprintf(msg2,"elapsed time: %lld.%lld\n",ts2.tv_sec , ts2.tv_nsec);
		strcat(msg,msg2);
	}

	if(opened)
	opened=0;

	return  0;


	//printk(KERN_INFO "got from user: %s\n", msg);

	return procfs_buf_len;
}


static struct proc_ops procfile_fops = {
        .proc_read =  procfile_read,
	.proc_write = procfile_write,
	.proc_open  =  procfile_open,
};


static int hello_init(void)
{
	proc_entry = proc_create("timer",0666, NULL,&procfile_fops);
	if (proc_entry == NULL)
		return -ENOMEM;

	return 0;

}

static void hello_exit(void)
{
    proc_remove(proc_entry);
    return;
}


module_init(hello_init);
module_exit(hello_exit);
