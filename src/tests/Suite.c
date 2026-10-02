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

static bool _validateList(struct CircularLinkedList* list, char* buf, size_t size) {
    return _v((struct Y*)list, buf, size);
}

#define EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, actual, summary, hint)          \
    do {                                                                         \
        struct CircularLinkedList* _ut_expected_list = (expected);               \
        struct CircularLinkedList* _ut_actual_list = (actual);                   \
        if (!_equalLists(_ut_expected_list, _ut_actual_list)) {                  \
            char _ut_expected_text[1024] = {0};                                  \
            char _ut_actual_text[1024] = {0};                                    \
            __p(_ut_expected_text, sizeof(_ut_expected_text), _ut_expected_list);\
            __p(_ut_actual_text, sizeof(_ut_actual_text), _ut_actual_list);       \
            _UT_record_failure_ex(__FILE__, __LINE__, (summary), (hint),         \
                                  "expected list", _ut_expected_text,           \
                                  "observed list", _ut_actual_text);            \
        }                                                                        \
    } while (0)

#define VALIDATE_CIRCULAR_LINKED_LIST(list)                                      \
    do {                                                                         \
        struct CircularLinkedList* _ut_list = (list);                            \
        char _ut_validation[1024] = {0};                                         \
        if (!_validateList(_ut_list, _ut_validation, sizeof(_ut_validation)))    \
            _UT_record_failure_ex(__FILE__, __LINE__,                            \
                "The circular linked-list structure is invalid",               \
                "Preserve size, p_last, and exactly one circular traversal.",   \
                "required structure", "a valid circular linked list",          \
                "observed structure", _ut_validation);                         \
    } while (0)

#define EQUAL_INT_MSG(expected, actual, summary, hint)                           \
    do {                                                                         \
        int _ut_e = (int)(expected), _ut_a = (int)(actual);                      \
        if (_ut_e != _ut_a) {                                                    \
            char _ut_eb[64], _ut_ab[64];                                         \
            snprintf(_ut_eb, sizeof(_ut_eb), "%d", _ut_e);                    \
            snprintf(_ut_ab, sizeof(_ut_ab), "%d", _ut_a);                    \
            _UT_record_failure_ex(__FILE__, __LINE__, (summary), (hint),         \
                                  "expected value", _ut_eb, #actual, _ut_ab);  \
        }                                                                        \
    } while (0)

#define EQUAL_BOOL_MSG(expected, actual, summary, hint)                          \
    do {                                                                         \
        bool _ut_e = (bool)(expected), _ut_a = (bool)(actual);                   \
        if (_ut_e != _ut_a)                                                      \
            _UT_record_failure_ex(__FILE__, __LINE__, (summary), (hint),         \
                "expected value", _ut_e ? "true" : "false", #actual,          \
                _ut_a ? "true" : "false");                                    \
    } while (0)

/*============================================================================*/
/* TEST SUITE A: CircularLinkedList_new                                       */
/*============================================================================*/
TEST_CASE(CircularLinkedList_new, "Creates a non-NULL list structure") {
    TEST_CONTEXT("Contract: CircularLinkedList_new must return a non-NULL empty list with p_last equal to NULL and size equal to 0. Hint: Allocate one list header and initialize every field before returning it.");
    // result is non NULL, and p_last is NULL and size is 0
    UT_disable_leak_check();
    struct CircularLinkedList* list = CircularLinkedList_new();
    REQUIRE_NOT_NULL_MSG(list,
        "CircularLinkedList_new returned NULL",
        "Allocate and return one CircularLinkedList header before initializing its fields.");
    if (list != NULL) {
        VALIDATE_CIRCULAR_LINKED_LIST(list);
        EQUAL_POINTER_MSG(NULL, list->p_last,
                          "A new circular list is not empty",
                          "Initialize p_last to NULL for an empty list.");
        EQUAL_INT_MSG(0, list->size,
                      "A new circular list has an incorrect size",
                      "Initialize size to 0 before returning the list.");
    }
}
TEST_CASE(CircularLinkedList_new, "Allocates exactly one block and frees none") {
    TEST_CONTEXT("Contract: CircularLinkedList_new must allocate exactly one CircularLinkedList header and no nodes. Hint: Call malloc(sizeof(struct CircularLinkedList)) once and initialize the empty-list fields.");
    // just one allocation, no frees and exactly a struct CircularLinkedList block was allocated
    UT_disable_leak_check();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_new();
    }, 1, 0, sizeof(struct CircularLinkedList), 0);
}

/*============================================================================*/
/* TEST SUITE B: CircularLinkedList_insert                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_insert, "Assertion should fail on NULL p_list parameter") {
    TEST_CONTEXT("Contract: CircularLinkedList_insert must assert when p_list is NULL. Hint: Validate the list pointer before reading its size or links.");
    // attempt to insert into a NULL list should trigger an assertion failure
    CircularLinkedList_insert(NULL, 10);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_insert, "Assertion should fail on NULL p_list parameter with \"List is NULL\" message", "List is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_insert must assert when p_list is NULL. Hint: Validate the list pointer before reading its size or links.");
    // attempt to insert into a NULL list should trigger an assertion failure with the correct message
    CircularLinkedList_insert(NULL, 10);
}
TEST_CASE(CircularLinkedList_insert, "Inserts into an empty list") {
    TEST_CONTEXT("Contract: Inserting into an empty list must create one node, make p_last reference it, make its p_next point to itself, and set size to 1. Hint: Handle the empty case before traversing existing nodes.");
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    struct CircularLinkedList* expected = _create_test_list((int[]){10}, 1);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 10);
    }, 1, 0, sizeof(struct Node), 0);
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after insertion is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}
TEST_CASE(CircularLinkedList_insert, "Inserts smaller element at the beginning") {
    TEST_CONTEXT("Contract: Ordered insertion of a new minimum must place the node before the logical first node while preserving p_last and circularity. Hint: Link the new node between p_last and the former first node.");
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10, 20, 30}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 5);
    }, 1, 0, sizeof(struct Node), 0);
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after insertion is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}
TEST_CASE(CircularLinkedList_insert, "Inserts larger element at the end") {
    TEST_CONTEXT("Contract: Ordered insertion of a new maximum must append the node after the former last node, preserve the link to the first node, and update p_last. Hint: Connect the new node between the former last and first nodes.");
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 40);
    }, 1, 0, sizeof(struct Node), 0);
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after insertion is incorrect",
                                      "Append the new maximum, update p_last, and preserve the circular link to the first node.");
}
TEST_CASE(CircularLinkedList_insert, "Inserts an element in the middle") {
    TEST_CONTEXT("Contract: Ordered insertion must place the new value between its predecessor and successor while preserving sorted order, size, and circularity. Hint: Locate the insertion point before reconnecting links.");
    // check that after insertion, the list is correctly updated and one node was allocated
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 40}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_insert(list, 30);
    }, 1, 0, sizeof(struct Node), 0);
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after insertion is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}

/*============================================================================*/
/* TEST SUITE C: CircularLinkedList_remove                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_remove, "Assertion should fail with on out of bounds index") {
    TEST_CONTEXT("Contract: CircularLinkedList_remove must assert when index is not in [0, size). Hint: Check the upper bound before traversing or changing links.");
    // attempt to remove an element at an out-of-bounds index should trigger an assertion failure
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    UT_mark_memory_as_baseline();
    CircularLinkedList_remove(list, 3);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_remove, "Assertion should fail with on out of bounds index with \"Index out of bounds\" message", "Index out of bounds") {
    TEST_CONTEXT("Contract: CircularLinkedList_remove must assert when index is not in [0, size). Hint: Check the upper bound before traversing or changing links.");
    // attempt to remove an element at an out-of-bounds index should trigger an assertion failure with the correct message
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    UT_mark_memory_as_baseline();
    CircularLinkedList_remove(list, 3);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_remove, "Assertion should fail on NULL p_list parameter") {
    TEST_CONTEXT("Contract: CircularLinkedList_remove must assert when p_list is NULL. Hint: Validate the list pointer before checking size or traversing nodes.");
    // attempt to remove from a NULL list should trigger an assertion failure
    CircularLinkedList_remove(NULL, 0);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_remove, "Assertion should fail on NULL p_list parameter with \"List is NULL\" message", "List is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_remove must assert when p_list is NULL. Hint: Validate the list pointer before checking size or traversing nodes.");
    // attempt to remove from a NULL list should trigger an assertion failure with the correct message
    CircularLinkedList_remove(NULL, 0);
}
TEST_CASE(CircularLinkedList_remove, "Removes the only element") {
    TEST_CONTEXT("Contract: Removing the only node must free it and leave p_last equal to NULL and size equal to 0. Hint: Handle the single-node case separately.");
    // check that one node is freed and the list becomes empty
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    struct CircularLinkedList* expected = _create_test_list(NULL, 0);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 0);
    }, 0, 1, 0, sizeof(struct Node));
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after removal is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}
TEST_CASE(CircularLinkedList_remove, "Removes the first element") {
    TEST_CONTEXT("Contract: Removing index 0 must bypass and free the logical first node while preserving p_last and circularity. Hint: Update p_last->p_next to the former second node.");
    // check that one node is freed and the first element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 15}, 2);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 0);
    }, 0, 1, 0, sizeof(struct Node));
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after removal is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}
TEST_CASE(CircularLinkedList_remove, "Removes the last element") {
    TEST_CONTEXT("Contract: Removing the last node must update p_last to its predecessor, reconnect it to the first node, and decrement size once. Hint: Locate the predecessor before freeing the old last node.");
    // check that one node is freed and the last element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10}, 2);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 2);
    }, 0, 1, 0, sizeof(struct Node));
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after removal is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}
TEST_CASE(CircularLinkedList_remove, "Removes an element from the middle") {
    TEST_CONTEXT("Contract: Removing a middle node must bypass and free exactly that node while preserving order, size, and circularity. Hint: Keep the predecessor and successor before unlinking.");
    // check that one node is freed and the middle element is correctly removed
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15, 20}, 4);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 15, 20}, 3);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_remove(list, 1);
    }, 0, 1, 0, sizeof(struct Node));
    VALIDATE_CIRCULAR_LINKED_LIST(list);
    EQUAL_CIRCULAR_LINKED_LIST_MSG(expected, list,
                                      "CircularLinkedList after removal is incorrect",
                                      "Preserve the expected values, size, p_last position, and circular links.");
}

/*============================================================================*/
/* TEST SUITE D: CircularLinkedList_print                                     */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_print, "Assertion should fail on NULL p_list parameter") {
    TEST_CONTEXT("Contract: CircularLinkedList_print must assert when p_list is NULL. Hint: Validate the list pointer before traversing or printing.");
    // attempt to print a NULL list should trigger an assertion failure
    CircularLinkedList_print(NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_print, "Assertion should fail on NULL p_list parameter with \"List is NULL\" message", "List is null", .min_similarity = 0.85f) {
    TEST_CONTEXT("Contract: CircularLinkedList_print must assert when p_list is NULL. Hint: Validate the list pointer before traversing or printing.");
    // attempt to print a NULL list should trigger an assertion failure
    CircularLinkedList_print(NULL);
}
TEST_CASE(CircularLinkedList_print, "Prints an empty list correctly") {
    TEST_CONTEXT("Contract: CircularLinkedList_print must produce the exact logical first-to-last representation without modifying the list. Hint: Traverse once around the circle and avoid extra separators or newlines.");
    // check that printing an empty list outputs just a newline
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "\n");
}
TEST_CASE(CircularLinkedList_print, "Prints a single element list correctly") {
    TEST_CONTEXT("Contract: CircularLinkedList_print must produce the exact logical first-to-last representation without modifying the list. Hint: Traverse once around the circle and avoid extra separators or newlines.");
    // check that a single-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "42 \n");
}
TEST_CASE(CircularLinkedList_print, "Prints a two element list correctly") {
    TEST_CONTEXT("Contract: CircularLinkedList_print must produce the exact logical first-to-last representation without modifying the list. Hint: Traverse once around the circle and avoid extra separators or newlines.");
    // check that a two-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20}, 2);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 \n");
}
TEST_CASE(CircularLinkedList_print, "Prints a multi element list correctly") {
    TEST_CONTEXT("Contract: CircularLinkedList_print must produce the exact logical first-to-last representation without modifying the list. Hint: Traverse once around the circle and avoid extra separators or newlines.");
    // check that a multi-element list prints correctly
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 30 \n");
}

/*============================================================================*/
/* TEST SUITE E: CircularLinkedList_free                                      */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_free, "Assertion should fail on pointer to NULL pointer parameter") {
    TEST_CONTEXT("Contract: Freeing a non-empty list must release every node exactly once, release the list header, and set the caller pointer to NULL. Hint: Traverse the circular chain safely before freeing the header.");
    // attempt to free a list via a pointer to a NULL pointer should trigger an assertion failure
    struct CircularLinkedList* list = NULL;
    CircularLinkedList_free(&list);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_free, "Assertion should fail on pointer to NULL pointer parameter with \"List is NULL\" message", "List is NULL") {
    TEST_CONTEXT("Contract: Freeing a non-empty list must release every node exactly once, release the list header, and set the caller pointer to NULL. Hint: Traverse the circular chain safely before freeing the header.");
    // attempt to free a list via a pointer to a NULL pointer should trigger an assertion failure with the correct message
    struct CircularLinkedList* list = NULL;
    CircularLinkedList_free(&list);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_free, "Assertion should fail on NULL p_list parameter") {
    TEST_CONTEXT("Contract: Freeing a non-empty list must release every node exactly once, release the list header, and set the caller pointer to NULL. Hint: Traverse the circular chain safely before freeing the header.");
    // attempt to free a list via a NULL double pointer should trigger an assertion failure
    CircularLinkedList_free(NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_free, "Assertion should fail on NULL p_list parameter with \"Pointer is NULL\" message", "Pointer is NULL") {
    TEST_CONTEXT("Contract: Freeing a non-empty list must release every node exactly once, release the list header, and set the caller pointer to NULL. Hint: Traverse the circular chain safely before freeing the header.");
    // attempt to free a list via a NULL double pointer should trigger an assertion failure with the correct message
    CircularLinkedList_free(NULL);
}
TEST_CASE(CircularLinkedList_free, "Frees an empty list correctly") {
    TEST_CONTEXT("Contract: Freeing an empty list must release exactly the list header and set the caller pointer to NULL. Hint: Do not attempt to free a node when p_last is NULL.");
    // check that one block (the list struct) is freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 1, 0, sizeof(struct CircularLinkedList));
    EQUAL_POINTER_MSG(NULL, list,
                      "CircularLinkedList_free did not set the caller pointer to NULL",
                      "After freeing all owned blocks, assign NULL through p_p_list.");
}
TEST_CASE(CircularLinkedList_free, "Frees a single element list correctly") {
    TEST_CONTEXT("Contract: Freeing a one-node list must release the node and list header exactly once, then set the caller pointer to NULL. Hint: Preserve ownership boundaries and nullify *p_p_list last.");
    // check that two blocks (list struct and one node) are freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list((int[]){100}, 1);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 2, 0, sizeof(struct CircularLinkedList) + sizeof(struct Node));
    EQUAL_POINTER_MSG(NULL, list,
                      "CircularLinkedList_free did not set the caller pointer to NULL",
                      "After freeing all owned blocks, assign NULL through p_p_list.");
}
TEST_CASE(CircularLinkedList_free, "Frees all memory for a multi element list") {
    TEST_CONTEXT("Contract: Freeing a non-empty list must release every node exactly once, release the list header, and set the caller pointer to NULL. Hint: Traverse the circular chain safely before freeing the header.");
    // check that all blocks (list struct and all nodes) are freed and the list pointer is set to NULL
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 5}, 3);
    UT_mark_memory_as_baseline();
    ASSERT_AND_MARK_MEMORY_CHANGES_BYTES({
        CircularLinkedList_free(&list);
    }, 0, 4, 0, sizeof(struct CircularLinkedList) + (3 * sizeof(struct Node)));
    EQUAL_POINTER_MSG(NULL, list,
                      "CircularLinkedList_free did not set the caller pointer to NULL",
                      "After freeing all owned blocks, assign NULL through p_p_list.");
}

/*============================================================================*/
/* TEST SUITE F: CircularLinkedList_equals                                    */
/*============================================================================*/
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion should fail when first list is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must assert when the first list is NULL. Hint: Validate list1 before inspecting size or nodes.");
    // attempt to compare with a NULL list as the first argument should trigger an assertion failure
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(NULL, list2);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_equals, "Assertion should fail when first list is NULL with \"List 1 is NULL\" message", "List 1 is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must assert when the first list is NULL. Hint: Validate list1 before inspecting size or nodes.");
    // attempt to compare with a NULL list as the first argument should trigger an assertion failure with the correct message
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(NULL, list2);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion should fail when second list is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must assert when the second list is NULL. Hint: Validate list2 before inspecting size or nodes.");
    // attempt to compare with a NULL list as the second argument should trigger an assertion failure
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(list1, NULL);
}
TEST_ASSERTION_FAILURE_WITH_SIMILAR_MESSAGE(CircularLinkedList_equals, "Assertion should fail when second list is NULL with \"List 2 is NULL\" message", "List 2 is NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must assert when the second list is NULL. Hint: Validate list2 before inspecting size or nodes.");
    // attempt to compare with a NULL list as the second argument should trigger an assertion failure with the correct message
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(list1, NULL);
}
TEST_ASSERTION_FAILURE(CircularLinkedList_equals, "Assertion should fail when both lists are NULL") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must reject two NULL list arguments by assertion. Hint: Validate both public inputs before comparing them.");
    // attempt to compare two NULL lists should trigger an assertion failure
    CircularLinkedList_equals(NULL, NULL);
}
TEST_CASE(CircularLinkedList_equals, "Returns true for two identical non-empty lists") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must compare logical size and values in first-to-last order without modifying either circular list. Hint: Stop after one complete traversal.");
    // check that two identical lists are considered equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    EQUAL_BOOL_MSG(true, CircularLinkedList_equals(list1, list2),
                   "CircularLinkedList_equals returned false for identical lists",
                   "Compare size and every logical value exactly once around the circle.");
}
TEST_CASE(CircularLinkedList_equals, "Returns false when first list is shorter") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must compare logical size and values in first-to-last order without modifying either circular list. Hint: Stop after one complete traversal.");
    // check that lists of different sizes are not equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    EQUAL_BOOL_MSG(false, CircularLinkedList_equals(list1, list2),
                   "CircularLinkedList_equals returned true for different lists",
                   "Return false when sizes or any corresponding logical values differ.");
}
TEST_CASE(CircularLinkedList_equals, "Returns false when first list is longer") {
    TEST_CONTEXT("Contract: CircularLinkedList_equals must compare logical size and values in first-to-last order without modifying either circular list. Hint: Stop after one complete traversal.");
    // check that lists of different sizes are not equal
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    EQUAL_BOOL_MSG(false, CircularLinkedList_equals(list1, list2),
                   "CircularLinkedList_equals returned true for different lists",
                   "Return false when sizes or any corresponding logical values differ.");
}

/*============================================================================*/
/* MAIN FUNCTION                                                              */
/*============================================================================*/
int runAllTests(int argc, char* argv[]) {
    return UT_RUN_ALL_TESTS();
}