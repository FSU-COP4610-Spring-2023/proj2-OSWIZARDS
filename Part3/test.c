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

         j = mutex_lock_interruptible(&thread1.procMutex);  
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
                 mutex_unlock(&thread1.procMutex);  

            }
        }
    }

   if(mutex_is_locked(&thread1.procMutex))
        mutex_unlock(&thread1.procMutex);  


    return removed;
}

static bool waiter_clean_table(void) {
    int dirty_count = 0, i;

   
        for (i = 0; i < 8; i++) {
            if (stool[8*(current_table-1) + i].status == 'D') {
                dirty_count++;
            }
        }

        
        
        if (dirty_count == 8) {
        //     if(mutex_is_locked(&thread1.procMutex))
        //         return false;
        // i = mutex_lock_interruptible(&thread1.procMutex);  
        
            for (i = 0; i < 8; i++) {
                if (stool[8*(current_table-1) + i].status == 'D' ) {
                        stool[8*(current_table-1) + i].status = 'C';
                       
                    }
                
            }   
        //mutex_unlock(&thread1.procMutex);       //it will only be locked if dirty_count>=4
        return true;
    }

    return false;
}

static int addQueue(char type, int num) {
        int group_id = groups_encountered;
        //if waiter is acessing queue wait untill its done  
        int i;
        i =  mutex_lock_interruptible(&(thread1.queueMutex));

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
    

    return 1;
}

static int deleteQueue(void) {
    Customer *c;
    int i;
    int amt;

    if (list_empty(&Queue)) //if empty
        return -1;

    /* even though the func is only used by waiter we want to lock to make sure main process is not
        accessing shared resources and editing such as "adding someone to queue" */
        i = mutex_lock_interruptible(&(thread1.queueMutex));
        c = list_first_entry(&Queue, Customer, list);
        amt = c->group_count;
        list_del(&c->list);
        mutex_unlock(&(thread1.queueMutex));    
        queue_group_num--;
        queue_customer_num -= amt;
    
    

    return 0;
}

static bool waiter_seat_customer(void) {
    Customer *c;
    int i, num = 0, seated = 0;
    // look at front customer
        // if(mutex_is_locked(&thread1.queueMutex))
        //     return false;
        i = mutex_lock_interruptible(&thread1.queueMutex);  
        c = list_first_entry(&Queue, Customer, list);
        mutex_unlock(&thread1.queueMutex);      //unlocks mutex here as after list_entry we do not acess 


    // see if current table has space
    for (i = 0; i < 8; i++) {
        if (stool[8*(current_table-1) + i].status == 'C') {
                num++; 
        }
    }
    if(num<=c->group_count)
        return false;
    
    // if(mutex_is_locked(&thread1.procMutex))
    //         return false;
    i=mutex_lock_interruptible(&thread1.procMutex);     //lock both queue and proc mutex as we are chaning both of shared contents

    if (num >= c->group_count) { // if it does
        // seatem
        for (i = 0; seated < c->group_count; i++) {
            if (stool[8*(current_table-1) + i].status == 'C') {
                stool[8*(current_table-1) + i].status = c->type;
                stool[8*(current_table-1) + i].occupant = c;
                seated++; 
            }
        }

    
    mutex_unlock(&thread1.procMutex);      
    deleteQueue();                         
    return true;

    }

    mutex_unlock(&thread1.procMutex);      
    return false;
}

static bool waiter_move_table(void) {

        //  mutex_lock_interruptible(&thread1.procMutex);
        current_table++;
        if (current_table == 5) {
            current_table = 1;
        }


   // mutex_unlock(&thread1.procMutex);
    return true;
}

static int waiter_brain(void * data) {

    struct thread_parameter *parm = data;
    bool seat;

    while (!kthread_should_stop()) {

    // we will call locks inside of fucntion that way it will be easier and it wont have to wait to be ublocked after msleep()
    // move table
        seat=0;
        if(waiter_state==OFFLINE)
            seat=0;

    else{
        if (waiter_move_table()) {
            ssleep(2);
        }

        // seat customers
        if (waiter_seat_customer()) {
            ssleep(1);
        }
        
        // else{
        //     seat=waiter_toss_customer();
        //     ssleep(1);
        // }


        // remove customers
        if (waiter_toss_customer() && !seat) {
            ssleep(1);
        }
        // else
        // {
        //     seat=waiter_clean_table();
        //     ssleep(2);
        
        // }


        // clean table
        if (waiter_clean_table() && !seat) {
            ssleep(1);
        }
        // else 
        // {
        //     waiter_move_table();
        //     ssleep(2);

        // }

        // seat customers (again)

        if (waiter_seat_customer()) {
            ssleep(1);
        }


}
    }

    return 0;
}
