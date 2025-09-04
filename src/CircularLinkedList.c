///////////////////////////////////////////////////////////////////////////////
// Student's name: [Your Name]
// Identity number:  [Your DNI or Passport Number] 
///////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "CircularLinkedList.h"
#include "test/unit/UnitTest.h"

//// BEGIN (A)
struct CircularLinkedList* CircularLinkedList_new() {
  // Allocate memory for the list
  struct CircularLinkedList* p_list = malloc(sizeof(struct CircularLinkedList));
  assert(p_list != NULL && "Memory allocation failed");

  // Initialize the list
  p_list->p_last = NULL;
  p_list->size = 0;
  return p_list;
}
//// END (A)


//// BEGIN (B)
void CircularLinkedList_insert(struct CircularLinkedList* p_list, int element) {
  assert(p_list != NULL && "List is NULL");

  // Allocate memory for the new node and initialize element
  struct Node* p_node = malloc(sizeof(struct Node));
  assert(p_node != NULL && "Memory allocation failed");
  p_node->element = element;

  if (p_list->size == 0) {
   // The list is empty
    p_list->p_last = p_node;
    p_node->p_next = p_node;
  } else {
    // The list is not empty
    struct Node* p_previous = p_list->p_last;    // Last node
    struct Node* p_current = p_previous->p_next; // First node
    
    // Find the correct position to insert the new element
    size_t i = 0;
    while (i < p_list->size && p_current->element < element) {
      p_previous = p_current;
      p_current = p_current->p_next;
      i++;
    }

    // Insert the new element between p_previous and p_current
    p_previous->p_next = p_node;
    p_node->p_next = p_current;

    // Update the last node if necessary
    if (element > p_list->p_last->element) {
      p_list->p_last = p_node;
    }
  }
  // Update the size of the list
  p_list->size++;  
}
//// END (B)


//// BEGIN (C)
void CircularLinkedList_remove(struct CircularLinkedList* p_list, size_t index) {
  assert(p_list != NULL && "List is NULL"); 
  assert(index < p_list->size && "Index out of bounds");

  struct Node* p_previous = p_list->p_last;     // Last node
  struct Node* p_toDelete = p_previous->p_next; // First node

  // Find the node to delete 
  for (size_t i = 0; i < index; i++) {
    p_previous = p_toDelete;
    p_toDelete = p_toDelete->p_next;
  }

  // Remove the node from the list 
  p_previous->p_next = p_toDelete->p_next;

  // Update the last node if necessary 
  if (p_toDelete == p_list->p_last) {
    p_list->p_last = p_list->size == 1 ? NULL : p_previous;
  }

  // Free the memory allocated for the node
  free(p_toDelete);

  // Update the size of the list
  p_list->size--;
}
//// END (C)


//// BEGIN (D)
void CircularLinkedList_print(const struct CircularLinkedList* p_list) {
  assert(p_list != NULL && "List is NULL");

  if (p_list->size != 0) {
    struct Node* p_first = p_list->p_last->p_next;
    struct Node* p_current = p_first;
    
    for (size_t i = 0; i < p_list->size; i++) {
        printf("%d ", p_current->element);
        p_current = p_current->p_next;
    }
  }
   printf("\n");
}
//// END (D)


//// BEGIN (E)
void CircularLinkedList_free(struct CircularLinkedList** p_p_list) {
  assert(p_p_list != NULL && "Pointer is NULL");

  struct CircularLinkedList* p_list = *p_p_list;
  assert(p_list != NULL && "List is NULL"); 

  // Free all the nodes in the list 
  struct Node* p_current = p_list->p_last;
  for (size_t i = 0; i < p_list->size; i++) {
    struct Node* p_toDelete = p_current;
    p_current = p_current->p_next;
    free(p_toDelete);
  }

  // Free the list structure 
  free(p_list);

   // Set the pointer to the list to NULL
  *p_p_list = NULL;
}
//// END (E)


//// BEGIN (F)
bool CircularLinkedList_equals(struct CircularLinkedList* p_list1, struct CircularLinkedList* p_list2) {
  assert(p_list1 != NULL && "List 1 is NULL");
  assert(p_list2 != NULL && "List 2 is NULL"); 
  
  if (p_list1->size != p_list2->size) {
    return false;
  }

  if (p_list1->size == 0) {
    return true;
  }

  struct Node* p_current1 = p_list1->p_last->p_next;
  struct Node* p_current2 = p_list2->p_last->p_next;

  for (size_t i = 0; i < p_list1->size; i++) {
    if (p_current1->element != p_current2->element) {
      return false;
    }
    p_current1 = p_current1->p_next;
    p_current2 = p_current2->p_next;
  }

  return true;
}
//// END (F)
