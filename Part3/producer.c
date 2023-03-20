#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "wrappers.h"

int rnd(int min, int max) {
	return rand() % (max - min + 1) + min; //slight bias towards first k
}

int main(int argc, char **argv) {

	int type;
	int number_of_customers;
	int i;
	int num;
	srand(time(0));

	if (argc != 2) {
		printf("wrong number of args. producer.x num_of_requests\n");
		return -1;
	}

	sscanf(argv[1],"%d",&num);

	for(i=0; i < num;i+=1)
	{
		type = rnd(0,4);
		number_of_customers = rnd(1, 8);
		long ret = customer_arrival(number_of_customers, type);
		printf("Issue (%d, %d) returned %ld\n", number_of_customers, type, 
		ret);
	}

	return 0;
}
