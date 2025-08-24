// suite.c 

#define UNIT_TEST_IMPLEMENTATION
#include "test/unit/UnitTest.h"
#include "CircularLinkedList.h"
#include "Helpers.h"

/*============================================================================*/
/* TEST HELPERS                                                      */
/*============================================================================*/

static struct CircularLinkedList* _create_test_list(const int values[], size_t count) {
    return (struct CircularLinkedList*) _n((int*)values, count);
}

static bool _compare_test_lists(const struct CircularLinkedList* l1, const struct CircularLinkedList* l2) {
    return _c((struct Y*)l1, (struct Y*)l2);
}

/*============================================================================*/
/* TEST SUITE A: CircularLinkedList_new                                       */
/*============================================================================*/
TEST_CASE(New, "Creates a non-NULL list structure") {
    UT_disable_leak_check(); // This test doesn't test free().
    struct CircularLinkedList* list = CircularLinkedList_new();
    ASSERT(list != NULL);
    EQUAL_POINTER(list->p_last, NULL);
    EQUAL_INT(list->size, 0);
}
TEST_CASE(New, "Allocates exactly one block and frees none") {
    UT_disable_leak_check(); // This test doesn't test free().
    CircularLinkedList_new();
    ASSERT_ALLOC_COUNT(1);
    ASSERT_FREE_COUNT(0);
}

/*============================================================================*/
/* TEST SUITE B: CircularLinkedList_insert                                    */
/*============================================================================*/
TEST_CASE(Insert, "Inserts into an empty list") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    struct CircularLinkedList* expected = _create_test_list((int[]){10}, 1);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_insert(list, 10);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before + 1, UT_alloc_count);
    EQUAL_INT(frees_before, UT_free_count);
}
TEST_CASE(Insert, "Inserts smaller element at the beginning") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10, 20, 30}, 4);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_insert(list, 5);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before + 1, UT_alloc_count);
    EQUAL_INT(frees_before, UT_free_count);
}
TEST_CASE(Insert, "Inserts larger element at the end") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_insert(list, 40);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before + 1, UT_alloc_count);
    EQUAL_INT(frees_before, UT_free_count);
}
TEST_CASE(Insert, "Inserts an element in the middle") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 40}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 20, 30, 40}, 4);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_insert(list, 30);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before + 1, UT_alloc_count);
    EQUAL_INT(frees_before, UT_free_count);
}
TEST_DEATH_CASE(Insert, "Assertion fails on NULL p_list parameter") {
    CircularLinkedList_insert(NULL, 10);
}

/*============================================================================*/
/* TEST SUITE C: CircularLinkedList_remove                                    */
/*============================================================================*/
TEST_CASE(Remove, "Removes the only element") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    struct CircularLinkedList* expected = _create_test_list(NULL, 0);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_remove(list, 0);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before, UT_alloc_count);
    EQUAL_INT(frees_before + 1, UT_free_count);
}
TEST_CASE(Remove, "Removes the first element") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){10, 15}, 2);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_remove(list, 0);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before, UT_alloc_count);
    EQUAL_INT(frees_before + 1, UT_free_count);
}
TEST_CASE(Remove, "Removes the last element") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 10}, 2);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_remove(list, 2);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before, UT_alloc_count);
    EQUAL_INT(frees_before + 1, UT_free_count);
}
TEST_CASE(Remove, "Removes an element from the middle") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15, 20}, 4);
    struct CircularLinkedList* expected = _create_test_list((int[]){5, 15, 20}, 3);
    int allocs_before = UT_alloc_count, frees_before = UT_free_count;
    CircularLinkedList_remove(list, 1);
    ASSERT(_compare_test_lists(list, expected));
    EQUAL_INT(allocs_before, UT_alloc_count);
    EQUAL_INT(frees_before + 1, UT_free_count);
}
TEST_DEATH_CASE(Remove, "Assertion fails with on out of bounds index") {
    struct CircularLinkedList* list = _create_test_list((int[]){5, 10, 15}, 3);
    CircularLinkedList_remove(list, 3);
}
TEST_DEATH_CASE(Remove, "Assertion fails on NULL p_list parameter") {
    CircularLinkedList_remove(NULL, 0);
}

/*============================================================================*/
/* TEST SUITE D: CircularLinkedList_print                                     */
/*============================================================================*/
TEST_CASE(Print, "Prints an empty list correctly") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "\n");
}
TEST_CASE(Print, "Prints a single element list correctly") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){42}, 1);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "42 \n");
}
TEST_CASE(Print, "Prints a two element list correctly") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20}, 2);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 \n");
}
TEST_CASE(Print, "Prints a multi element list correctly") {
    UT_disable_leak_check();
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 30}, 3);
    ASSERT_STDOUT_EQUAL(CircularLinkedList_print(list), "10 20 30 \n");
}
TEST_DEATH_CASE(Print, "Assertion fails on NULL p_list parameter") {
    CircularLinkedList_print(NULL);
}

/*============================================================================*/
/* TEST SUITE E: CircularLinkedList_free                                      */
/*============================================================================*/
TEST_CASE(Free, "Frees an empty list correctly") {
    struct CircularLinkedList* list = _create_test_list(NULL, 0);
    ASSERT_ALLOC_COUNT(1);
    CircularLinkedList_free(&list);
    ASSERT(list == NULL);
    ASSERT_FREE_COUNT(1);
}
TEST_CASE(Free, "Frees a single element list correctly") {
    struct CircularLinkedList* list = _create_test_list((int[]){100}, 1);
    ASSERT_ALLOC_COUNT(2); // 1 node was tracked + 1 struct
    CircularLinkedList_free(&list);
    ASSERT(list == NULL);
    // Student must free 1 node + 1 struct.
    ASSERT_FREE_COUNT(2); 
}
TEST_CASE(Free, "Frees all memory for a multi element list") {
    // Leak check is ON by default for this suite
    struct CircularLinkedList* list = _create_test_list((int[]){10, 20, 5}, 3);
    ASSERT_ALLOC_COUNT(4); // 3 nodes were tracked + 1 struct
    CircularLinkedList_free(&list);
    ASSERT(list == NULL);
    // Student must free 3 nodes + 1 struct.
    ASSERT_FREE_COUNT(4); 
}
TEST_DEATH_CASE(Free, "Assertion fails on pointer to NULL pointer parameter") {
    struct CircularLinkedList* list = NULL;
    CircularLinkedList_free(&list);
}
TEST_DEATH_CASE(Free, "Assertion fails on NULL p_list parameter") {
    CircularLinkedList_free(NULL);
}

/*============================================================================*/
/* TEST SUITE F: CircularLinkedList_equals                                    */
/*============================================================================*/
TEST_CASE(Equals, "Returns true for two identical non-empty lists") {
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    ASSERT(CircularLinkedList_equals(list1, list2));
}
TEST_CASE(Equals, "Returns false when first list is shorter") {
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20, 30}, 3);
    REFUTE(CircularLinkedList_equals(list1, list2));
}
TEST_CASE(Equals, "Returns false when first list is longer") {
    UT_disable_leak_check();
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20, 30}, 3);
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    REFUTE(CircularLinkedList_equals(list1, list2));
}
TEST_DEATH_CASE(Equals, "Assertion fails when first list is NULL") {
    struct CircularLinkedList* list2 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(NULL, list2);
}
TEST_DEATH_CASE(Equals, "Assertion fails when second list is NULL") {
    struct CircularLinkedList* list1 = _create_test_list((int[]){10, 20}, 2);
    CircularLinkedList_equals(list1, NULL);
}
TEST_DEATH_CASE(Equals, "Assertion fails when both lists are NULL") {
    CircularLinkedList_equals(NULL, NULL);
}
/*============================================================================*/
/* MAIN FUNCTION                                                              */
/*============================================================================*/
int main(int argc, char* argv[]) {
    return UT_RUN_ALL_TESTS();
}