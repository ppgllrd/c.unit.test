// Pepe Gallardo, Data Structures, University of Malaga

#ifndef CIRCULAR_LINKED_LIST_H
#define CIRCULAR_LINKED_LIST_H

#include <stddef.h>
#include <stdbool.h>

struct Node {
  int element;         // element in the node
  struct Node* p_next; // pointer to the next node
};

struct CircularLinkedList {
  struct Node* p_last; // pointer to the last node
  size_t size;         // number of elements in the list
};

struct CircularLinkedList* CircularLinkedList_new();
void CircularLinkedList_insert(struct CircularLinkedList* p_list, int element);
void CircularLinkedList_remove(struct CircularLinkedList* p_list, size_t index);
void CircularLinkedList_print(const struct CircularLinkedList* p_list);
void CircularLinkedList_free(struct CircularLinkedList** p_p_list);
bool CircularLinkedList_equals(const struct CircularLinkedList* p_list1, const struct CircularLinkedList* p_list2);

#endif