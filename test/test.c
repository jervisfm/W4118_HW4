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
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h> /* high-res timers */
#include <sched.h> /* for std. scheduling system calls */
/* For the custom System calls  */
#include "../android-tegra-3.1/arch/arm/include/asm/unistd.h"
#include "prime.h"

/* Allowable Weights values */
#define MIN_WEIGHT 1
#define MAX_WEIGHT 20

/* Add the relevant definition policies in the scheduler */
#define SCHED_NORMAL		0
#define SCHED_FIFO		1
#define SCHED_RR		2
#define SCHED_BATCH		3
/* SCHED_ISO: reserved but not implemented yet */
#define SCHED_IDLE		5
#define SCHED_WRR		6

static struct timeval start_time;
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

static int is_wrr_policy(int policy)
{
	return policy == SCHED_WRR;
}

/* Get */
static char *get_policy_name(int policy)
{
	char *result = calloc(100, sizeof(char));
	if (result == NULL) {
		printf("Memory allocation failed in get_policy_name");
		printf("Aborting...");
		exit(1);
	}

	switch (policy) {
		case SCHED_NORMAL:{
			char *s = "CFS: Normal";
			strncpy(result,s,strlen(s));
			break;
		}
		case SCHED_FIFO:{
			char *s = "RT: FIFO";
			strncpy(result,s,strlen(s));
			break;
		}
		case SCHED_RR:{
			char *s = "RT: Round Robin";
			strncpy(result,s,strlen(s));
			break;
		}
		case SCHED_BATCH:{
			char *s = "CFS: Batch";
			strncpy(result,s,strlen(s));
			break;
		}
		case SCHED_IDLE:{
			char *s = "CFS: Idle";
			strncpy(result,s,strlen(s));
			break;
		}
		case SCHED_WRR:{
			char *s = "Custom: Weighted Round Robin";
			strncpy(result,s,strlen(s));
			break;
		}
		default: {
			char *s = "Unknown Policy";
			strncpy(result,s,strlen(s));
			break;
		}
	}
	return result;
}

static void start_timer()
{
	int ret;
	ret = gettimeofday(&start_time, NULL);
	if (ret != 0 )
		return perror("Warning: GetTime in start_time() failed!");
}
/* Returns the number of seconds that have elapsed
 * since start_timer was called */
static double stop_timer()
{
	int ret;
	struct timeval now, diff;

	ret = gettimeofday(&now, NULL);
	if (ret != 0) {
		perror("Warning: GetTime in stop_timer() failed!");
		return -1;
	}

	/* find the difference */
	diff.tv_sec = now.tv_sec - start_time.tv_sec;
	diff.tv_usec = now.tv_usec - start_time.tv_usec;

	return diff.tv_sec + (diff.tv_usec / pow(10, 6));
}

/* Prints the currently assigned scheduler of this process */
static void print_scheduler()
{
	int ret;
	char *policy;
	pid_t pid = getpid();

	if (pid < 0) {
		perror("get pid call failed. aborting from print_scheduler...");
		exit(-1);
	}

	ret = sched_getscheduler(pid);

	if (ret < 0) {
		perror("Getscheduler failed. Aborting...");
		exit(-1);
	}

	policy = get_policy_name(ret);
	printf("Current Policy: %s\n", policy);
	free(policy);
}

/* Test Function to change scheduling policy*/
static void change_scheduler()
{
	int ret;
	struct sched_param param;
	pid_t pid = getpid();
	int policy = SCHED_WRR;
	if (pid < 0) {
		perror("Get PID call failed. Aborting...");
		exit(-1);
	}

	printf("Changing Scheduler for PID %d\n", pid);

	ret = sched_setscheduler(pid, policy, &param);
	if (ret < 0) {
		perror("Changing scheduler failed.");
		exit(-1);
	}
}
/* Test function : print weight of given pid */
static void print_weight(pid_t pid)
{
	int ret;
	if (pid < 0)
		return;
	ret = syscall(__NR_sched_getweight, pid);
	if (ret < 0)
		return perror("get_weight call failed");
	printf("Weight of Process %d is %d\n", pid, ret);
}
/* Test function : Print weight of current process */
static void print_current_weight()
{
	pid_t pid = getpid();
	if (pid < 0) {
		perror("Get PID Call failed. Aborting ...");
		exit(-1);
	}
	print_weight(pid);
}

static void test_change()
{
	return;
	printf("Before Change:\n");
	print_scheduler();
	printf("Changing Scheduler...\n");
	change_scheduler();
	printf("After Change:\n");
	print_scheduler();

}

static void do_nothing()
{
	return;
	print_current_weight();
	is_wrr_policy(0);
	print_scheduler();
	change_scheduler();
}

static void test(mpz_t number, const char* weight_string)
{
	double run_time;
	int wt = atoi(weight_string);
	do_nothing(); /* make the compiler shut up */
	printf("Input Weight = %d\n", wt);

	test_change();
	print_current_weight();
	printf("Finding Factors...\n");
	start_timer();
	find_factors(number);
	run_time = stop_timer();
	printf("Factorization completed in %f seconds.\n", run_time);



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
