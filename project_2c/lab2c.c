#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "SortedList.h"
#include <getopt.h>
#include <pthread.h>

// Linked list helper functions
#define NUM_ELEM 9
SortedListElement_t *makeNode(char *key);
void freeMemory(SortedList_t *head);
void arrayFromList(char buff[][10], SortedList_t *head);
void printData(SortedList_t *list);
void testList();
void randomString(char *s, const int len);

// Lock globals
int opt_yield;
char sync_char;
pthread_mutex_t *lock;
volatile int *exclusion;

// Lock function declarations
void* manageList(void* arg);
void initMutex(pthread_mutex_t *lock);
void deinitMutex(pthread_mutex_t *lock);
void spin_lock(int index);
void spin_unlock(int index);

struct compute_args {
  //long long *counter;
  int numIterations;
  SortedListElement_t **list; // array of nodes to insert
  SortedList_t **listHead; // head of list to insert into
};
void doOperations(struct compute_args *args);

// Multiple list declarations
int listNum;
int hashKeyToList(char *key);

int main (int argc, char **argv) {
  //testList();

  // start getting arguments
  char arg;

  static struct option long_options[] =
    {
      {"threads=", required_argument, 0, 't'},
      {"iterations=", required_argument, 0, 'i'},
      {"sync=", required_argument, 0, 's'},
      {"yield=", required_argument, 0, 'y'},
      {"lists=", required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };

  long long unsigned numThreads = 1;
  long long unsigned numIterations = 1;
  opt_yield = 0;
  listNum = 1;
  while ((arg = getopt_long_only(argc, argv, "yt:i:s:l:", long_options, NULL)) != -1)
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
	  if (strchr(optarg, 'i'))
	    opt_yield |= INSERT_YIELD;
	  if (strchr(optarg, 'd'))
	    opt_yield |= DELETE_YIELD;
	  if (strchr(optarg, 's'))
	    opt_yield |= SEARCH_YIELD;
          break;
        case 's':
          sync_char = optarg[0];
          break;
	case 'l':
	  listNum = atoi(optarg);
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

  // Initialize locking mechanisms
  if (sync_char == 'm') {
    lock = malloc(listNum * sizeof(pthread_mutex_t));
    initMutex(lock);
  } else if (sync_char == 's') {
    exclusion = malloc(listNum * sizeof(int));
    for (int i = 0; i < listNum; ++i)
      exclusion[i] = 0;
  }
  
  // Initialize head of list basedon --listNum
  SortedList_t **head;
  head = malloc(sizeof(SortedList_t *) * listNum);
  for (int i = 0; i < listNum; ++i) {
    head[i] = (SortedList_t *)makeNode(NULL);
  }

  // Randomizer
  int seed = time(NULL);
  srand(seed);
  
  SortedListElement_t*** nodes = malloc(numThreads * sizeof(SortedListElement_t *));
  int countPerRow[numThreads];
  
  // Initialize all nodes when --list=1
    for (int i = 0; i < numThreads; ++i) {
      nodes[i] = malloc(numIterations * sizeof(SortedListElement_t));
      for (int j = 0; j < numIterations; ++j) {
	char *randomKey = malloc(100);
	randomString(randomKey, rand() % 100);
	SortedListElement_t *elem = makeNode(randomKey);
	nodes[i][j] = elem;
      }
    }
  // end initializing nodes when --list=1
  
  // Setup args
  struct compute_args args[numThreads];
  for (int i = 0; i < numThreads; ++i) {
    args[i].list = nodes[i];
    args[i].numIterations = numIterations;
    args[i].listHead = head;
  }

  // Get start time for run
  struct timespec startTime, endTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  
  // Start making threads
  pthread_t t[numThreads];
  for (int i = 0; i < numThreads; ++i) {
    int ret = pthread_create(&t[i], NULL, manageList, &args[i]);
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

  // Stop timer and print num operations + time needed
  clock_gettime(CLOCK_MONOTONIC, &endTime);
  
  // Calculate number of operations
  long long numOps = numThreads * numIterations * 2;
  printf("%llu threads x %llu iterations x (insert + lookup/delete) = %llu operations\n", numThreads, numIterations, numOps);

  long long elapsedTime = 1000000000L * (endTime.tv_sec - startTime.tv_sec) + endTime.tv_nsec - startTime.tv_nsec;
  printf("elapsed time: %lluns\n", elapsedTime);
  printf("per operation: %lluns\n", elapsedTime / numOps);

  // Check final length of list
  int finalLength = 0;  
  for (int i = 0; i < listNum; ++i)
    finalLength += SortedList_length(head[i]);
  
  if (finalLength != 0) {
    fprintf(stderr, "ERROR: final length = %d\n", finalLength);
    exit(1);
  }
  for (int i = 0; i < listNum; ++i)
    freeMemory(head[i]);
  free(nodes);
  if (sync_char == 'm') {
    deinitMutex(lock);
    free(lock);
  }
  return 0;
}
void testList() {
  SortedList_t *head = (SortedList_t *)makeNode(NULL);
  
  char input[NUM_ELEM][10] = { "word1", "apple", "zebra", "bobb", "yellow", "zzzz", "abole", "crayon", "radon" };

  for (int i = 0; i < NUM_ELEM; ++i) { 
    SortedListElement_t *elem = makeNode(input[i]);
    SortedList_insert(head, elem);
    assert(SortedList_length(head) == i + 1);
  }

  char sorted[NUM_ELEM][10];
  arrayFromList(sorted, head);
  char answer[NUM_ELEM][10] = { "abole", "apple", "bobb", "crayon", "radon", "word1", "yellow", "zebra", "zzzz" };
  for (int i = 0; i < NUM_ELEM; ++i) {
    assert(strcmp(sorted[i], answer[i]) == 0);
  }
  
  assert(SortedList_length(head) == NUM_ELEM);

  for (int i = 0; i < NUM_ELEM; ++i) { 
    SortedListElement_t *elem = SortedList_lookup(head, input[i]);
    assert(elem->key == input[i]);
  }

  SortedListElement_t *firstElem = SortedList_lookup(head, "abole");
  SortedList_delete(firstElem);
  arrayFromList(sorted, head);
  assert(SortedList_length(head) == NUM_ELEM - 1);
  char answer2[NUM_ELEM - 1][10] = { "apple", "bobb", "crayon", "radon", "word1", "yellow", "zebra", "zzzz" };
  for (int i = 0; i < NUM_ELEM - 1; ++i) {
    assert(strcmp(sorted[i], answer2[i]) == 0);
  }
    
  firstElem = SortedList_lookup(head, "zzzz");
  SortedList_delete(firstElem);
  firstElem = SortedList_lookup(head, "radon");
  SortedList_delete(firstElem);
  firstElem = SortedList_lookup(head, "apple");
  SortedList_delete(firstElem);
  firstElem = SortedList_lookup(head, "zebra");
  SortedList_delete(firstElem);

  arrayFromList(sorted, head);
  assert(SortedList_length(head) == NUM_ELEM - 5);
  char answer3[NUM_ELEM - 5][10] = { "bobb", "crayon", "word1", "yellow" };
  for (int i = 0; i < NUM_ELEM - 5; ++i) {
    assert(strcmp(sorted[i], answer3[i]) == 0);
  }

  for (int i = 0; i < 3; ++i) { 
    SortedListElement_t *elem = makeNode("raking");
    SortedList_insert(head, elem);
  }
  assert(SortedList_length(head) == 7);

  arrayFromList(sorted, head);
  char answer4[7][10] = { "bobb", "crayon", "raking", "raking", "raking", "word1", "yellow" };
  for (int i = 0; i < 7; ++i) {
    assert(strcmp(sorted[i], answer4[i]) == 0);
  }

  freeMemory(head);

  printf("PASSED UNIT TESTS.\n");
}

SortedListElement_t *makeNode(char *key) {
  SortedListElement_t *elem = malloc(sizeof(SortedListElement_t));
  elem->key = key;
  elem->next = NULL;
  elem->prev = NULL;

  return elem;
}

void freeMemory(SortedList_t *head) {
  SortedListElement_t *curr = (SortedListElement_t *)head;
  SortedListElement_t *currNext = NULL;
  while (curr != NULL) {
    currNext = curr->next;
    free(curr);
    curr = currNext;
  }
}

void arrayFromList(char buff[][10], SortedList_t *head) {
  int index = 0;
  SortedListElement_t *curr = (SortedListElement_t *)head;
  while (curr != NULL) {
    if (curr->key == NULL) {
      curr = curr->next;
      continue;
    }
    strcpy(buff[index], curr->key);
    curr = curr->next;
    ++index;
  }
}

// DEBUG PURPOSE

void printData(SortedList_t *list) {
  SortedListElement_t *curr = list;
  printf("STARTLIST\n");
  while (curr != NULL) {
    if (curr->key == NULL) {
      curr = curr->next;
      continue;
    }
    printf("%s\n", curr->key);
    curr = curr->next;
  }
  printf("ENDLIST\n");
}

// sync=m option
void initMutex(pthread_mutex_t *lock) {
  for (int i = 0; i < listNum; ++i) {
    if (pthread_mutex_init(&lock[i], NULL) != 0) {
      printf("mutex init failed\n");
      return;
    }
  }
}

void deinitMutex(pthread_mutex_t *lock) {
  for (int i = 0; i < listNum; ++i) {
    pthread_mutex_destroy(&lock[i]);
  }
}

// sync=s option
void spin_lock(int index) {
  while (__sync_lock_test_and_set(&exclusion[index], 1))
    ;
}

void spin_unlock(int index) {
  __sync_lock_release(&exclusion[index]);
}
// end sync=s option

void* manageList(void* arg) {
  struct compute_args *args = arg;
  doOperations(args);   
}

void doOperations(struct compute_args *args) {
  // Insert nodes into list
  for (int i = 0; i < args->numIterations; ++i) {
    char *key = (char *)args->list[i]->key;
    int bucketNum = hashKeyToList(key);
   
    if (sync_char == 'm')
      pthread_mutex_lock(&lock[bucketNum]);
    else if (sync_char == 's')
      spin_lock(bucketNum);
    
    SortedList_insert((SortedList_t *)args->listHead[bucketNum], (SortedListElement_t *)args->list[i]);

    if (sync_char == 'm')
      pthread_mutex_unlock(&lock[bucketNum]);
    else if (sync_char == 's')
      spin_unlock(bucketNum);
  }

  for (int i = 0; i < listNum; ++i)
    SortedList_length((SortedList_t *)args->listHead[i]);

  // Delete inserted nodes from list
  for (int i = 0; i < args->numIterations; ++i) {
    char *key = (char *)args->list[i]->key;
    int bucketNum = hashKeyToList(key);
   
    if (sync_char == 'm')
      pthread_mutex_lock(&lock[bucketNum]);
    else if (sync_char == 's')
      spin_lock(bucketNum);
    
    SortedListElement_t *elem = SortedList_lookup(args->listHead[bucketNum], key);    
    SortedList_delete(elem);

    if (sync_char == 'm')
      pthread_mutex_unlock(&lock[bucketNum]);
    else if (sync_char == 's')
      spin_unlock(bucketNum);
  }
}  

void randomString(char *s, const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
      s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len] = '\0';
}

int hashKeyToList(char *key) {
  int length = strlen(key);
  int total = 0;
  for (int i = 0; i < length; ++i)
    total += key[i];
  return total % listNum;
}
