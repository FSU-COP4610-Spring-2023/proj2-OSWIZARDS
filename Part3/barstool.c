#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");

#define BUF_LEN 600
#define DEBUG 1

static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int procfs_buf_len;



typedef struct{
        struct timespec64 entered;
        int group_id; // Group_id could be a random number between INT_MIX and INT_MAX to keep track of the groups.
        int type;
}customer;


typedef struct
{	bool status;
	costumer student;
}seat;

typedef struct{
	seat  seat[8];
}table;


typedef struct{
	struct list_head queue;
	table tables[4];
}bar;


typedef struct{
	int groupID;
	customer gr[8];
	int groupCount;
	struct list_head list;
	char  type;
}Group;

struct{
	 struct list_head list;
	int numGroups;
	int numCustomers;

}Queue;


struct bar b;
int ooccupancy;
enum State{OFFLINE, IDLE, LOADING, MOVING };
enum Class{FRESHMEN = 0, SOPHMORE = 1, JUNIOR = 2, SENIOR = 5, PROFESSOR = 4}
enum State s;
enum Class c;
int groupEncountered;
char  message[200];
char  buf[200];
extern int (*STUB_initialize_bar)(void);
int initialize_bar(void) {
    if (DEBUG) {
        printk(KERN_INFO "initialize_bar\n");
    }
	if(s==IDLE)
	   return 1; //BAR IS ALREADY ACTIVE



	groupEncountered=0;
	if(s==OFFLINE)
	  return  0;

 return -1;
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

int procfile_open(struct inode *sp_inode, struct file *sp_file) {

	if (message == NULL) {
		printk(KERN_WARNING "animal_proc_open");
		return -ENOMEM;
	}

	addQueuel(get_random_int() % 5 + 1, get_random_int() % 8 + 1 );

	return print_animals();

}

static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
	.proc_open =  procfile_open,
};






int addQueue(char type, int num)
	struct timespec64 time;
        int group_id = groupEncountered++;
	Group  *g;



	s = kmalloc(sizeof(Group) * 1, __GFP_RECLAIM);
	if (s == NULL)
		return -ENOMEM;


	g->groupID=group_id;
	g->groupCount=num;
	g->type=type;


	if(type>1)
		list_add_tail(&g->list, &Queue.list); /* insert at front of list */

	if(type<=1)
		list_add(&g->list, &Queue.list); /* insert at back of list */


	Queue.numGroups++;
	Queue.numCustomers+=num;
}


int  deleteQueue(void)
{
	if(list_empty_careful(&Queue.list) ==  0) 	//check what  return not sure
		return -1;

	struct list_head *temp;
	Group * g;
	temp = list_next_entry (&Queue.list);
	int numGroups;
        int numCustomers;
	Queue.numGroups--;
	Queue.numCustomers-= g->groupCount;
	list_del(temp);
	kfree(&temp);
	return 0;
}


int print_queue(void)
{
	int i;
	Group  *g;
	struct list_head *temp;

	if (buf == NULL) {
		printk(KERN_WARNING "Print BarsTool");
		return -ENOMEM;
	}

	/* init message buffer */
	strcpy(message, "");
	strcpy(buf,"");
	/* headers, print to temporary then append to message buffer */
	sprintf(buf, "Number of customers waiting:  %d\n", Queue.numCustomers); strcat(message, buf);
	sprintf(buf, "Number of groups waiting:: %d\n", Queue.numGroups);       strcat(message, buf);
	sprintf(buf, "Contents of queue: ");                                    strcat(message, buf);

	/* print entries */
	i = 0;
	//list_for_each_prev(temp, &animals.list) { /* backwards */
	list_for_each(temp, &Queue.list) { /* forwards*/
		g = list_entry(temp, Group, list);

		/* newline after every 5 entries */
		for( i = 0 ; i<g->groupCount ; i++)
			sprintf(buf, "%c ",g->type); strcat(message, buf);

		sprintf(buf, "(group id: %d) ",g->groupID); strcat(message, buf);
		strcat(message, "\n");
	}

	/* trailing newline to separate file from commands */
	sprintf(buf, "Number customers serviced: %d ", groupEncountered); strcat(message, buf);
	strcat(message, "\n");
	return 0;


}

static int barstool_init(void) {
        printk(KERN_INFO "barstool_init\n");
	STUB_close_bar = close_bar;
	STUB_customer_arrival = customer_arrival;
	STUB_initialize_bar = initialize_bar;
	proc_entry = proc_create("majorsbar", 0666, NULL, &procfile_fops);

	Queue.numGroups=0;
	Queue.numCustomers=0;
	INIT_LIST_HEAD(&Queue.list);

	if (proc_entry == NULL) {
		return -ENOMEM;
	}


    return 0;

}




static void barstool_exit(void){ 
	printk(KERN_INFO "barstool_exit\n");
	STUB_close_bar = NULL;
	STUB_customer_arrival = NULL;
	STUB_initialize_bar = NULL;

	proc_remove(proc_entry);
	return;
}
module_init(barstool_init);
module_exit(barstool_exit);
