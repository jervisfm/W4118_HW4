/*
 * infinite_loop.c
 *
 *  Created on: Nov 8, 2012
 *      Author: w4118
 */

#include <stdio.h>

int main(int argc, char **argv)
{
	printf("Hello World");
	long counter = 0;
	while(1) {
		++counter;
		printf("%ld\n", counter);
	}
}
