#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

#define DEBUG 1

int (*STUB_initialize_bar)(void) = NULL;
EXPORT_SYMBOL(STUB_initialize_bar);
SYSCALL_DEFINE0(initialize_bar) {
    if (DEBUG)
        printk(KERN_NOTICE "SYSCALL_DEFINE0 initialize_bar\n");

    if (STUB_initialize_bar != NULL)
        return STUB_initialize_bar();
    else
        return -ENOSYS;
}

int (*STUB_customer_arrival)(int, int) = NULL;
EXPORT_SYMBOL(STUB_customer_arrival);
SYSCALL_DEFINE2(customer_arrival, int, number_of_customers, int, type) {
    if (DEBUG)
        printk(KERN_NOTICE "SYSCALL_DEFINE2 customer_arrival\n");

    if (STUB_customer_arrival != NULL)
        return STUB_customer_arrival(number_of_customers, type);
    else
        return -ENOSYS;
}

int (*STUB_close_bar)(void) = NULL;
EXPORT_SYMBOL(STUB_close_bar);
SYSCALL_DEFINE0(close_bar) {
    if (DEBUG)
        printk(KERN_NOTICE "SYSCALL_DEFINE0 close_bar\n");

    if (STUB_close_bar != NULL)
        return STUB_close_bar();
    else
        return -ENOSYS;
}