#define _GNU_SOURCE
#include "SortedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {  
  int yield = opt_yield && INSERT_YIELD;

  // Empty list case so insert into empty list
  if (list->key == NULL && list->next == NULL) {
    list->next = element;
    element->prev = list;
  }
  // Search for correct spot to insert node somewhere in middle of list
  else {
    SortedListElement_t *curr = list;
    while (curr != NULL) {
      // insert after head of list as first node
      if (curr->key == NULL) {
        if (strcmp(element->key, curr->next->key) <= 0) {
	  if (yield)
	    pthread_yield();
	  element->next = curr->next;
          element->prev = curr;
          curr->next->prev = element;
          curr->next = element;
          return;
        }
      }
      // Insert in between 2 nodes
      else if (strcmp(element->key, curr->key) > 0 && curr->next && strcmp(element->key, curr->next->key) <= 0) {
	if (yield)
	  pthread_yield();
        element->next = curr->next;
        element->prev = curr;
        curr->next->prev = element;
        curr->next = element;
        return;
      }
      // Insert at end of list
      else if (curr->next == NULL && strcmp(element->key, curr->key) >= 0) {
	if (yield)
	  pthread_yield();
	element->next = NULL;
        element->prev = curr;
        curr->next = element;
        return;
      }
      if (yield)
	pthread_yield();      
      curr = curr->next;
    }    
  }
  return;
}

int SortedList_delete( SortedListElement_t *element) {
  int yield = opt_yield && DELETE_YIELD;
  
  SortedListElement_t *toDelete = element;
  // If deleting last node in list
  if (element->next == NULL && element->prev->next == element) {
    if (yield)
      pthread_yield();
    element->prev->next = NULL;
  }
  // If deleting a middle node, also check that next->prev and prev->next point to same address
  else if (element->prev && element->next->prev == element->prev->next) {
    if (yield)
      pthread_yield();
    // Fix pointers of neighbor nodes
    element->next->prev = element->prev;
    element->prev->next = element->next;
  }
  
  free(toDelete);
  element = NULL;
  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  int yield = opt_yield && SEARCH_YIELD;
  if (yield)
    pthread_yield();

  SortedListElement_t *curr = (SortedListElement_t *)list;
  while (curr != NULL) {
    if (curr->key && strcmp(curr->key, key) == 0)
      return curr;
    curr = curr->next;
  }
  return 0;
}

int SortedList_length(SortedList_t *list) {
  int yield = opt_yield && SEARCH_YIELD;
  if (yield)
    pthread_yield();

  int length = 0;
  SortedListElement_t *curr = (SortedListElement_t *)list;
  while (curr != NULL) {
    if (curr->key != NULL)
      ++length;
    curr = curr->next;
  }
  return length;
}
