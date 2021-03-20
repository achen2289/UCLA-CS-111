// NAME: Alex Chen
// EMAIL: achen2289@gmail.com
// ID: 005299047

#include "SortedList.h"
#include <stddef.h>
#include <string.h>
#include <sched.h>

// Insert an element into a SortedList
// Check that SortedList, element, and element's key are non-NULL
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
    if (list == NULL || element == NULL || element->key == NULL) {
        return;
    }
    SortedListElement_t *curr = list;
    while (curr->next != list && strcmp(element->key, curr->next->key) > 0){
        curr = curr->next;
    }
    if (opt_yield & INSERT_YIELD) {
        sched_yield();
    }
    SortedListElement_t *old_next = curr->next;
    curr->next = element;
    element->prev = curr;
    element->next = old_next;
    old_next->prev = element;
}

// Delete element from SortedList
int SortedList_delete(SortedListElement_t *element) {
    if (!element || !element->prev || !element->next) {
        return 1;
    }
    if (element->prev->next != element || element->next->prev != element) {
        return 1;
    }
    if (opt_yield & DELETE_YIELD) {
        sched_yield();
    }
    SortedListElement_t *n = element->next, *p = element->prev;
    n->prev = p;
    p->next = n;
    // element->prev->next = element->next;
    // element->next->prev = element->prev;
    element->prev = NULL;
    element->next = NULL;
    return 0;
}

// Return pointer to element with given key, if existing
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
    if (!list || !key || !list->prev || !list->next) {
        return NULL;
    }
    SortedListElement_t *curr = list->next;
    for (; curr!=list; curr=curr->next) {
        if (opt_yield & LOOKUP_YIELD) {
            sched_yield();
        }
        if (strcmp(curr->key, key) == 0) {
            return curr;
        }
    }
    return NULL;
}

// Return length of SortedList
int SortedList_length(SortedList_t *list) {
    if (!list || !list->prev || !list->next) { // check validity of head ptr
        return -1;
    }
    if (list->prev->next != list || list->next->prev != list) { // check validity of head ptr
        return -1;
    }
    SortedListElement_t *curr = list->next;
    int count = 0;
    for (; curr!=list; curr=curr->next, count++) {
        if (opt_yield & LOOKUP_YIELD) {
            sched_yield();
        }
        if (!curr || !curr->prev || !curr->next) { // check all ptrs
            return -1;
        }
        if (curr->prev->next != curr || curr->next->prev != curr) {
            return -1;
        }
    }
    return count;
}
