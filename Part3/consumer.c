#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "wrappers.h"

int main(int argc, char **argv) {
	long ret;

	if (argc != 2) {
		printf("wrong number of args\n");
		return -1;
	}

	if (strcmp(argv[1], "--start") == 0) {
		ret = initialize_bar();
		printf("Initialize_bar returned %ld\n", ret);
	}

	else if (strcmp(argv[1], "--stop") == 0) {
		ret = close_bar();
		printf("Close bar returned %ld\n", ret);
	}

	else
		return -1;
	return 0;
}
