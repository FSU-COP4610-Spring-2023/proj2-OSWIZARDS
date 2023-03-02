#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
MODULE_LICENSE("GPL");

int (*STUB_initialize_bar)(void) = NULL;
int (*STUB_customer_arrival)(int, int) = NULL;
int (*STUB_close_bar)(void) = NULL;
EXPORT_SYMBOL(STUB_initialize_bar);
EXPORT_SYMBOL(STUB_customer_arrival);
EXPORT_SYMBOL(STUB_close_bar);

SYSCALL_DEFINE0(initialize_bar)
{
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s\n", __FUNCTION__);
	if (STUB_initialize_bar != NULL)
		return STUB_initialize_bar();
	else
		return -ENOSYS;
}

SYSCALL_DEFINE2(customer_arrival, int, number_of_customers, int, type)
{
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE2 block. %s: Your number of costumers are %d and the type is %d\n", __FUNCTION__, number_of_customers, type);
        if (STUB_customer_arrival!= NULL)
                return STUB_customer_arrival(number_of_customers, type);
        else
                return -ENOSYS;

}

SYSCALL_DEFINE0( close_bar)
{
	 printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s:\n", __FUNCTION__);
        if (STUB_close_bar != NULL)
                return STUB_close_bar();
        else
                return -ENOSYS;
}

