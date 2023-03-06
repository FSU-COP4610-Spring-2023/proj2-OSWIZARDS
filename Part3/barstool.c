#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>

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

static int waiter_pid;

typedef struct {
    struct timespec64 time;
    int group_id;
    int group_count;
    char type;
    struct list_head list;
} Customer;

struct list_head Queue = LIST_HEAD_INIT(Queue);

typedef struct {
    Customer *occupant;
    char status;
} Place;

static Customer last_customer;
static Place stool[32];

#define DEBUG 1

static bool waiter_toss_customer(void) {
    int req_time;
    Customer *c;
    bool removed = false;
    struct timespec64 ctime;

    int i;
    for (i = 0; i < 8; i++) {
        req_time = -1;
        switch(stool[8*(current_table-1) + i].status) {
            case 'F':
                req_time = 5;
                break;
            case 'O':
                req_time = 10;
                break;
            case 'J':
                req_time = 15;
                break;
            case 'S':
                req_time = 20;
                break;
            case 'P':
                req_time = 25;
                break;
        }
        if (req_time != -1) {
            c = stool[8*(current_table-1) + i].occupant;
            ktime_get_real_ts64(&ctime);

            // if they've had enough time
            if (ctime.tv_sec - c->time.tv_sec >= req_time) {
                removed = true;
                stool[8*(current_table-1) + i].status = 'D';
                // TODO review logic right here.
                // once the customer leaves the chair, they are dead to us.
                kfree(stool[8*(current_table-1) + i].occupant);
            }
        }
    }

    return removed;
}

static bool waiter_clean_table(void) {
    int dirty_count = 0, i;

    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'D') {
            dirty_count++;
        }
    }

    if (dirty_count >= 4) {
        for (i = 0; i < 8; i++) {
            if (stool[8*(current_table-1) + i].status == 'D') {
                stool[8*(current_table-1) + i].status = 'C';
            }
        }

        return true;
    }

    return false;
}

static bool waiter_seat_customer(void) {
    Customer *c;
    int i, num = 0, seated = 0;

    // look at front customer
    c = list_first_entry(&Queue, Customer, list);

    // see if current table has space
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'C') {
            num++; 
        }
    }

    if (num >= c->group_count) { // if it does
        // seatem
        for (i = 0; seated < c->group_count; i++) {
            if (stool[8*(current_table-1) + i].status == 'C') {
                stool[8*(current_table-1) + i].status = c->type;
                stool[8*(current_table-1) + i].occupant = c;
                seated++; 
            }
        }

        list_del(&c->list);
        return true;
    }

    return false;
}

static bool waiter_move_table(void) {
    current_table++;
    if (current_table == 5) {
        current_table = 1; 
    }

    return true;
}

static void waiter_brain(void) {
    while (true) {
        // move table
        if (waiter_move_table()) {
            msleep(2000);
        }

        // seat customers
        if (waiter_seat_customer()) {
            msleep(1000);
        }

        // remove customers
        if (waiter_toss_customer()) {
            msleep(1000);
        }

        // clean table
        if (waiter_clean_table()) {
            msleep(10000);
        }

        // seat customers (again)
        if (waiter_seat_customer()) {
            msleep(1000);
        }

    }
}

static int addQueue(char type, int num) {
    int group_id = groups_encountered;
    int i;

    for (i = 0; i < num; i++) {
        Customer* new_cus = kmalloc(sizeof(Customer), __GFP_NOFAIL);
        new_cus->type = type;
        new_cus->group_id = group_id;
        new_cus->group_count = num;
        INIT_LIST_HEAD(&(new_cus->list));

        if (type == 'F') {
            list_add(&(new_cus->list), &Queue);
        }
        else {
            list_add_tail(&(new_cus->list), &Queue);
        }
    }

	queue_group_num++;
	queue_customer_num += num;

    return 1;
}

static int deleteQueue(void) {
    Customer *c;
    int amt, i;

	if (list_empty_careful(&Queue) != 0) //if empty
		return -1;

    c = list_first_entry(&Queue, Customer, list);
    amt = c->group_count;
    list_del(&c->list);
    kfree(c);

    for (i = 1; i < amt; i++) {
        c = list_first_entry(&Queue, Customer, list);
        list_del(&c->list);
        kfree(c);
    }

    queue_group_num--;
    queue_customer_num -= amt;

	return 0;
}

static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos) {
    char s1[10], s2[50], s3[5];
    struct timespec64 ctime;
    int i1, i, fr=0, so=0, ju=0, se=0, pr=0, c_id, n_id, j;
    Customer *c, *c2;

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

    ktime_get_real_ts64(&ctime);
    i1 = ctime.tv_sec - time.tv_sec;
    if (ctime.tv_nsec >= 500000000) 
        i1++;

    if (occupancy == 0) {
        strcpy(s2, "Empty");
    }
    else {
        for (i = 0; i < 32; i++) {
            switch (stool[i].status) {
                case 'F':
                    fr++;
                    break;
                case 'O':
                    so++;
                    break;
                case 'J':
                    ju++;
                    break;
                case 'S':
                    se++;
                    break;
                case 'P':
                    pr++;
                    break;
                default:
                    ;
            }
        }
        sprintf(s2, "Fr: %i | So: %i | Ju: %i | Se: %i | Pr: %i", fr, so, ju, se, pr);
    }

    sprintf(msg, "Waiter state: %s\nCurrent table: %i\nElapsed time: %i seconds\nCurrent occupancy: %i\n", s1, current_table, i1, occupancy);
    sprintf(msg, "%sBar status: %s\nNumber of customers waiting: %i\nNumber of groups waiting: %i\n", msg, s2, queue_customer_num, queue_group_num);
    sprintf(msg, "%sContents of queue:\n", msg);
    
    // queue contents
    if (list_empty(&Queue) == 0) { // if not empty
        list_for_each_entry(c, &Queue, list) {
            c_id = c->group_id;
            c2 = list_next_entry(c, list);
            n_id = c2 ? c2->group_id : -1;

            sprintf(msg, "%s%c ", msg, c->type);
            if (c_id != n_id) {
                sprintf(msg, "%s(group id: %i)\n", msg, c->group_id);
            }
        }
    }
    else {
        sprintf(msg, "%sEmpty\n", msg);
    }

    sprintf(msg, "%sNumber of customers serviced: %i\n\n\n", msg, serviced_customers);
    for (i = 4; i >= 1; i--) {
        strcpy(s3, (current_table == i) ? "[*]" : "[ ]");
        sprintf(msg, "%s%s Table %i: ", msg, s3, i);
        for (j = (i-1)*8; j < (i-1)*8 + 8; j++) {
            sprintf(msg, "%s%c ", msg, stool[j].status);
        }
        sprintf(msg, "%s\n", msg);
    }

    if (DEBUG) {
        sprintf(msg, "%sMost recent addition to queue:\n\ttype: %c group_count: %i group_id: %i\n", msg, last_customer.type, last_customer.group_count, last_customer.group_id);
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
    int i;

    if (count > BUF_LEN) {
		procfs_buf_len = BUF_LEN;
    }
	else {
		procfs_buf_len = count;
    }

	i = copy_from_user(msg, ubuf, procfs_buf_len);

	return procfs_buf_len;
}

static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};

extern int (*STUB_initialize_bar)(void);
int initialize_bar(void) {
    int i, pid;

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

    for (i = 0; i < 32; i++) {
        stool[i].status = 'C';
    }

    // setup waiter
    // TO DO: fork? process? 
    pid = -1;
    if (pid == 0) {
        // in waiter
        waiter_brain();
        return 0;
    }
    else {
        waiter_pid = pid;
    }

	return 0;
}

extern int (*STUB_customer_arrival)(int, int);
int customer_arrival(int number_of_customers, int type) {
    char c;

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
    last_customer.group_count = number_of_customers;

    addQueue(c, number_of_customers);

    return 0;
}

extern int (*STUB_close_bar)(void);
int close_bar(void) {
    if (OPEN == true) {
        OPEN = false;
    }
    else {
        return 1;
    }

    while (queue_group_num > 0) {
        deleteQueue();
    }

    // kill waiter
    // kill(waiter_pid);

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

    // TODO: delete list? 

	proc_remove(proc_entry);
	return;
}

module_init(barstool_init);
module_exit(barstool_exit);