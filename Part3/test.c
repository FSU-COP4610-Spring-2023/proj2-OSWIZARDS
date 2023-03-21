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
#include <linux/mutex.h>
#include <linux/kthread.h>



MODULE_LICENSE("Dual BSD/GPL");

// TODO: possible issue with too many entries in queue pushing over char limit
// len w no queue: ~400 ch, each group in queue ~50 ch
#define BUF_LEN 1048576
static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int procfs_buf_len;

static bool OPEN;
static struct timespec64 time;
int prevTime;

enum states {OFFLINE, IDLE, LOADING, CLEANING, MOVING};
static enum states waiter_state;
static int current_table,
    occupancy,
    queue_group_num,
    queue_customer_num,
    serviced_customers,
    groups_encountered;


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

struct thread_parameter {
	int id;
	struct task_struct *kthread;
	struct mutex queueMutex;
    struct mutex procMutex;

};

struct thread_parameter thread1;



static Customer last_customer;
static Place stool[32];

#define DEBUG 1

static bool waiter_toss_customer(void) {
    int req_time;
    int i,j;
    Customer *c;
    bool removed = false;
    struct timespec64 ctime;

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
                j=mutex_lock_interruptible(&thread1.procMutex); 
                stool[8*(current_table-1) + i].status = 'D';
                // once the customer leaves the chair, they are dead to us.
	           mutex_unlock(&thread1.procMutex);

            }

        }
    }

    if(removed==true)
    {    waiter_state=LOADING;
         c=NULL;
        for( i = 0 ; i < 8; i++)
        {
            if(stool[8*(current_table-1) + i].occupant!=c && stool[8*(current_table-1) + i].status=='D')
            {   serviced_customers+=stool[8*(current_table-1) + i].occupant->group_count;
		kfree(stool[8*(current_table-1) + i].occupant); 
                c=stool[8*(current_table-1) + i].occupant;
            }

        }
    }
    
    return removed;
}

static bool waiter_clean_table(void) {
    int dirty_count = 0, i,j;

    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'D') {
            dirty_count++;
        }
    }

    if(dirty_count<4)
        return false;


    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'D'  ) {
            j=mutex_lock_interruptible(&thread1.procMutex);
            stool[8*(current_table-1) + i].status = 'C';
            mutex_unlock(&thread1.procMutex);
        }
    }
    
        return true;


}

static void addQueue(char type, int num) {
    int group_id = groups_encountered;


    //if waiter is acessing queue wait untill its done  
    if(mutex_lock_interruptible(&(thread1.queueMutex))==0){
        Customer* new_cus = kmalloc(sizeof(Customer), __GFP_NOFAIL);
        new_cus->type = type;
        new_cus->group_id = group_id;
        new_cus->group_count = num;
        INIT_LIST_HEAD(&(new_cus->list));

            if (type == 'F' || type=='O') {
                list_add(&(new_cus->list), &Queue);
            }
            else {
                list_add_tail(&(new_cus->list), &Queue);
            }

	    queue_group_num++;
	    queue_customer_num += num;
        mutex_unlock(&(thread1.queueMutex));
    }

}

static Customer*  deleteQueue(void) {
    Customer *c, *temp;
    int amt;

	if (list_empty(&Queue)) //if empty
		return NULL;

    /* even though the func is only used by waiter we want to lock to make sure main process is not
        accessing shared resources and editing such as "adding someone to queue" */
    if(mutex_lock_interruptible(&(thread1.queueMutex))==0){
        c = list_first_entry(&Queue, Customer, list);
        amt = c->group_count;
        temp=c;
        list_del(&c->list);

        queue_group_num--;
        queue_customer_num -= amt;
    }
    mutex_unlock(&(thread1.queueMutex));	

	return temp;
}

static bool waiter_seat_customer(void) {
    Customer *c;
    int i, num = 0, seated = 0;
    // look at front customer
    i=mutex_lock_interruptible(&thread1.queueMutex);  
    c = list_first_entry(&Queue, Customer, list);
    mutex_unlock(&thread1.queueMutex);      //unlocks mutex so that  Deletequeue can now delete content
    ktime_get_real_ts64(&c->time);

    // see if current table has space
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'C') {
            	num++; 
        }
    }
    if(queue_group_num==0)
        return false;

    if (num >= c->group_count) { // if it does
        // seatem
	i=mutex_lock_interruptible(&thread1.procMutex);  
        for (i = 0; seated < c->group_count; i++) {
            if (stool[8*(current_table-1) + i].status == 'C') {
                 waiter_state=LOADING;
                stool[8*(current_table-1) + i].status = c->type;
                stool[8*(current_table-1) + i].occupant = c;
                seated++; 
            }
        }

        mutex_unlock(&thread1.procMutex);      //unlocks mutex so that  Deletequeue can now delete content
        deleteQueue();                          // while in the function we lock it to make sure no one else its editing queue
        return true;
    }

    return false;
}

static bool waiter_move_table(void) {
        waiter_state=MOVING;
        current_table++;
        if (current_table == 5) {
            current_table = 1;
        }

    return true;
}

static int waiter_brain(void * data) {

    // struct thread_parameter *parm = data;

    while (!kthread_should_stop()) {

    // we will call locks inside of fucntion that way it will be easier and it wont have to wait to be ublocked after msleep()
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

    return 0;
}

static bool isNotClean(void)
{
    int i;
    for(i=0;i<32;i++)
    {
        if(stool[i].status!='C')
            return true;
    }

    return false;
}
static void cleanBar(void)
{
    int i;
    for(i=0;i<32;i++)
    {
        if(stool[i].status=='D')
        {
            stool[i].status='C';
        }

        else
        {
            if(stool[i].occupant==NULL)
                kfree(stool[i].occupant);
            stool[i].status='C';
        }
    }

}
static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos) {
    char s1[10], s2[50], s3[5];
    struct timespec64 ctime;
    // struct list_head* temp;

    int  i, fr=0, so=0, ju=0, se=0, pr=0, c_id, j;
    Customer *c;

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
    if(waiter_state!=OFFLINE)
    {   ktime_get_real_ts64(&ctime);
        prevTime = ctime.tv_sec - time.tv_sec;
        if (ctime.tv_nsec >= 500000000) 
            prevTime++;
    }



    
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

    sprintf(msg, "Waiter state: %s\nCurrent table: %i\nElapsed time: %i seconds\nCurrent occupancy: %i\n", s1, current_table, prevTime, occupancy);
    sprintf(msg, "%sBar status: %s\nNumber of customers waiting: %i\nNumber of groups waiting: %i\n", msg, s2, queue_customer_num, queue_group_num);
    sprintf(msg, "%sContents of queue:\n", msg);
    
    // queue contents
    
    if (list_empty(&Queue) == 0) { // if not empty
        
        i= mutex_lock_interruptible(&(thread1.queueMutex));
        list_for_each_entry(c, &Queue, list) {      //iterates through list and makes c point to cur node
            c_id = c->group_id;
            //c2 = list_entry (temp, Customer, list);

            for(j=0;j<(c->group_count);j++)     //prints as many
            sprintf(msg, "%s%c ", msg, c->type);
            
            if ((c->group_count) > 0) {
                sprintf(msg, "%s(group id: %i)\n", msg, c->group_id);
            }
        }
        
        mutex_unlock(&thread1.queueMutex);

    }
    else {
        sprintf(msg, "%sEmpty\n", msg);
    }

    sprintf(msg, "%sNumber of customers serviced: %i\n\n\n", msg, serviced_customers);

    i=mutex_lock_interruptible(&(thread1.procMutex));
    for (i = 4; i >= 1; i--) {
        strcpy(s3, (current_table == i) ? "[*]" : "[ ]");
        sprintf(msg, "%s%s Table %i: ", msg, s3, i);
        for (j = (i-1)*8; j < (i-1)*8 + 8; j++) {
            sprintf(msg, "%s%c ", msg, stool[j].status);
        }
        sprintf(msg, "%s\n", msg);
    }
    mutex_unlock(&thread1.procMutex);



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

//     	i=mutex_lock_interruptible(&thread1.procMutex);
	i = copy_from_user(msg, ubuf, procfs_buf_len);
//	 mutex_lock(&thread1.procMutex);

	return procfs_buf_len;
}

static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};

extern int (*STUB_initialize_bar)(void);
int initialize_bar(void) {
    int i;

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
    mutex_init(&thread1.queueMutex);    //intitalize both mutex
    mutex_init(&thread1.procMutex);
    thread1.kthread = kthread_run(waiter_brain, &thread1 , "waiter thread \n");
    if (IS_ERR(thread1.kthread)) {
		printk(KERN_WARNING "error spawning thread");
		return -1;
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
    int i;
    Customer * temp;

    if (OPEN == true) {
        OPEN = false;
    }
    else {
        return 1;
    }

    while (!list_empty(&Queue))
    {   temp = deleteQueue();
        kfree(temp);
    }

    if(isNotClean())
        cleanBar();

    mutex_destroy(&thread1.queueMutex);         //destroy the mutex so you dont have a deadlock
    mutex_destroy(&thread1.procMutex);         // it should be done before 
    i = kthread_stop(thread1.kthread);          //stop thread
    waiter_state=OFFLINE;

   if (i != -EINTR)                        //checks if thread did actually stop
    {    printk("Waiter thread has stopped\n");

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
    last_customer.group_id = -1;
    last_customer.type = 'X';
    waiter_state=OFFLINE;
    current_table = 1;
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

	proc_remove(proc_entry);
	return;
}

module_init(barstool_init);
module_exit(barstool_exit);
static bool waiter_toss_customer(void) {
    // variables
    int req_time;
    int i,j;
    Customer *c;
    bool removed = false;
    struct timespec64 ctime;

    // go through each customer for the current table
    for (i = 0; i < 8; i++) {
        req_time = -1; // initialize the time it takes the customer to drink
        switch(stool[8*(current_table-1) + i].status) { // check what customer type and
            case 'F': // freshman			// get the drinking time
                req_time = 5; // 5 seconds to drink
                break;
            case 'O': // sophomore
                req_time = 10; // 10 seconds to drink
                break;
            case 'J': // junior 
                req_time = 15; // 15 seconds to drink
                break;
            case 'S': // senior
                req_time = 20; // 20 seconds to drink
                break;
            case 'P': // professor
                req_time = 25; // 25 seconds to drink
                break;
        }

        if (req_time != -1) { // if a customer is seated there
            c = stool[8*(current_table-1) + i].occupant; // c = the customer
            ktime_get_real_ts64(&ctime); // update current time
            // if they've had enough time
            if (ctime.tv_sec - c->time.tv_sec >= req_time) {
                removed = true;
                j=mutex_lock_interruptible(&thread1.procMutex); // wait for proc unlock, then lock
                stool[8*(current_table-1) + i].status = 'D';
                // once the customer leaves the chair, they are dead to us.
	           mutex_unlock(&thread1.procMutex); // unlock the procfile thread
            }
        }
    }

    if(removed==true) // if the waiter removed a customer
    {    waiter_state=LOADING; // update waiter state
         c=NULL; // reset customer to null
        for( i = 0 ; i < 8; i++) // go through the customers
	{
            if(stool[8*(current_table-1) + i].occupant!=c && stool[8*(current_table-1) + i]
	    .status=='D')
            {   // increment the number of customers serviced
		serviced_customers+=stool[8*(current_table-1) + i].occupant->group_count;
		kfree(stool[8*(current_table-1) + i].occupant);  // free the stool spot
                c=stool[8*(current_table-1) + i].occupant; // to make sure the customers
            }                                              // are the same group

        }
    }

    return removed; // return true or false if a customer was removed
}

/* Function for the waiter to clean the table */
static bool waiter_clean_table(void) {
    // variables
    int dirty_count = 0, i,j;

    // go through the customers for the current table
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'D') { // if dirty
            dirty_count++; // increment the number of dirty stools
        }
    }

    if(dirty_count<4) // for efficiency to not clean tables that don't have
        return false; // many dirty stools

    // go through the stools
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'D'  ) { // if stool is dirty
            j=mutex_lock_interruptible(&thread1.procMutex); // wait for the proc unlock, then lock
            stool[8*(current_table-1) + i].status = 'C'; //  clean the table
            mutex_unlock(&thread1.procMutex); // unlock the proc file lock so it can be edited
        }
    }

    return true; // return true
}

/* Function to add customers to the queue */
static void addQueue(char type, int num) {
    // variables
    int group_id = groups_encountered;

    //if waiter is acessing queue wait until it's done
    if(mutex_lock_interruptible(&(thread1.queueMutex))==0){
	// Make a new customer
        Customer* new_cus = kmalloc(sizeof(Customer), __GFP_NOFAIL);
        new_cus->type = type;
        new_cus->group_id = group_id;
        new_cus->group_count = num;
        INIT_LIST_HEAD(&(new_cus->list)); // make the new customer the head of list

        if (type == 'F' || type=='O') { // if they are quick drinkers, prioritize them
            list_add(&(new_cus->list), &Queue);
        }

        else { // else put them to the back of the queue
            list_add_tail(&(new_cus->list), &Queue);
        }

	queue_group_num++; // increment number of groups in queue
	queue_customer_num += num; // increment number of customers in queue
        mutex_unlock(&(thread1.queueMutex)); // unlock the queue lock to free waiter changes
    }
}

/* Delete the queue */
static Customer* deleteQueue(void) {
    // variables
    Customer *c, *temp;
    int amt;

    if (list_empty(&Queue)) //if empty return
	return NULL;

    /*even though the func is only used by waiter we want to lock to make sure main process is not
      accessing shared resources and editing such as "adding someone to queue" */
    if(mutex_lock_interruptible(&(thread1.queueMutex))==0) {
        // removing the customer from the list
        c = list_first_entry(&Queue, Customer, list);
        temp=c;
        amt = c->group_count;
        list_del(&c->list);

        queue_group_num--; // decrement the # of groups in queue
        queue_customer_num -= amt; // decrement # of customers in queue
    }

    mutex_unlock(&(thread1.queueMutex)); // free the main process editing queue

    return temp;
}

/* Function for the waiter to seat a customer */
static bool waiter_seat_customer(void) {
    // variables
    Customer *c;
    int i, num = 0, seated = 0;
    i=mutex_lock_interruptible(&thread1.queueMutex);  
    c = list_first_entry(&Queue, Customer, list);
    mutex_unlock(&thread1.queueMutex); // unlocks mutex so that  deleteQueue() can now delete
    ktime_get_real_ts64(&c->time); // update time since customer arrived

    // see if current table has space
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'C') {
            num++; // number of free seats
        }
    }

    if (num >= c->group_count) { // if it number of free seats is less than group size
        // seat them
	i=mutex_lock_interruptible(&thread1.procMutex); // wait for proc to be unlocked, then lock
        waiter_state=LOADING; // update waiter state
        for (i = 0; seated < c->group_count; i++) {
            if (stool[8*(current_table-1) + i].status == 'C') {
		// update the values of that seat to match the customer
                stool[8*(current_table-1) + i].status = c->type;
                stool[8*(current_table-1) + i].occupant = c;
                seated++; 
            }
        }

        mutex_unlock(&thread1.procMutex);  // unlocks mutex so that deleteQueue() can now delete
        deleteQueue(); // while in the function we lock to make sure other threads cant edit queue
        return true; // return true
    }

    return false; // return false if didn't seat
}

/* Function for the waiter to move to the next different table */
static bool waiter_move_table(void) {
    waiter_state=MOVING; // update waiter state
    current_table++; // change to next table: circular 1->2->3->4

    if (current_table == 5) { // to go from 4->1
        current_table = 1;
    }

    return true;
}

/* Main function for waiter to make various actions */
static int waiter_brain(void * data) {

    // waiter thread constantly just moves through this function */
    while (!kthread_should_stop()) {

    // we will call locks inside of fucntion that way it will be easier and it wont
    // have to wait to be ublocked after msleep()
        // move table
        if (waiter_move_table()) {
            msleep(2000); // sleep for two seconds
        }

        // seat customers
        if (waiter_seat_customer()) {
            msleep(1000); // sleep for one second
        }


        // remove customers
        if (waiter_toss_customer()) {
            msleep(1000); // sleep for one second
        }


        // clean table
        if (waiter_clean_table()) {
            msleep(10000); // sleep for 10 seconds
        }

        // seat customers (again)
        if (waiter_seat_customer()) {
            msleep(1000); // sleep for one second
        }



    }

    return 0;
}

/* Function to check if bar is clean */
static bool isNotClean(void)
{
    int i;
    for(i=0;i<32;i++) // go through the stools
    {
        if(stool[i].status!='C') // if a single stoool is not clean, return true
            return true;
    }

    return false; // if all stools are clean, return false
}

/* Function to clean whole bar */
static void cleanBar(void)
{
    int i;
    for(i=0;i<32;i++) { // go through the stools
        if(stool[i].status=='D') // if dirty
        {
            stool[i].status='C'; // clean
        }

        else // if clean
        {
            if(stool[i].occupant==NULL) // and if no customer is seated
                kfree(stool[i].occupant); // free the stool
            stool[i].status='C'; // clean
        }
    }
}

/* Primary procfile read function */
static ssize_t procfile_read(struct file* file, char * ubuf, size_t count, loff_t *ppos) {
    // variables
    char s1[10], s2[50], s3[5];
    struct timespec64 ctime;
    int i1, i, fr=0, so=0, ju=0, se=0, pr=0, c_id, j;
    Customer *c;

    // update waiter state
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

    ktime_get_real_ts64(&ctime); // update current time
    i1 = ctime.tv_sec - time.tv_sec; // get time (i1) since bar was started
    if (ctime.tv_nsec >= 500000000) 
        i1++;

    if (occupancy == 0) { // if bar is empty, write empty for bar status
        strcpy(s2, "Empty");
    }

    else {
        for (i = 0; i < 32; i++) { // go through the stools, incrementing # of each customer type
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

    // update the proc file msg
    sprintf(msg, "Waiter state: %s\nCurrent table: %i\n", s1, current_table);
    sprintf(msg, "%sElapsed time: %i seconds\nCurrent occupancy: %i\n", msg, i1, occupancy);
    sprintf(msg, "%sBar status: %s\nNumber of customers waiting: %i\n", msg, s2, queue_customer_num);
    sprintf(msg, "%sNumber of groups waiting: %i\n", msg, queue_group_num);
    sprintf(msg, "%sContents of queue:\n", msg);

    // queue contents
    if (list_empty(&Queue) == 0) { // if not empty
	// wait for the edit freedom for the queue, then lock it
        i= mutex_lock_interruptible(&(thread1.queueMutex));
        list_for_each_entry(c, &Queue, list) { //iterates through list making c point to cur node
        c_id = c->group_id;

            for(j=0;j<(c->group_count);j++)     //prints as many
                sprintf(msg, "%s%c ", msg, c->type);

            if ((c->group_count) > 0) 
                sprintf(msg, "%s(group id: %i)\n", msg, c->group_id);
        }

        mutex_unlock(&thread1.queueMutex); // unlock the mutex
    }

    else // if bar is empty
        sprintf(msg, "%sEmpty\n", msg);

    // updating msg with # of customers serviced
    sprintf(msg, "%sNumber of customers serviced: %i\n\n\n", msg, serviced_customers);

    i=mutex_lock_interruptible(&(thread1.procMutex)); // lock other threads changing proc
    for (i = 4; i >= 1; i--) { // go through the tables
	// print the tables
        strcpy(s3, (current_table == i) ? "[*]" : "[ ]");
        sprintf(msg, "%s%s Table %i: ", msg, s3, i);
        for (j = (i-1)*8; j < (i-1)*8 + 8; j++) //print the customers
            sprintf(msg, "%s%c ", msg, stool[j].status);
        sprintf(msg, "%s\n", msg);
    }
    mutex_unlock(&thread1.procMutex); // unlock the proc mutex

    // update proc file
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

/* Main procfile write function */
static ssize_t procfile_write(struct file* file, const char * ubuf, size_t count, loff_t* ppos) {
    int i;

    if (count > BUF_LEN) {
		procfs_buf_len = BUF_LEN;
    }
	else {
		procfs_buf_len = count;
    }

//     	i=mutex_lock_interruptible(&thread1.procMutex);
	i = copy_from_user(msg, ubuf, procfs_buf_len);
//	 mutex_lock(&thread1.procMutex);

	return procfs_buf_len;
}

// proc_ops struct
static struct proc_ops procfile_fops = {
	.proc_read = procfile_read,
	.proc_write = procfile_write,
};

/* Function to initialize bar */
extern int (*STUB_initialize_bar)(void);
int initialize_bar(void) {
    int i;

    // if it's now open
    if (OPEN) {
        return 1; // return 1
    }

    // if fails to allocate memory
    if (false) {
        return -1;
    }

   // set it to open
    OPEN = true;
    waiter_state = IDLE; // set waiter state to idle
	ktime_get_real_ts64(&time); // update the running time
    current_table = 1; // set current table to 1
    occupancy = 0; // set occupancy to 0
    groups_encountered = 0; // set groups to 0

    // set the stool statuses to all clean
    for (i = 0; i < 32; i++) {
        stool[i].status = 'C';
    }

    // setup waiter
    mutex_init(&thread1.queueMutex);    //intitalize both mutex
    mutex_init(&thread1.procMutex);
    thread1.kthread = kthread_run(waiter_brain, &thread1 , "waiter thread \n"); // start waiter

    if (IS_ERR(thread1.kthread)) { // if there is an error when starting waiter thread
	printk(KERN_WARNING "error spawning thread");
	return -1;
    }

	return 0; // return 0
}

/* Function for customer arrival */
extern int (*STUB_customer_arrival)(int, int);
int customer_arrival(int number_of_customers, int type) {
    char c; // customer type

    if (!OPEN) { // if bar is not yet open 
        return 0;// return 0
    }

    if (DEBUG) { // if debuging, update kernel log
        printk(KERN_INFO "customer_arrival");
    }

    if (number_of_customers > 8) { // if trying to add more than 8 customers in a group
        printk(KERN_INFO "invalid customer amount %i\n", number_of_customers);
        return 1;
    }

    switch (type) { // set the status of the customer based on the int (convert)
        case 0:
            c = 'F'; // freshman
            break;
        case 1:
            c = 'O'; // sophomore
            break;
        case 2:
            c = 'J'; // junior
            break;
        case 3:
            c = 'S'; // senior
            break;
        case 4:
            c = 'P'; // professor
            break;
        default: // for kernel log
            printk(KERN_INFO "invalid customer type %i\n", type);
            c = 'X';
            return 1;

    }

    // update groups encountered values
    groups_encountered++;

    addQueue(c, number_of_customers); // add the customers/group

    return 0; // return 0
}

/* Function for closing the bar */
extern int (*STUB_close_bar)(void);
int close_bar(void) {
    int i;

    if (OPEN == true) { // make sure bar is open to close it
        OPEN = false;
    }
    else {
        return 1;
    }

    while (!list_empty(&Queue)) // while the queue is not empty
        deleteQueue(); // empty the queue

    if(isNotClean()) // while the bar is not clean
        cleanBar(); // clean the bar

    mutex_destroy(&thread1.queueMutex); //destroy the mutex so you dont have a deadlock
    mutex_destroy(&thread1.procMutex);  // it should be done before
    i = kthread_stop(thread1.kthread);  //stop thread
    waiter_state=OFFLINE; // waiter state to offline

    if (i != -EINTR) //checks if thread did actually stop
    {
	printk("Waiter thread has stopped\n"); // update kernel log
    }

	return 0;
}

/* Barstool initialize the kernel module */
static int barstool_init(void) {
    if (DEBUG) { // if debuging
        printk(KERN_INFO "barstool_init\n");
    }

    // create proc file
    proc_entry = proc_create("majorsbar", 0666, NULL, &procfile_fops);
	strcpy(msg, "");

    // set the syscalls to the right functions
    STUB_close_bar = close_bar;
    STUB_customer_arrival = customer_arrival;
    STUB_initialize_bar = initialize_bar;
    // update starting values
    waiter_state=OFFLINE;
    current_table = 1;
    OPEN = false;

    if (proc_entry == NULL) {
	return -ENOMEM;
    }

    return 0;
}

/* Barstool exit function */
static void barstool_exit(void)
{
    if (DEBUG) { // if debuging
        printk(KERN_INFO "barstool_exit\n");
    }

    // Set syscall function values to NULL
    STUB_close_bar = NULL;
    STUB_customer_arrival = NULL;
    STUB_initialize_bar = NULL;

    // remove the proc file
    proc_remove(proc_entry);
    return;
}

// init exit
module_init(barstool_init);
module_exit(barstool_exit);