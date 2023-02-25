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
    printf("type a command number, then the parameters\n1 for init bar (no params). 2 for cust arr (num type). 3 for close bar (no params). 4 for test call(num)\n");

    int cmd = atoi(argv[1]);
    int ret;
    switch (cmd) {
        case 1:
            ret = initialize_bar();
            break;
        case 2:
            ret = customer_arrival(atoi(argv[2]), atoi(argv[3]));
            break;
        case 3:
            ret = close_bar();
            break;
        case 4:
            ret = test_call(atoi(argv[2]));
            break;
        default:
    }

    if (ret < 0)
        perror("system call error");
    else
        printf("Function successful.");

    return 0;
}
