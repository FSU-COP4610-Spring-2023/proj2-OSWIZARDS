#ifndef __WRAPPERS_H
#define __WRAPPERS_H
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#define __NR_INITIALIZE_BAR 548
#define __NR_CUSTOMER_ARRIVAL 549
#define __NR_CLOSE_BAR 550

int initialize_bar() {
	return syscall(__NR_INITIALIZE_BAR);
	}

int customer_arrival(int number_of_customers, int type) {
	return syscall(__NR_CUSTOMER_ARRIVAL, number_of_customers, type);
	}

int close_bar() {
	return syscall(__NR_CLOSE_BAR);
	}

#endif
