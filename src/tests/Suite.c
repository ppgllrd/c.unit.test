/*============================================================================*/
/* Test Suites for CircularLinkedList                                         */
/* Pepe Gallardo, 2025                                                        */
/*============================================================================*/

#include "CircularLinkedList.h"
#include "Helpers.h"

#define UNIT_TEST_DECLARATION
#define UNIT_TEST_IMPLEMENTATION
#include "test/unit/UnitTest.h"

/*============================================================================*/
/* TEST HELPERS                                                               */
/*============================================================================*/

static struct CircularLinkedList* _create_test_list(const int values[], size_t count) {
    return (struct CircularLinkedList*) _n((int*)values, count);
}

static bool _equalLists(const struct CircularLinkedList* l1, const struct CircularLinkedList* l2) {
    return _c((struct Y*)l1, (struct Y*)l2);
}


static void __p(char* buf, size_t size, struct CircularLinkedList* list) {
    _p(buf, size, (struct Y*)list);
}

#define EQUAL_CIRCULAR_LINKED_LIST(expected, actual) EQUAL_BY(expected, actual, _equalLists, __p)

/*============================================================================*/
/* TEST SUITE A: CircularLinkedList_new                                       */
/*============================================================================*/
TEST_CASE(CircularLinkedList_new, "Creates a non-NULL list structure") {
    // result is non NULL, and p_last is NULL and size is 0
    UT_disable_leak_check();
    struct CircularLinkedList* list = CircularLinkedList_new();
    REFUTE_NULL(list);
    ASSERT_NULL(list->p_last);
    EQUAL_INT(0, list->size);
}
TEST_CASE(CircularLinkedList_new, "Allocates exactly one block and frees none") {
    // just one allocation, no frees and exactly a struct CircularLinkedList block was allocated
    UT_disable_leak_check();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_new();
    }, 1, 0, sizeof(struct CircularLinkedList), 0);
}

/*============================================================================*/
/* TEST SUITE B: CircularLinkedList_insert                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_insert, "Assertion fails on NULL p_list parameter") {
    // attempt to insert into a NULL list should trigger an assertion failure
    CircularLinkedList_insert(NULL, 10);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_insert, "Assertion fails on NULL p_list parameter with \"List is NULL\" message", "List is NULL") {
    // attempt to insert into a NULL list should trigger an assertion failure with the correct message
    CircularLinkedList_insert(NULL, 10);
}
TEST_CASE(CircularLinkedList_insert, "Inserts into an empty list") {
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    struct CircularLinkedList* expected = _create_test_list((int[]){10}, 1);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 10);
    }, 1, 0, sizeof(struct Node), 0);
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_insert, "Inserts smaller element at the beginning") {
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10, 20, 30}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 5);
    }, 1, 0, sizeof(struct Node), 0);
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_insert, "Inserts larger element at the end") {
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 40);
    }, 1, 0, sizeof(struct Node), 0);
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_insert, "Inserts an element in the middle") {
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 40}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 30);
    }, 1, 0, sizeof(struct Node), 0);
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}

/*============================================================================*/
/* TEST SUITE C: CircularLinkedList_remove                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_remove, "Assertion fails with on out of bounds index") {
    // attempt to remove an element at an out-of-bounds index should trigger an assertion failure
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    UT_mark_memory_as_baseline();
    CircularLinkedList_remove(list, 3);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_remove, "Assertion fails with on out of bounds index with \"Index out of bounds\" message", "Index out of bounds") {
    // attempt to remove an element at an out-of-bounds index should trigger an assertion failure with the correct message
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    UT_mark_memory_as_baseline();
    CircularLinkedList_remove(list, 3);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_remove, "Assertion fails on NULL p_list parameter") {
    // attempt to remove from a NULL list should trigger an assertion failure
    CircularLinkedList_remove(NULL, 0);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_remove, "Assertion fails on NULL p_list parameter with \"List is NULL\" message", "List is NULL") {
    // attempt to remove from a NULL list should trigger an assertion failure with the correct message
    CircularLinkedList_remove(NULL, 0);
}
TEST_CASE(CircularLinkedList_remove, "Removes the only element") {
    // check that one node is freed and the list becomes empty
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    struct CircularLinkedList* expected = _create_test_list(NULL, 0);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 0);
    }, 0, 1, 0, sizeof(struct Node));
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_remove, "Removes the first element") {
    // check that one node is freed and the first element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 15}, 2);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 0);
    }, 0, 1, 0, sizeof(struct Node));
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_remove, "Removes the last element") {
    // check that one node is freed and the last element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10}, 2);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 2);
    }, 0, 1, 0, sizeof(struct Node));
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}
TEST_CASE(CircularLinkedList_remove, "Removes an element from the middle") {
    // check that one node is freed and the middle element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15, 20}, 4);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 15, 20}, 3);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 1);
    }, 0, 1, 0, sizeof(struct Node));
    EQUAL_CIRCULAR_LINKED_LIST(expected, list);
}

/*============================================================================*/
/* TEST SUITE D: CircularLinkedList_print                                     */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_print, "Assertion fails on NULL p_list parameter") {
    // attempt to print a NULL list should trigger an assertion failure
    CircularLinkedList_print(NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_print, "Assertion fails on NULL p_list parameter with \"List is NULL\" message", "List is null", .min_similarity = 0.85f) {
    // attempt to print a NULL list should trigger an assertion failure
    CircularLinkedList_print(NULL);
}
TEST_CASE(CircularLinkedList_print, "Prints an empty list correctly") {
    // check that printing an empty list outputs just a newline
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "\n");
}
TEST_CASE(CircularLinkedList_print, "Prints a single element list correctly") {
    // check that a single-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "42 \n");
}
TEST_CASE(CircularLinkedList_print, "Prints a two element list correctly") {
    // check that a two-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20}, 2);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 \n");
}
TEST_CASE(CircularLinkedList_print, "Prints a multi element list correctly") {
    // check that a multi-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 30 \n");
}

/*============================================================================*/
/* TEST SUITE E: CircularLinkedList_free                                      */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_free, "Assertion fails on pointer to NULL pointer parameter") {
    // attempt to free a list via a pointer to a NULL pointer should trigger an assertion failure
    struct CircularLinkedList* list = NULL;
    CircularLinkedList_free(&list);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_free, "Assertion fails on pointer to NULL pointer parameter with \"List is NULL\" message", "List is NULL") {
    // attempt to free a list via a pointer to a NULL pointer should trigger an assertion failure with the correct message
    struct CircularLinkedList* list = NULL;
    CircularLinkedList_free(&list);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_free, "Assertion fails on NULL p_list parameter") {
    // attempt to free a list via a NULL double pointer should trigger an assertion failure
    CircularLinkedList_free(NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_free, "Assertion fails on NULL p_list parameter with \"Pointer is NULL\" message", "Pointer is NULL") {
    // attempt to free a list via a NULL double pointer should trigger an assertion failure with the correct message
    CircularLinkedList_free(NULL);
}
TEST_CASE(CircularLinkedList_free, "Frees an empty list correctly") {
    // check that one block (the list struct) is freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 1, 0, sizeof(struct CircularLinkedList));
    ASSERT_NULL(list);
}
TEST_CASE(CircularLinkedList_free, "Frees a single element list correctly") {
    // check that two blocks (list struct and one node) are freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list((int[]){100}, 1);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 2, 0, sizeof(struct CircularLinkedList) + sizeof(struct Node));
    ASSERT_NULL(list);
}
TEST_CASE(CircularLinkedList_free, "Frees all memory for a multi element list") {
    // check that all blocks (list struct and all nodes) are freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 5}, 3);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 4, 0, sizeof(struct CircularLinkedList) + (3 * sizeof(struct Node)));
    ASSERT_NULL(list);
}

/*============================================================================*/
/* TEST SUITE F: CircularLinkedList_equals                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion fails when first list is NULL") {
    // attempt to compare with a NULL list as the first argument should trigger an assertion failure
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(NULL, list2);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_equals, "Assertion fails when first list is NULL with \"List 1 is NULL\" message", "List 1 is NULL") {
    // attempt to compare with a NULL list as the first argument should trigger an assertion failure with the correct message
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(NULL, list2);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion fails when second list is NULL") {
    // attempt to compare with a NULL list as the second argument should trigger an assertion failure
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(list1, NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_equals, "Assertion fails when second list is NULL with \"List 2 is NULL\" message", "List 2 is NULL") {
    // attempt to compare with a NULL list as the second argument should trigger an assertion failure with the correct message
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(list1, NULL);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion fails when both lists are NULL") {
    // attempt to compare two NULL lists should trigger an assertion failure
    CircularLinkedList_equals(NULL, NULL);
}
TEST_CASE(CircularLinkedList_equals, "Returns true for two identical non-empty lists") {
    // check that two identical lists are considered equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    ASSERT(CircularLinkedList_equals(list1, list2));
}
TEST_CASE(CircularLinkedList_equals, "Returns false when first list is shorter") {
    // check that lists of different sizes are not equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    REFUTE(CircularLinkedList_equals(list1, list2));
}
TEST_CASE(CircularLinkedList_equals, "Returns false when first list is longer") {
    // check that lists of different sizes are not equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    REFUTE(CircularLinkedList_equals(list1, list2));
}

/*============================================================================*/
/* MAIN FUNCTION                                                              */
/*============================================================================*/
int runAllTests(int argc, char* argv[]) {
    return UT_RUN_ALL_TESTS();
}