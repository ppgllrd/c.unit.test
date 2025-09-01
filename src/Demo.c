// Pepe Gallardo, Data Structures, University of Malaga

#include "CircularLinkedList.h"
#include <stdio.h>

int runDemo(void) {
  struct CircularLinkedList* p_list1 = CircularLinkedList_new();

  CircularLinkedList_insert(p_list1, 3);
  CircularLinkedList_insert(p_list1, 1);
  CircularLinkedList_insert(p_list1, 5);
  CircularLinkedList_insert(p_list1, 2);
  CircularLinkedList_insert(p_list1, 4);
  CircularLinkedList_insert(p_list1, 6);

  printf("List1 after inserting elements: ");
  CircularLinkedList_print(p_list1); // 1 2 3 4 5 6

  CircularLinkedList_remove(p_list1, 5);
  CircularLinkedList_remove(p_list1, 1);
  CircularLinkedList_remove(p_list1, 0);

  printf("List1 after removing elements: ");
  CircularLinkedList_print(p_list1); // 3 4 5

  struct CircularLinkedList* p_list2 = CircularLinkedList_new();

  CircularLinkedList_insert(p_list2, 5);
  CircularLinkedList_insert(p_list2, 4);
  CircularLinkedList_insert(p_list2, 3);

  printf("List2 after inserting elements: ");
  CircularLinkedList_print(p_list2); // 3 4 5

  if(CircularLinkedList_equals(p_list1, p_list2)) {
    printf("Lists are equal\n");
  } else {
    printf("Lists are not equal\n");
  }

  CircularLinkedList_free(&p_list1);
  CircularLinkedList_free(&p_list2);

  printf("Lists have been freed\n");
  
  return 0;
}