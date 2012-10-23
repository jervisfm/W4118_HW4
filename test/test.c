#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <gmp.h>
#include <linux/unistd.h>
#include "prime.h"

static void find_factors(mpz_t base);

int main(int argc, const char *argv[])
{
	mpz_t largenum;

	if (argc < 3) {
		printf("Usage: %s <number> <weight>\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (mpz_init_set_str(largenum, argv[1], 10) < 0) {
		perror("Invalid number");
		return EXIT_FAILURE;
	}

	/* TODO: Handle weight argument here */

	find_factors(largenum);

	return EXIT_SUCCESS;
}

/*
 * DO NOT MODIFY BELOW THIS LINE
 * -----------------------------
 */

static void find_factors(mpz_t base)
{
	char *str;
	int res;
	mpz_t i;
	mpz_t half;
	mpz_t two;

	mpz_init_set_str(two, "2", 10);
	mpz_init_set_str(i, "2", 10);
	mpz_init(half);
	mpz_cdiv_q(half, base, two);

	str = mpz_to_str(base);
	if (!str)
		return;

	/*
	 * We simply return the prime number itself if the base is prime.
	 * (We use the GMP probabilistic function with 10 repetitions).
	 */
	res = mpz_probab_prime_p(base, 10);
	if (res) {
		printf("%s is a prime number\n", str);
		free(str);
		return;
	}

	printf("Trial: prime factors for %s are:", str);
	free(str);
	do {
		if (mpz_divisible_p(base, i) && verify_is_prime(i)) {
			str = mpz_to_str(i);
			if (!str)
				return;
			printf(" %s", str);
			free(str);
		}

		mpz_nextprime(i, i);
	} while (mpz_cmp(i, half) <= 0);
	printf("\n");
}
