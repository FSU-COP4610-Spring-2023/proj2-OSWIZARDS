#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/list.h>

MODULE_LICENSE("Dual BSD/GPL");

// TODO: possible issue with too many entries in queue pushing over char limit
// len w no queue: ~400 ch, each group in queue ~50 ch
#define BUF_LEN 1000
static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int procfs_buf_len;

static bool OPEN;
static struct timespec64 time;
enum states {OFFLINE, IDLE, LOADING, CLEANING, MOVING};
static enum states waiter_state;
static int current_table,
    occupancy,
    queue_group_num,
    queue_customer_num,
    serviced_customers,
    groups_encountered;

typedef struct {
    struct timespec64 time_entered;
    int group_id;
    char type;
} Customer;

typedef struct {
    Customer occupant;
    char status;
} Place;

static Customer last_customer;
static Place stool[32];

#define DEBUG 1

static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos) {
    char s1[10];
    switch(waiter_state){
        case OFFLINE:
            strcpy(s1, "OFFLINE");
            break;
        case IDLE:
            strcpy(s1, "IDLE");
            break;
        case LOADING:
            strcpy(s1, "LOADING");
            break;
        case CLEANING:
            strcpy(s1, "CLEANING");
            break;
        case MOVING:
            strcpy(s1, "MOVING");
            break;
        default:
            strcpy(s1, "ERROR");
    }

    struct timespec64 ctime;
    ktime_get_real_ts64(&ctime);
    int i1 = ctime.tv_sec - time.tv_sec;
    if (ctime.tv_nsec >= 500000000) 
        i1++;

    // TODO: bar status
    char s2[15];
    strcpy(s2, "todo");

    sprintf(msg, "Waiter state: %s\nCurrent table: %i\nElapsed time: %i seconds\nCurrent occupancy: %i\n", s1, current_table, i1, occupancy);
    sprintf(msg, "%sBar status: %s\nNumber of customers waiting: %i\nNumber of groups waiting: %i\n", msg, s2, queue_customer_num, queue_group_num);
    sprintf(msg, "%sContents of queue:\n", msg);
    // TODO: queue contents
    sprintf(msg, "%sNumber of customers serviced: %i\n\n\n", msg, serviced_customers);
    char s3[5];
    int i;
    for (i = 4; i >= 1; i--) {
        strcpy(s3, (current_table == i) ? "[*]" : "[ ]");
        sprintf(msg, "%s%s Table %i: ", msg, s3, i);
        // TODO: table contents
        // 24 25 26 27 28 29 30 31
        // 16 17 18 19 20 21 22 23
        // 8 9 10 11 12 13 14 15
        // 0 1 2 3 4 5 6 7
        int j;
        for (j = (i-1)*8; j < (i-1)*8 + 8; j++) {
            sprintf(msg, "%s%c ", msg, stool[j].status);
        }
        sprintf(msg, "%s\n", msg);
    }

    if (DEBUG) {
        sprintf(msg, "%sLast customer data:\n\tarrival: %ld:%ld\n\t%c - %i\n", msg, last_customer.time_entered.tv_sec, last_customer.time_entered.tv_nsec, last_customer.type, last_customer.group_id);
    }

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
    if (OPEN) {
        return 1;
    }

    // if fails to allocate memory
    if (false) {
        return -1;
    }

    OPEN = true;
    waiter_state = IDLE;
	ktime_get_real_ts64(&time);
    current_table = 1;
    occupancy = 0;
    groups_encountered = 0;
    int i;
    for (i = 0; i < 32; i++)
        stool[i].status = 'C';

	return 0;
}

extern int (*STUB_customer_arrival)(int, int);
int customer_arrival(int number_of_customers, int type) {
    if (!OPEN) {
        return 0;
    }

    if (DEBUG) {
        printk(KERN_INFO "customer_arrival");
    }

    if (number_of_customers > 8) {
        printk(KERN_INFO "invalid customer amount %i\n", number_of_customers);
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

    last_customer.group_id = groups_encountered++;
    last_customer.type = c;

    return 0;
}

extern int (*STUB_close_bar)(void);
int close_bar(void) {
    if (OPEN == true)
        OPEN = false;
    else
        return 1;
	return 0;
}

extern long (*STUB_test_call)(int);
long test_call(int test) {
    return test;
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
    STUB_test_call = test_call;

    last_customer.group_id = -1;
    last_customer.type = 'X';


    current_table = -1;
    OPEN = false;

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
    STUB_test_call = NULL;

	proc_remove(proc_entry);
	return;
}

module_init(barstool_init);
module_exit(barstool_exit);