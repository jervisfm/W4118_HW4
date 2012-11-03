#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <gmp.h>
#include <linux/unistd.h>
#include <ctype.h>
/* For the custom System calls  */
#include "../android-tegra-3.1/arch/arm/include/asm/unistd.h"
#include "prime.h"

/* Allowable Weights values */
#define MIN_WEIGHT 1
#define MAX_WEIGHT 20

static void find_factors(mpz_t base);


static int is_number(const char *string)
{
	int i = 0;

	if (string == NULL)
		return 0;

	for (; string[i] != '\0'; ++i) {
		if (!isdigit(string[i]))
			return 0;
	}
	return 1;
}

/* Checks if given weight is valid.
 * Return 0 if false, and 1 if true.
 */
static int valid_weight(const char *string)
{
	/* Ensure that string is a valid number */
	if (!is_number(string))
		return 0;

	/* Convert Weight to int type */
	int wt = atoi(string);

	/* Ensure weight is within range */

	if (wt >= MIN_WEIGHT && wt <= MAX_WEIGHT)
		return 1;
	else
		return 0;
}

static void test(mpz_t number, const char* weight_string)
{
	int wt = atoi(weight_string);
	printf("Weight = %d\n", wt);
	printf("Finding Factors...\n");
	find_factors(number);
	printf("Factorization complete.\n");

}

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

	/* Ensure valid weight value */
	if (!valid_weight(argv[2])) {
		printf("Invalid Weight.Only integers 1-20 inclusive allowed\n");
		return EXIT_FAILURE;
	}


	test(largenum, argv[2]);
	/*
	 * TODO:
	 * - Handle weight argument here
	 * - actually do the find factors */
	// find_factors(largenum);

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
