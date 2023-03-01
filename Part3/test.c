#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int initialize_bar (void) {
    return syscall(548);
}

int customer_arrival(int number_of_customers, int type) {
    return syscall(549, number_of_customers, type);
}

int close_bar(void) {
    return syscall(550);
}

int test_call(int test) {
    return syscall(551, test);
}

int main(int argc, char **argv) {
    int cmd = atoi(argv[1]);
    int ret;
    switch (cmd) {
        case 1:
            //printf("initializing bar...\n");
            ret = initialize_bar();
            break;
        case 2:
            //printf("customer arrival...\n");
            ret = customer_arrival(atoi(argv[2]), atoi(argv[3]));
            break;
        case 3:
            //printf("closing bar...\n");
            ret = close_bar();
            break;
        case 4:
            //printf("test call...\n");
            ret = test_call(atoi(argv[2]));
            break;
        default:
    }

    /*if (ret < 0)
        perror("system call error\n");
    else
        printf("Function successful.\n");*/

    return 0;
}
