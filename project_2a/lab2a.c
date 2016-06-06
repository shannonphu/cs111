#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>

void add(long long *pointer, long long value);
void* compute(void* arg);

void initMutex(pthread_mutex_t *lock);
void add_m(long long *pointer, long long value, pthread_mutex_t *lock);
void add_s(long long *pointer, long long value);

void spin_lock();
void spin_unlock();

struct compute_args {
  long long *counter;
  int numIterations;
};

int opt_yield;
char sync_char;
volatile int exclusion = 0;
pthread_mutex_t lock;
int sync_c_lock;

int main (int argc, char **argv) {
  
  // start getting arguments
  char arg;

  static struct option long_options[] =
    {
      {"threads=", required_argument, 0, 't'},
      {"iterations=", required_argument, 0, 'i'},
      {"sync=", required_argument, 0, 's'},
      {"yield", no_argument, 0, 'y'},
      {0, 0, 0, 0}
    };

  long long unsigned numThreads = 1;
  long long unsigned numIterations = 1;
  opt_yield = 0;
  while ((arg = getopt_long_only(argc, argv, "yt:i:s:", long_options, NULL)) != -1)
    {
      switch (arg)
        {
        case 't':
	  numThreads = atoi(optarg);
          break;
        case 'i':
	  numIterations = atoi(optarg);
          break;
	case 'y':
	  opt_yield = 1;
	  break;
	case 's':
	  sync_char = optarg[0];

	  if (sync_char == 'm')
	    initMutex(&lock);
	  else if (sync_char == 'c')
	    sync_c_lock = 0;

	  break;
	case '?':
	  exit(1);
	  break;
        default:
          fprintf(stderr, "Invalid argument.\n");
	  break;
        }
  } // endwhile
  // end getting arguments

  long long *counter = malloc(sizeof(long long));
  *counter = 0;

  // Get start time for run
  struct timespec startTime, endTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);

  // Start making threads
  pthread_t t[numThreads];
  for (int i = 0; i < numThreads; ++i) {
    struct compute_args args;
    args.counter = counter;
    args.numIterations = numIterations;
    int ret = pthread_create(&t[i], NULL, compute, &args);
    if (ret != 0) {
      fprintf(stderr, "Could not make thread.\n");
      exit(1);
    }
  }
  for (int i = 0; i < numThreads; ++i) {
    int ret = pthread_join(t[i], NULL);
    if (ret != 0) {
      fprintf(stderr, "Could not join thread.\n");
      exit(1);
    }
  }
  // end making threads

  if (sync_char == 'm')
      pthread_mutex_destroy(&lock);

  // Stop timer and print num operations + time needed
  clock_gettime(CLOCK_MONOTONIC, &endTime);

  // Calculate number of operations
  long long numOps = numThreads * numIterations * 2;
  printf("%llu threads x %llu iterations x (add + subtract) = %llu operations\n", numThreads, numIterations, numOps);

  long long elapsedTime = 1000000000L * (endTime.tv_sec - startTime.tv_sec) + endTime.tv_nsec - startTime.tv_nsec;
  printf("elapsed time: %llu ns\n", elapsedTime);
  printf("per operation: %llu ns\n", elapsedTime / numOps);
  
  // Check final value of counter
  if (*counter != 0) {
    fprintf(stderr, "ERROR: final count = %llu\n", *counter);
    free(counter);
    exit(1);
  }
  printf("Final value of counter: %llu \n", *counter);
  
  // Free memory from allocating counter
  free(counter);
  exit(0);
}

// default case
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    pthread_yield();
  *pointer = sum;
}


// sync=m option
void initMutex(pthread_mutex_t *lock) {
  if (pthread_mutex_init(lock, NULL) != 0)
  {
      printf("mutex init failed\n");
      return;
  }
}


// sync=s option
void spin_lock() {
  while (__sync_lock_test_and_set(&exclusion, 1))
    ;
}

void spin_unlock() {
  __sync_lock_release(&exclusion);
}
// end sync=s option

// sync=c option

void add_c(long long *pointer, long long value) {
  if (opt_yield)
    pthread_yield();

  long long prev = *pointer;
  while(__sync_val_compare_and_swap(pointer, prev, prev + value) != prev) {
    prev = *pointer;
  }
}

// end sync=c option

void* compute(void* arg) {
  struct compute_args *args = arg;

  for (int i = 0; i < args->numIterations; ++i) {
    switch (sync_char) {
    case 'm':
      pthread_mutex_lock(&lock);
      add(args->counter, 1);
      add(args->counter, -1);
      pthread_mutex_unlock(&lock);
      break;
    case 's':
      spin_lock();      
      add(args->counter, 1);
      add(args->counter, -1);
      spin_unlock();
      break;
    case 'c':
      add_c(args->counter, 1);
      add_c(args->counter, -1);
      break;
    default:
      add(args->counter, 1);
      add(args->counter, -1);      
      break;
    }
  }
}

