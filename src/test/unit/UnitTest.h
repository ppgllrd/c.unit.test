#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h> 

/*============================================================================*/
/* SECTION 1: CORE DEFINITIONS & PLATFORM ABSTRACTION                         */
/*============================================================================*/

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define UT_IS_TTY _isatty(_fileno(stdout))
#else // POSIX
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <time.h>
    #include <errno.h>
    #define UT_IS_TTY isatty(STDOUT_FILENO)
#endif

#ifndef UT_TEST_TIMEOUT_SECONDS
    #define UT_TEST_TIMEOUT_SECONDS 2
#endif

#define UT_UT_SUITE_RESULTS_BUFFER_SIZE 4096
#define UT_SUITE_RESULTS_BUFFER_SIZE 1024
#define UT_MAX_SUITES 128

// Forward declarations
typedef void (*_UT_TestFunction)(void);
typedef struct _UT_TestInfo _UT_TestInfo;
typedef struct { const char* expected_msg; int expected_signal; int expected_exit_code; float min_similarity; } _UT_DeathExpect;

struct _UT_TestInfo {
    const char* suite_name;
    const char* test_name;
    _UT_TestFunction func;
    const _UT_DeathExpect* death_expect;
    _UT_TestInfo* next;
};

/*============================================================================*/
/* SECTION 2: GLOBAL STATE AND COLOR MANAGEMENT                               */
/*============================================================================*/

static _UT_TestInfo* _UT_registry_head = NULL;
static _UT_TestInfo* g_UT_registry_tail = NULL;
static int _UT_use_color = 1;
static int _UT_is_ci_mode = 0;

typedef struct { int passed; int total; char details[UT_SUITE_RESULTS_BUFFER_SIZE]; } _UT_SuiteResult;
static _UT_SuiteResult _UT_all_suite_results[UT_MAX_SUITES];
static int _UT_suite_count = 0;

#define KNRM (_UT_use_color ? "\x1B[0m" : "")
#define KRED (_UT_use_color ? "\x1B[31m" : "")
#define KGRN (_UT_use_color ? "\x1B[32m" : "")
#define KYEL (_UT_use_color ? "\x1B[33m" : "")
#define KBLU (_UT_use_color ? "\x1B[34m" : "")

static void _UT_init_colors(void) {
    const char* no_color = getenv("NO_COLOR");
    _UT_use_color = UT_IS_TTY && !no_color;
#ifdef _WIN32
    if (_UT_use_color) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
#endif
}

/*============================================================================*/
/* SECTION 3: ADVANCED MEMORY TRACKING                                        */
/*============================================================================*/
#define UT_MEMORY_TRACKING_ENABLED

#ifdef UT_MEMORY_TRACKING_ENABLED
// Node for the linked list that tracks memory allocations.
typedef struct _UT_MemInfo {
    void* address;
    size_t size;
    const char* file;
    int line;
    struct _UT_MemInfo* next;
} _UT_MemInfo;

// Shared global state for memory tracking.
// Declared as 'extern' so all modules see the same variables.
// Defined once in the file that sets UNIT_TEST_IMPLEMENTATION.
extern _UT_MemInfo* _UT_mem_head;
extern int UT_alloc_count, UT_free_count;
extern int _UT_mem_tracking_enabled;
extern int _UT_mem_tracking_is_active;
extern int _UT_leak_UT_check_enabled;

#ifdef UNIT_TEST_IMPLEMENTATION
// Actual definitions of the global variables. This code will only be
// compiled into the single .c file that defines UNIT_TEST_IMPLEMENTATION.
_UT_MemInfo* _UT_mem_head = NULL;
int UT_alloc_count = 0, UT_free_count = 0;
int _UT_mem_tracking_enabled = 0;
int _UT_mem_tracking_is_active = 1;
int _UT_leak_UT_check_enabled = 1;
#endif // UNIT_TEST_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4090 4091)
#endif

// Isolate macro redefinitions to safely access original libc functions.
#pragma push_macro("malloc")
#pragma push_macro("calloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef calloc
#undef realloc
#undef free

// Provide explicit declarations for the standard library memory functions.
#include <stdlib.h>

#pragma pop_macro("free")
#pragma pop_macro("realloc")
#pragma pop_macro("calloc")
#pragma pop_macro("malloc")

/**
 * @brief Dynamically enables memory tracking at runtime.
 *        Does nothing if UT_MEMORY_TRACKING_ENABLED was not defined at compile time.
 */
static void UT_enable_memory_tracking(void) { _UT_mem_tracking_is_active = 1; }

/**
 * @brief Dynamically disables memory tracking at runtime.
 *        Allocations and frees will not be tracked until it is re-enabled.
 */
static void UT_disable_memory_tracking(void) { _UT_mem_tracking_is_active = 0; }

/**
 * @brief Disables the final memory leak check for the current test.
 *        Useful for tests that intentionally don't free setup memory.
 */
static void UT_disable_leak_check(void) { _UT_leak_UT_check_enabled = 0; }

// Resets the memory tracking state for a new test run.
static void _UT_init_memory_tracking(void) {
    while (_UT_mem_head != NULL) {
        _UT_MemInfo* temp = _UT_mem_head;
        _UT_mem_head = _UT_mem_head->next;
        free(temp); // Use original free to release tracking nodes.
    }
    UT_alloc_count = 0;
    UT_free_count = 0;
    _UT_mem_tracking_enabled = 1;
    _UT_mem_tracking_is_active = 1; // Ensure tracking is active by default.
    _UT_leak_UT_check_enabled = 1;     // Ensure leak checking is active by default for each test.
}

// Checks for memory leaks at the end of a test and exits if any are found.
static void _UT_check_for_leaks(void) {
    _UT_mem_tracking_enabled = 0; // Disable tracking during check.
    if (_UT_mem_head != NULL) {
        fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM);
        fprintf(stderr, "   Reason: Memory leak detected.\n");
        _UT_MemInfo* current = _UT_mem_head;
        while (current != NULL) {
            fprintf(stderr, "      - %zu bytes allocated at %s:%d\n", current->size, current->file, current->line);
            current = current->next;
        }
        exit(1);
    }
}

// Wrapper for malloc that adds allocation details to the tracking list.
static void* _UT_malloc(size_t size, const char* file, int line) {
    // Bypass tracking if the feature is disabled globally or dynamically turned off.
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active) {
        return malloc(size);
    }
    void* ptr = malloc(size);
    if (ptr) {
        // Temporarily disable tracking to prevent self-tracking the _UT_MemInfo allocation.
        _UT_mem_tracking_is_active = 0;
        _UT_MemInfo* info = (_UT_MemInfo*)malloc(sizeof(_UT_MemInfo));
        _UT_mem_tracking_is_active = 1;
        if (info) {
            info->address = ptr; info->size = size; info->file = file; info->line = line;
            info->next = _UT_mem_head; _UT_mem_head = info; UT_alloc_count++;
        }
    }
    return ptr;
}

// Wrapper for calloc that adds allocation details to the tracking list.
static void* _UT_calloc(size_t num, size_t size, const char* file, int line) {
    // Bypass tracking if the feature is disabled globally or dynamically turned off.
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active) {
        return calloc(num, size);
    }
    void* ptr = calloc(num, size);
    if (ptr) {
        // Temporarily disable tracking to prevent self-tracking the _UT_MemInfo allocation.
        _UT_mem_tracking_is_active = 0;
        _UT_MemInfo* info = (_UT_MemInfo*)malloc(sizeof(_UT_MemInfo));
        _UT_mem_tracking_is_active = 1;
        if (info) {
            info->address = ptr; info->size = num * size; info->file = file; info->line = line;
            info->next = _UT_mem_head; _UT_mem_head = info; UT_alloc_count++;
        }
    }
    return ptr;
}

// Wrapper for realloc that updates the tracking information for a memory block.
static void* _UT_realloc(void* old_ptr, size_t new_size, const char* file, int line) {
    if (old_ptr == NULL) {
        return _UT_malloc(new_size, file, line);
    }
    // Bypass tracking if the feature is disabled globally or dynamically turned off.
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active) {
        return realloc(old_ptr, new_size);
    }
    _UT_MemInfo* c = _UT_mem_head;
    while (c != NULL && c->address != old_ptr) {
        c = c->next;
    }
    if (c == NULL) {
        fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: realloc of invalid pointer (%p) at %s:%d\n", KRED, KNRM, old_ptr, file, line);
        exit(1);
    }
    void* new_ptr = realloc(old_ptr, new_size);
    if (new_ptr) {
        // Update the tracking node with the new address and size.
        c->address = new_ptr; c->size = new_size; c->file = file; c->line = line;
    }
    return new_ptr;
}

// Wrapper for free that removes an allocation from the tracking list.
static void _UT_free(void* ptr, const char* file, int line) {
    if (ptr == NULL) {
        // Calling free(NULL) is a valid no-op. The test framework can be stricter
        // and fail the test if tracking is active, as it may indicate a bug.
        if (_UT_mem_tracking_enabled && _UT_mem_tracking_is_active) {
            fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: Attempt to free NULL pointer at %s:%d\n", KRED, KNRM, file, line);
            exit(1);
        }
        return;
    }
    // Bypass tracking if the feature is disabled globally or dynamically turned off.
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active) {
        free(ptr);
        return;
    }
    _UT_MemInfo *c = _UT_mem_head, *p = NULL;
    while (c != NULL && c->address != ptr) {
        p = c; c = c->next;
    }
    if (c == NULL) {
        fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: Invalid or double-freed pointer (%p) at %s:%d\n", KRED, KNRM, ptr, file, line);
        exit(1);
    }
    // Unlink the node from the tracking list.
    if (p == NULL) _UT_mem_head = c->next;
    else p->next = c->next;

    // Temporarily disable tracking to prevent self-tracking the free of the _UT_MemInfo node.
    _UT_mem_tracking_is_active = 0;
    free(c);
    _UT_mem_tracking_is_active = 1;

    UT_free_count++;
    free(ptr);
}

// Restore original macro definitions, if they existed.
#pragma pop_macro("free")
#pragma pop_macro("realloc")
#pragma pop_macro("calloc")
#pragma pop_macro("malloc")

// Hijack standard memory functions to use our tracking wrappers.
#define malloc(size) _UT_malloc(size, __FILE__, __LINE__)
#define calloc(num, size) _UT_calloc(num, size, __FILE__, __LINE__)
#define realloc(ptr, size) _UT_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) _UT_free(ptr, __FILE__, __LINE__)

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif // UT_MEMORY_TRACKING_ENABLED

/*============================================================================*/
/* SECTION 4: TEST REGISTRATION API                                           */
/*============================================================================*/
static void _UT_register_test(_UT_TestInfo* test_info) { if (_UT_registry_head == NULL) { _UT_registry_head = test_info; g_UT_registry_tail = test_info; } else { g_UT_registry_tail->next = test_info; g_UT_registry_tail = test_info; } }
#ifdef _MSC_VER
    #pragma section(".CRT$XCU", read)
    #define _TEST_INITIALIZER(f) static void __cdecl f(void); __declspec(dllexport, allocate(".CRT$XCU")) void (__cdecl* f##_)(void) = f; static void __cdecl f(void)
#else
    #define _TEST_INITIALIZER(f) static void f(void) __attribute__((constructor)); static void f(void)
#endif
#define _UT_PASTE(a, b) a##b
#define _UT_CONCAT(a, b) _UT_PASTE(a, b)

/**
 * @brief Defines a standard test case.
 *
 * This macro creates and registers a test function within a given test suite.
 * The code block that follows the macro constitutes the body of the test.
 *
 * @param SuiteName The name of the test suite to which this test belongs.
 * @param TestDescription A descriptive name for the test case.
 */
#define TEST_CASE(SuiteName, TestDescription) static void _UT_CONCAT(test_func_, __LINE__)(void); _TEST_INITIALIZER(_UT_CONCAT(test_registrar_, __LINE__)) { static _UT_TestInfo ti = { #SuiteName, TestDescription, _UT_CONCAT(test_func_, __LINE__), NULL, NULL }; _UT_register_test(&ti); } static void _UT_CONCAT(test_func_, __LINE__)(void)

/**
 * @brief Defines a "death test" case, which is expected to terminate abnormally.
 *
 * This is used for testing conditions that should cause the program to exit
 * with a non-zero exit code or be terminated by a specific signal (e.g., segmentation fault).
 *
 * @param SuiteName The name of the test suite.
 * @param TestDescription A descriptive name for the test case.
 * @param ... A variadic list of key-value pairs to specify exit conditions.
 *        Possible keys:
 *        - .expected_msg: A substring expected to be present in stderr.
 *        - .expected_signal: The signal number expected to terminate the process (POSIX-only).
 *        - .expected_exit_code: The exact exit code expected from the process.
 */
#define TEST_DEATH_CASE(SuiteName, TestDescription, ...) static void _UT_CONCAT(test_func_, __LINE__)(void); _TEST_INITIALIZER(_UT_CONCAT(test_registrar_, __LINE__)) { static _UT_DeathExpect de = { .expected_msg = NULL, .expected_signal = 0, .expected_exit_code = -1, .min_similarity = 0.95f, __VA_ARGS__ }; static _UT_TestInfo ti = { #SuiteName, TestDescription, _UT_CONCAT(test_func_, __LINE__), &de, NULL }; _UT_register_test(&ti); } static void _UT_CONCAT(test_func_, __LINE__)(void)

/*============================================================================*/
/* SECTION 5: ASSERTION MACROS                                                */
/*============================================================================*/

static int _ut_min3(int a, int b, int c) {
    if (a < b && a < c) return a;
    if (b < a && b < c) return b;
    return c;
}

/**
 * @brief (Auxiliar function) Calculates the Levenshtein distance between two strings,
 * ignoring case differences.
 * This is a space-optimized implementation.
 */
static int _ut_levenshtein_distance(const char *s1, const char *s2) {
    int s1len = strlen(s1);
    int s2len = strlen(s2);

    int *v0 = (int *)malloc((s2len + 1) * sizeof(int));
    int *v1 = (int *)malloc((s2len + 1) * sizeof(int));

    for (int i = 0; i <= s2len; i++) {
        v0[i] = i;
    }

    for (int i = 0; i < s1len; i++) {
        v1[0] = i + 1;
        for (int j = 0; j < s2len; j++) {
            // --- LA ÚNICA LÍNEA MODIFICADA ---
            // Comparamos los caracteres en minúscula.
            int cost = (tolower((unsigned char)s1[i]) == tolower((unsigned char)s2[j])) ? 0 : 1;
            // ---------------------------------
            
            v1[j + 1] = _ut_min3(v1[j] + 1, v0[j + 1] + 1, v0[j] + cost);
        }
        for (int j = 0; j <= s2len; j++) {
            v0[j] = v1[j];
        }
    }

    int result = v1[s2len];
    free(v0);
    free(v1);
    return result;
}

/**
 * @brief  (Auxiliar function) Calculates a similarity ratio (0.0 to 1.0) based on the Levenshtein distance.
 *
 */
static float _ut_calculate_similarity_ratio(const char *s1, const char *s2) {
    int s1len = strlen(s1);
    int s2len = strlen(s2);
    if (s1len == 0 && s2len == 0) return 1.0f;

    int max_len = (s1len > s2len) ? s1len : s2len;
    int distance = _ut_levenshtein_distance(s1, s2);
    
    return 1.0f - ((float)distance / max_len);
}

#define _UT_ASSERT_GENERIC(condition, condition_str, expected_str, actual_str) do { if (!(condition)) { fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM); fprintf(stderr, "   Assertion failed: %s\n      At: %s:%d\n", condition_str, __FILE__, __LINE__); if ((expected_str)[0]) fprintf(stderr, "   Expected: %s%s%s\n", KGRN, expected_str, KNRM); if ((actual_str)[0]) fprintf(stderr, "   Got: %s%s%s\n", KRED, actual_str, KNRM); exit(1); } } while (0)
#define _UT_ASSERT_GENERIC_PROP(condition, condition_str, help_text, actual_val_str) do { if (!(condition)) { fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM); fprintf(stderr, "   Property failed: %s\n      At: %s:%d\n", condition_str, __FILE__, __LINE__); fprintf(stderr, "   Reason: %s%s%s\n", KGRN, help_text, KNRM); fprintf(stderr, "   Actual value: %s%s%s\n", KRED, actual_val_str, KNRM); exit(1); } } while (0)

/**
 * @brief Asserts that a condition is true.
 *
 * If the condition evaluates to false, the test fails immediately and prints an error message.
 *
 * @param condition The expression to evaluate.
 */
#define ASSERT(condition) _UT_ASSERT_GENERIC(!!(condition), #condition, "true", (condition) ? "true" : "false")

/**
 * @brief Asserts that a condition is false.
 *
 * If the condition evaluates to true, the test fails immediately and prints an error message.
 *
 * @param condition The expression to evaluate.
 */
#define REFUTE(condition) _UT_ASSERT_GENERIC(!(condition), #condition, "false", (condition) ? "true" : "false")

/**
 * @brief Asserts that two integer values are equal.
 *
 * If the values are not equal, the test fails and prints both the expected and actual values.
 *
 * @param expected The expected integer value.
 * @param actual The actual integer value to check.
 */
#define EQUAL_INT(expected, actual) do { int e = (expected); int a = (actual); char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _UT_ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two character values are equal.
 *
 * If the characters are not equal, the test fails and prints both.
 *
 * @param expected The expected char value.
 * @param actual The actual char value to check.
 */
#define EQUAL_CHAR(expected, actual) do { char e = (expected); char a = (actual); char e_buf[4] = {'\'', e, '\'', 0}, a_buf[4] = {'\'', a, '\'', 0}; _UT_ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two pointer values are equal.
 *
 * If the pointers are not equal, the test fails and prints both pointer addresses.
 * This is useful for verifying that pointers point to the same memory location.
 *
 * @param expected The expected pointer value.
 * @param actual The actual pointer value to check.
 */
#define EQUAL_POINTER(expected, actual) do { void* e = (void*)(expected); void* a = (void*)(actual); char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%p", e); snprintf(a_buf, 32, "%p", a); _UT_ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that a pointer value is NULL.
 *
 * If the pointer is not NULL, the test fails and prints the actual pointer address.
 *
 * @param actual The actual pointer value to check.
 */
#define EQUAL_NULL(actual) EQUAL_POINTER(NULL, (actual))

/**
 * @brief Asserts that a pointer value is not NULL.
 *
 * If the pointer is NULL, the test fails and prints a message.
 *
 * @param actual The actual pointer value to check.
 */
#define NON_EQUAL_NULL(actual) do { void* a = (void*)(actual); _UT_ASSERT_GENERIC(a != NULL, #actual " != NULL", "non-NULL pointer", "NULL"); } while (0)

/**
 * @brief Asserts that two C-style strings are equal.
 *
 * Uses strcmp for comparison. The test fails if the strings are different, or if either pointer is NULL.
 *
 * @param expected The expected string value.
 * @param actual The actual string value to check.
 */
#define EQUAL_STRING(expected, actual) do { const char* e = (expected); const char* a = (actual); _UT_ASSERT_GENERIC(e && a && strcmp(e, a) == 0, #expected " == " #actual, e ? e : "NULL", a ? a : "NULL"); } while (0)

/**
 * @brief Asserts that two custom data types are equal using a provided comparison function.
 *
 * This allows for equality testing of complex structs or custom types.
 *
 * @param expected The expected value.
 * @param actual The actual value.
 * @param compare_fn A function pointer `int (*)(expected, actual)` that returns true if the items are equal.
 * @param print_fn A function pointer `void (*)(char* buf, size_t size, val)` that prints the value into a buffer.
 */
#define EQUAL_BY(expected, actual, compare_fn, print_fn) do { __typeof__(expected) _exp = (expected); __typeof__(actual) _act = (actual); if (!compare_fn(_exp, _act)) { char _exp_str[1024] = {0}, _act_str[1024] = {0}; print_fn(_exp_str, sizeof(_exp_str), _exp); print_fn(_act_str, sizeof(_act_str), _act); _UT_ASSERT_GENERIC(0, #compare_fn "(" #expected ", " #actual ")", _exp_str, _act_str); } } while (0)

/**
 * @brief Asserts that a value satisfies a given property (predicate).
 *
 * This is useful for property-based testing where you check for characteristics
 * rather than a specific value.
 *
 * @param value The value to test.
 * @param predicate_fn A function pointer `int (*)(value)` that returns true if the property holds.
 * @param print_fn A function pointer `void (*)(char* buf, size_t size, val)` that prints the value.
 * @param help_text A descriptive string explaining the property that was violated.
 */
#define PROPERTY(value, predicate_fn, print_fn, help_text) do { __typeof__(value) _val = (value); if (!predicate_fn(_val)) { char _val_str[1024] = {0}; print_fn(_val_str, sizeof(_val_str), _val); char _cond_str[1024]; snprintf(_cond_str, sizeof(_cond_str), "%s(%s)", #predicate_fn, #value); _UT_ASSERT_GENERIC_PROP(0, _cond_str, help_text, _val_str); } } while(0)

static void _UT_print_int(char* buf, size_t size, int val) { snprintf(buf, size, "%d", val); }
static void _UT_print_char(char* buf, size_t size, char val) { snprintf(buf, size, "'%c'", val); }
static void _UT_print_string(char* buf, size_t size, const char* val) { snprintf(buf, size, "\"%s\"", val); }

/**
 * @brief Asserts that an integer value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_INT(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_int, help_text)

/**
 * @brief Asserts that a character value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_CHAR(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_char, help_text)

/**
 * @brief Asserts that a string value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_STRING(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_string, help_text)

#ifdef UT_MEMORY_TRACKING_ENABLED
/**
 * @brief (Memory Tracking) Asserts that the total number of malloc/calloc/realloc calls matches the expected count.
 *
 * This macro is only available when the `UT_MEMORY_TRACKING_ENABLED` flag is defined during compilation.
 *
 * @param expected The expected number of allocations.
 */
#define ASSERT_ALLOC_COUNT(expected) do { int e = (expected), a = UT_alloc_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _UT_ASSERT_GENERIC(e == a, "UT_alloc_count == " #expected, e_buf, a_buf); } while(0)

/**
 * @brief (Memory Tracking) Asserts that the total number of free calls matches the expected count.
 *
 * This macro is only available when the `UT_MEMORY_TRACKING_ENABLED` flag is defined during compilation.
 *
 * @param expected The expected number of successful frees.
 */
#define ASSERT_FREE_COUNT(expected) do { int e = (expected), a = UT_free_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _UT_ASSERT_GENERIC(e == a, "UT_free_count == " #expected, e_buf, a_buf); } while(0)
#endif

/*============================================================================*/
/* SECTION 6: STDOUT CAPTURE AND ASSERTIONS                                   */
/*============================================================================*/

#define _STDOUT_CAPTURE_BUFFER_SIZE 8192
static char _UT_stdout_capture_buffer[_STDOUT_CAPTURE_BUFFER_SIZE];

#ifdef _WIN32
    // Windows-specific implementation for stdout redirection.
    static HANDLE _UT_stdout_pipe_read = NULL;
    static HANDLE _UT_stdout_pipe_write = NULL;
    static HANDLE _UT_stdout_original_handle = NULL;

    static void _UT_start_capture_stdout(void) {
        fflush(stdout);
        _UT_stdout_original_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        if (!CreatePipe(&_UT_stdout_pipe_read, &_UT_stdout_pipe_write, &sa, 0)) return;
        SetStdHandle(STD_OUTPUT_HANDLE, _UT_stdout_pipe_write);
    }

    static void _UT_stop_capture_stdout(char* buffer, size_t size) {
        fflush(stdout);
        CloseHandle(_UT_stdout_pipe_write); // Close write handle to signal end of data.
        _UT_stdout_pipe_write = NULL;

        DWORD bytes_read = 0;
        // Check if there is data in the pipe before reading to avoid blocking.
        DWORD bytes_available = 0;
        if (PeekNamedPipe(_UT_stdout_pipe_read, NULL, 0, NULL, &bytes_available, NULL) && bytes_available > 0) {
            ReadFile(_UT_stdout_pipe_read, buffer, (DWORD)size - 1, &bytes_read, NULL);
        }
        buffer[bytes_read] = '\0';

        CloseHandle(_UT_stdout_pipe_read);
        _UT_stdout_pipe_read = NULL;
        SetStdHandle(STD_OUTPUT_HANDLE, _UT_stdout_original_handle);
        _UT_stdout_original_handle = NULL;
    }
#else
    // POSIX-specific implementation for stdout redirection.
    static int _UT_stdout_pipe[2] = {-1, -1};
    static int _UT_stdout_original_fd = -1;

    static void _UT_start_capture_stdout(void) {
        fflush(stdout);
        _UT_stdout_original_fd = dup(STDOUT_FILENO);
        if (pipe(_UT_stdout_pipe) == -1) return;
        dup2(_UT_stdout_pipe[1], STDOUT_FILENO);
        close(_UT_stdout_pipe[1]);
    }

    static void _UT_stop_capture_stdout(char* buffer, size_t size) {
        fflush(stdout);
        dup2(_UT_stdout_original_fd, STDOUT_FILENO); // Restore stdout
        close(_UT_stdout_original_fd);

        ssize_t bytes_read = 0;
        ssize_t total_bytes = 0;
        // Make the read end non-blocking to read all available data without hanging.
        int flags = fcntl(_UT_stdout_pipe[0], F_GETFL, 0);
        fcntl(_UT_stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

        while ((bytes_read = read(_UT_stdout_pipe[0], buffer + total_bytes, size - 1 - total_bytes)) > 0) {
            total_bytes += bytes_read;
        }
        buffer[total_bytes] = '\0';
        close(_UT_stdout_pipe[0]);
    }
#endif

// Internal helper to normalize a string: trims whitespace and collapses internal whitespace.
static void _UT_normalize_string_for_comparison(char* str) {
    char *read_ptr = str, *write_ptr = str;
    int in_whitespace = 1; // Start as if preceded by whitespace to trim leading spaces.

    while (*read_ptr) {
        if (isspace((unsigned char)*read_ptr)) {
            if (!in_whitespace) {
                *write_ptr++ = ' ';
                in_whitespace = 1;
            }
        } else {
            *write_ptr++ = *read_ptr;
            in_whitespace = 0;
        }
        read_ptr++;
    }
    // Trim trailing space if present.
    if (write_ptr > str && *(write_ptr - 1) == ' ') {
        write_ptr--;
    }
    *write_ptr = '\0';
}

/**
 * @brief Asserts that a block of code prints an exact string to stdout.
 *
 * Captures everything printed to stdout during the execution of the code block
 * and compares it against the expected string. The comparison is case-sensitive
 * and whitespace-sensitive.
 *
 * @param code_block The C code to execute (e.g., a function call or a series of statements).
 * @param expected_str The exact string that the code is expected to print.
 */
#define ASSERT_STDOUT_EQUAL(code_block, expected_str) do { \
    _UT_start_capture_stdout(); \
    { code_block; } \
    _UT_stop_capture_stdout(_UT_stdout_capture_buffer, sizeof(_UT_stdout_capture_buffer)); \
    const char* e = (expected_str); \
    _UT_ASSERT_GENERIC(e && strcmp(e, _UT_stdout_capture_buffer) == 0, #code_block " prints " #expected_str, e, _UT_stdout_capture_buffer); \
} while (0)

/**
 * @brief Asserts that a block of code prints a string equivalent to the expected one.
 *
 * "Equivalent" means the strings are the same after normalizing whitespace.
 * Normalization involves:
 * 1. Trimming leading and trailing whitespace.
 * 2. Collapsing multiple consecutive whitespace characters (spaces, tabs, newlines)
 *    into a single space.
 * This is useful for testing formatted output where the exact spacing is not critical.
 *
 * @param code_block The C code to execute.
 * @param expected_str The string to compare against.
 */
#define ASSERT_STDOUT_EQUIVALENT(code_block, expected_str) do { \
    _UT_start_capture_stdout(); \
    { code_block; } \
    _UT_stop_capture_stdout(_UT_stdout_capture_buffer, sizeof(_UT_stdout_capture_buffer)); \
    char expected_normalized[_STDOUT_CAPTURE_BUFFER_SIZE]; \
    strncpy(expected_normalized, expected_str, sizeof(expected_normalized) - 1); \
    expected_normalized[sizeof(expected_normalized) - 1] = '\0'; \
    char actual_normalized[_STDOUT_CAPTURE_BUFFER_SIZE]; \
    strncpy(actual_normalized, _UT_stdout_capture_buffer, sizeof(actual_normalized) - 1); \
    actual_normalized[sizeof(actual_normalized) - 1] = '\0'; \
    _UT_normalize_string_for_comparison(expected_normalized); \
    _UT_normalize_string_for_comparison(actual_normalized); \
    char condition_str[256]; \
    snprintf(condition_str, sizeof(condition_str), "output of '%s' is equivalent to '%s'", #code_block, #expected_str); \
    _UT_ASSERT_GENERIC(strcmp(expected_normalized, actual_normalized) == 0, condition_str, (expected_str), (_UT_stdout_capture_buffer)); \
} while (0)

#ifdef UNIT_TEST_IMPLEMENTATION
/*============================================================================*/
/* SECTION 7: THE UT_RUN_ALL_TESTS IMPLEMENTATION (FULL VERSION)                 */
/*============================================================================*/

// Internal function to run all tests. Use the UT_RUN_ALL_TESTS() macro in your main function.
int _UT_UT_RUN_ALL_TESTS_impl(int argc, char* argv[]);

/**
 * @brief Runs all registered test cases.
 *
 * This macro should be called from the `main` function of your test executable.
 * It discovers and executes all tests defined with `TEST_CASE` and `TEST_DEATH_CASE`,
 * summarizes the results, and returns an exit code.
 *
 * @return 0 if all tests pass, 1 otherwise.
 */
#define UT_RUN_ALL_TESTS() _UT_UT_RUN_ALL_TESTS_impl(argc, argv)

#ifdef _WIN32
// ============================================================================
// VERSION FOR WINDOWS
// ============================================================================
static int _UT_run_process_win(_UT_TestInfo* test, const char* executable_path, char* stderr_buffer) {
    char command_line[2048];
    snprintf(command_line, sizeof(command_line), "\"%s\" --run_test \"%s\" \"%s\"", executable_path, test->suite_name, test->test_name);
    HANDLE h_read = NULL, h_write = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&h_read, &h_write, &sa, 0)) { snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "CreatePipe failed."); return -1; }
    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.hStdError = h_write; si.hStdOutput = h_write; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: CreateProcess failed (error %lu).", KRED, KNRM, GetLastError());
        CloseHandle(h_read); CloseHandle(h_write); return 0;
    }
    CloseHandle(h_write);
    DWORD wait_result = WaitForSingleObject(pi.hProcess, UT_TEST_TIMEOUT_SECONDS * 1000);
    DWORD bytes_read = 0; ReadFile(h_read, stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE - 1, &bytes_read, NULL);
    stderr_buffer[bytes_read] = '\0'; // Null-terminate the buffer

    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exceeded timeout of %d seconds.", KRED, KNRM, UT_TEST_TIMEOUT_SECONDS);
        return 0;
    }
    
    DWORD exit_code; GetExitCodeProcess(pi.hProcess, &exit_code);
    
    if (test->death_expect) {
        const _UT_DeathExpect* de = test->death_expect;
        int msg_ok = 1;
        int exit_ok = 1;

        float similarity = 0.0f;
        if (de->expected_msg) {
            similarity = _ut_calculate_similarity_ratio(de->expected_msg, stderr_buffer);
            if (similarity < de->min_similarity) {
                msg_ok = 0;
            }
        }
        
        if (de->expected_exit_code != -1 && exit_code != (DWORD)de->expected_exit_code) {
            exit_ok = 0;
        }

        if (exit_code != 0 && msg_ok && exit_ok) {
            return 2; // Death Test PASSED
        } else {
            // --- DETAILED ERROR MESSAGE CONSTRUCTION ---
            char temp_buffer[2048];
            char reason_buffer[1024] = {0};

            if (exit_code == 0) {
                strcat(reason_buffer, "\n   - Process finished successfully (exit code 0), but a failure was expected.");
            }
            if (!exit_ok) {
                char part[256];
                snprintf(part, sizeof(part), "\n   - Incorrect exit code. Expected: %d, Got: %lu",
                         de->expected_exit_code, exit_code);
                strcat(reason_buffer, part);
            }
            if (!msg_ok && de->expected_msg) {
                char part[512];
                snprintf(part, sizeof(part), "\n   - Message mismatch (Similarity: %.2f%%, Required minimum: %.2f%%).\n     Expected (approx): \"%s\"",
                         similarity * 100.0f, de->min_similarity * 100.0f, de->expected_msg);
                strcat(reason_buffer, part);
            }

            snprintf(temp_buffer, sizeof(temp_buffer), "\n   %sTEST FAILED!%s\n   Reason: Death test criteria not met.%s",
                     KRED, KNRM, reason_buffer);

            if (bytes_read > 0) {
                strcat(temp_buffer, "\n   Got output:\n---\n");
                strcat(temp_buffer, stderr_buffer);
                strcat(temp_buffer, "\n---");
            }
            strcpy(stderr_buffer, temp_buffer);
            return 0;
        }
    }
    
    if (exit_code == 0) return 1; // Normal pass
    if (strlen(stderr_buffer) == 0) { snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exited with code 0x%X.", KRED, KNRM, (unsigned int)exit_code); }
    return 0;
}

#else // POSIX
// ============================================================================
// VERSION FOR POSIX (LINUX, MACOS)
// ============================================================================
static int _UT_wait_with_timeout(pid_t pid, int* status, int timeout_sec) { struct timespec start, now, sleep_ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 }; clock_gettime(CLOCK_MONOTONIC, &start); for (;;) { pid_t r = waitpid(pid, status, WNOHANG); if (r == pid) return 0; if (r == -1 && errno != EINTR) return -1; clock_gettime(CLOCK_MONOTONIC, &now); if ((now.tv_sec - start.tv_sec) >= timeout_sec) { kill(pid, SIGKILL); while (waitpid(pid, status, 0) == -1 && errno == EINTR); return 1; } nanosleep(&sleep_ts, NULL); } }
static int _UT_run_process_posix(_UT_TestInfo* test, const char* executable_path, char* stderr_buffer) {
    int err_pipe[2] = {-1, -1};
    if (pipe(err_pipe) == -1) { perror("pipe"); return -1; }
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); close(err_pipe[0]); close(err_pipe[1]); return -1; }
    if (pid == 0) {
        close(err_pipe[0]); dup2(err_pipe[1], STDOUT_FILENO); dup2(err_pipe[1], STDERR_FILENO); close(err_pipe[1]);
        char* child_argv[] = {(char*)executable_path, "--run_test", (char*)test->suite_name, (char*)test->test_name, NULL};
        execv(executable_path, child_argv);
        perror("execv failed"); exit(127);
    } else {
        close(err_pipe[1]); int status; int r = _UT_wait_with_timeout(pid, &status, UT_TEST_TIMEOUT_SECONDS);
        ssize_t off = 0, bytes_read;
        int flags = fcntl(err_pipe[0], F_GETFL); fcntl(err_pipe[0], F_SETFL, flags | O_NONBLOCK);
        while ((bytes_read = read(err_pipe[0], stderr_buffer + off, UT_UT_SUITE_RESULTS_BUFFER_SIZE - 1 - off)) > 0) { off += bytes_read; }
        stderr_buffer[off] = '\0'; // Null-terminate the buffer
        close(err_pipe[0]);

        if (r == 1) { snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exceeded timeout of %d seconds.", KRED, KNRM, UT_TEST_TIMEOUT_SECONDS); return 0; }
        else if (r == -1) { perror("waitpid"); return -1; }

        if (test->death_expect) {
            const _UT_DeathExpect* de = test->death_expect;
            int sig_ok = 1, exit_ok = 1, msg_ok = 1;
            
            int received_signal = -1;
            int received_exit_code = -1;

            if (WIFSIGNALED(status)) {
                received_signal = WTERMSIG(status);
                if (de->expected_signal != 0 && received_signal != de->expected_signal) sig_ok = 0;
                if (de->expected_exit_code != -1) exit_ok = 0; // Expected exit code, not signal
            } else if (WIFEXITED(status)) {
                received_exit_code = WEXITSTATUS(status);
                if (de->expected_exit_code != -1 && received_exit_code != de->expected_exit_code) exit_ok = 0;
                if (de->expected_signal != 0) sig_ok = 0; // Expected signal, not exit code
            } else {
                sig_ok = 0; exit_ok = 0;
            }

            float similarity = 0.0f;
            if (de->expected_msg) {
                similarity = _ut_calculate_similarity_ratio(de->expected_msg, stderr_buffer);
                if (similarity < de->min_similarity) {
                    msg_ok = 0;
                }
            }

            if (sig_ok && exit_ok && msg_ok && (WIFSIGNALED(status) || WIFEXITED(status))) {
                return 2; // Death Test PASSED
            } else {
                // --- DETAILED ERROR MESSAGE CONSTRUCTION ---
                char temp_buffer[2048];
                char reason_buffer[1024] = {0};

                if (!sig_ok) {
                    char part[256];
                    snprintf(part, sizeof(part), "\n   - Incorrect signal. Expected: %d (%s), Got: %d (%s)",
                             de->expected_signal, strsignal(de->expected_signal),
                             received_signal, received_signal != -1 ? strsignal(received_signal) : "N/A (terminated by exit code)");
                    strcat(reason_buffer, part);
                }
                if (!exit_ok) {
                    char part[256];
                    snprintf(part, sizeof(part), "\n   - Incorrect exit code. Expected: %d, Got: %d",
                             de->expected_exit_code, received_exit_code);
                    strcat(reason_buffer, part);
                }
                if (!msg_ok && de->expected_msg) {
                    char part[512];
                    snprintf(part, sizeof(part), "\n   - Message mismatch (Similarity: %.2f%%, Required minimum: %.2f%%).\n     Expected (approx): \"%s\"",
                             similarity * 100.0f, de->min_similarity * 100.0f, de->expected_msg);
                    strcat(reason_buffer, part);
                }

                snprintf(temp_buffer, sizeof(temp_buffer), "\n   %sTEST FAILED!%s\n   Reason: Death test criteria not met.%s",
                         KRED, KNRM, reason_buffer);
                
                if (off > 0) {
                    strcat(temp_buffer, "\n   Got output:\n---\n");
                    strcat(temp_buffer, stderr_buffer);
                    strcat(temp_buffer, "\n---");
                }
                strcpy(stderr_buffer, temp_buffer);
                return 0;
            }
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
        if (strlen(stderr_buffer) == 0) {
            if (WIFSIGNALED(status)) { snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Terminated by signal: %s.", KRED, KNRM, strsignal(WTERMSIG(status))); }
            else { snprintf(stderr_buffer, UT_UT_SUITE_RESULTS_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exited with code %d.", KRED, KNRM, WEXITSTATUS(status)); }
        }
        return 0;
    }
}
#endif

static void _UT_print_colored_details(const char* details) {
    printf("Details: ");
    for (size_t i = 0; i < strlen(details); i++) {
        if (details[i] == '+') printf("%s+%s", KGRN, KNRM);
        else if (details[i] == '-') printf("%s-%s", KRED, KNRM);
        else printf("%c", details[i]);
    }
}

static void _UT_finalize_suite(const char* name, int total, int failed, const char* details) {
    if (total == 0) return;
    printf("\n\n%sPassed%s: %s%d%s, %sFailed%s: %s%d%s, Total: %d, ", KGRN, KNRM, KGRN, total - failed, KNRM, KRED, KNRM, KRED, failed, KNRM, total);
    _UT_print_colored_details(details);
    printf("\n\n");
    if (_UT_suite_count < UT_MAX_SUITES) {
        _UT_all_suite_results[_UT_suite_count].passed = total - failed;
        _UT_all_suite_results[_UT_suite_count].total = total;
        strncpy(_UT_all_suite_results[_UT_suite_count].details, details, UT_SUITE_RESULTS_BUFFER_SIZE -1);
        _UT_all_suite_results[_UT_suite_count].details[UT_SUITE_RESULTS_BUFFER_SIZE -1] = '\0';
    }
    _UT_suite_count++;
}

int _UT_UT_RUN_ALL_TESTS_impl(int argc, char* argv[]) {
    if (argc == 4 && strcmp(argv[1], "--run_test") == 0) {
        _UT_TestInfo* current = _UT_registry_head;
        while (current) {
            if (strcmp(current->suite_name, argv[2]) == 0 && strcmp(current->test_name, argv[3]) == 0) {
                setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
                #ifdef UT_MEMORY_TRACKING_ENABLED
                _UT_init_memory_tracking();
                #endif
                current->func();
                 #ifdef UT_MEMORY_TRACKING_ENABLED
                // Only check for leaks if it's enabled for this test.
                if (_UT_leak_UT_check_enabled) {
                    _UT_check_for_leaks();
                }
                #endif
                return 0;
            }
            current = current->next;
        }
        fprintf(stderr, "Error: Test '%s.%s' not found in registry.\n", argv[2], argv[3]); return 1;
    }
    _UT_is_ci_mode = 1; // argc > 1;
    int total = 0, passed = 0, failed = 0;
    int suite_total = 0, suite_failed = 0;
    char suite_results[UT_SUITE_RESULTS_BUFFER_SIZE];
    int suite_results_idx = 0;
    const char* current_suite = "";
    const char* executable_path = argv[0];
    _UT_init_colors();
    _UT_TestInfo* current_test = _UT_registry_head;
    while (current_test) {
        if (strcmp(current_suite, current_test->suite_name) != 0) {
            _UT_finalize_suite(current_suite, suite_total, suite_failed, suite_results);
            current_suite = current_test->suite_name;
            printf("%sTests for %s%s\n", KBLU, current_suite, KNRM);
            for(int i=0; i < (int)(strlen(current_suite) + 10); ++i) printf("%s=%s", KBLU, KNRM);
            suite_total = 0; suite_failed = 0; suite_results_idx = 0; 
            for(int i=0; i<UT_SUITE_RESULTS_BUFFER_SIZE; i++) suite_results[i] = '\0';
        }
        printf("\n%s: ", current_test->test_name);
        fflush(stdout);
        char stderr_buffer[UT_UT_SUITE_RESULTS_BUFFER_SIZE] = {0};
        #ifdef _WIN32
            int test_result = _UT_run_process_win(current_test, executable_path, stderr_buffer);
        #else
            int test_result = _UT_run_process_posix(current_test, executable_path, stderr_buffer);
        #endif
        if (test_result > 0) { // 1 for pass, 2 for death test pass
            if (test_result == 2) { printf("\n   %sTEST PASSED SUCCESSFULLY!%s (Abnormal exit expected)", KGRN, KNRM); }
            else { printf("\n   %sTEST PASSED SUCCESSFULLY!%s", KGRN, KNRM); }
            passed++; if (suite_results_idx < UT_SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '+';
        } else { // 0 for fail, -1 for error
            failed++; suite_failed++; if (suite_results_idx < UT_SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '-';
            printf("%s", stderr_buffer);
        }
        total++; suite_total++; current_test = current_test->next;
    }
    suite_results[suite_results_idx] = '\0';
    _UT_finalize_suite(current_suite, suite_total, suite_failed, suite_results);
    double rate = total > 0 ? ((double)passed / total) * 100.0 : 100.0;
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("%s Overall Summary%s\n", KBLU, KNRM);
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("Suites run:    %d\n", _UT_suite_count);
    printf("Total tests:   %d\n", total);
    printf("%sPassed:        %d%s\n", KGRN, passed, KNRM);
    printf("%sFailed:        %d%s\n", KRED, failed, KNRM);
    printf("Success rate:  %.2f%%\n", rate);
    printf("%s========================================%s\n", KBLU, KNRM);
    if(_UT_is_ci_mode) {
        printf("\n");
        for(int i=0; i < _UT_suite_count; ++i) { printf("%d/%d%s", _UT_all_suite_results[i].passed, _UT_all_suite_results[i].total, i == _UT_suite_count - 1 ? "" : " "); }
        printf("\n");
        for(int i=0; i < _UT_suite_count; ++i) { for(int j=0; j < _UT_all_suite_results[i].total; ++j) { printf("%c%s", _UT_all_suite_results[i].details[j], j == _UT_all_suite_results[i].total - 1 ? "" : ";"); } if (i < _UT_suite_count - 1) printf(";;"); }
        printf("\n");
        for(int i=0; i < _UT_suite_count; ++i) { printf("%d%s", _UT_all_suite_results[i].passed, i == _UT_suite_count - 1 ? "" : ";"); }
        printf("\n");
        for(int i=0; i < _UT_suite_count; ++i) { double r = _UT_all_suite_results[i].total > 0 ? (double)_UT_all_suite_results[i].passed / _UT_all_suite_results[i].total : 1.0; printf("%.3f%s", r, i == _UT_suite_count - 1 ? "" : ";"); }
        printf("\n");
    }
    return failed > 0 ? 1 : 0;
}
#endif // UNIT_TEST_IMPLEMENTATION

#endif // UNIT_TEST_H