#include <unistd.h>

int main () {
    alarm(4);
    alarm(3);
    alarm(2);
    alarm(1);
    return 0;
}