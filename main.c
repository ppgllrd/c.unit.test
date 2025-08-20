// Define this before including the header to enable memory tracking
#define MEMORY_TRACKING_ENABLED

#include "robust_test.h"

#ifdef _WIN32
    #include <windows.h> // For EXCEPTION_ACCESS_VIOLATION
#else
    #include <signal.h>  // For SIGSEGV, SIGABRT
#endif

/*==============================================================*/
/* Code and Helpers to be Tested                                */
/*==============================================================*/

// --- For Arithmetic Suite ---
int add(int a, int b) {
    return a + b;
}

// --- For Memory Suite ---
char* create_string(const char* initial) {
    if (!initial) return NULL;
    char* str = (char*)malloc(strlen(initial) + 1);
    strcpy(str, initial);
    return str;
}
void function_with_leak() {
    char* leaky_buffer = (char*)malloc(100);
    strcpy(leaky_buffer, "This memory is never freed");
}

// --- For System Failures Suite ---
void cause_crash() { int* p = NULL; *p = 1; }
void cause_assert() {
    fprintf(stderr, "Assertion failed: value > 0\n");
#ifdef _WIN32
    exit(3);
#else
    abort();
#endif
}

// --- For Custom Types Suite ---
typedef struct { int x; int y; } Point;
int points_are_equal(Point p1, Point p2) { return p1.x == p2.x && p1.y == p2.y; }
void print_point_to_buffer(char* buffer, size_t size, Point p) { snprintf(buffer, size, "Point(%d, %d)", p.x, p.y); }

// --- For Property Suite ---
int get_number() { return 7; }
int is_even(int n) { return n % 2 == 0; }
int is_positive(int n) { return n > 0; }

// --- For Timeout Suite ---
int timeout() { for(;;) ; }

/*==============================================================*/
/* Test Cases                                                   */
/*==============================================================*/

TEST_CASE(Arithmetic, "Correctly adds two positive numbers") {
    EQUAL_INT(5, add(2, 3));
    ASSERT(add(1,1) == 2);
    REFUTE(add(1,1) == 3);
}

TEST_CASE(Memory, "Correctly allocates and frees memory") {
    char* my_string = create_string("hello");
    ASSERT_ALLOC_COUNT(1);
    ASSERT_FREE_COUNT(0);
    
    free(my_string);
    ASSERT_FREE_COUNT(1);
}

TEST_CASE(Memory, "Detects a memory leak") {
    function_with_leak();
    // This test will pass its own code, but the framework's automatic leak check
    // will cause the child process to exit with an error, failing the test.
}

TEST_CASE(Memory, "Fails when freeing a NULL pointer") {
    free(NULL); // The framework's `free` wrapper will catch and fail this.
}

#ifdef _WIN32
    TEST_DEATH_CASE(SystemFailures, "A null pointer dereference causes an access violation", .expected_exit_code = EXCEPTION_ACCESS_VIOLATION) {
        cause_crash();
    }
    TEST_DEATH_CASE(SystemFailures, "An assert fires with the correct exit code and message", .expected_exit_code = 3, .expected_msg = "Assertion failed") {
        cause_assert();
    }
#else
    TEST_DEATH_CASE(SystemFailures, "A null pointer dereference causes a segmentation fault", .expected_signal = SIGSEGV) {
        cause_crash();
    }
    TEST_DEATH_CASE(SystemFailures, "An assert fires with the correct signal and message", .expected_signal = SIGABRT, .expected_msg = "Assertion failed") {
        cause_assert();
    }
#endif

TEST_CASE(CustomTypes, "Points with same coordinates should be equal") {
    Point a = {10, 20};
    Point b = {10, 20};
    EQUAL_BY(a, b, points_are_equal, print_point_to_buffer);
}

TEST_CASE(CustomTypes, "A test with different points that will fail") {
    Point a = {10, 20};
    Point c = {15, 25};
    EQUAL_BY(a, c, points_are_equal, print_point_to_buffer);
}

TEST_CASE(PropertyTests, "An integer should be positive") {
    int num = get_number();
    PROPERTY_INT(num, is_positive, "Value should be greater than zero");
}

TEST_CASE(PropertyTests, "An integer should be even (will fail)") {
    int num = get_number(); // num is 7
    PROPERTY_INT(num, is_even, "Value should be an even number");
}

TEST_CASE(TimeoutTests, "Non-terminating code (will fail)") {
    timeout();
}

/*==============================================================*/
/* Main Function (Remains trivial)                              */
/*==============================================================*/

int main(int argc, char* argv[]) {
    return RUN_ALL_TESTS();
}