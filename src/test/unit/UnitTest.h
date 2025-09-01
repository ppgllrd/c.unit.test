/*============================================================================*/
/* Unit Test Framework                                                        */
/* Pepe Gallardo, 2025                                                        */
/*                                                                            */
/* USAGE MODES:                                                               */
/*                                                                            */
/* This header can be used in several modes, controlled by preprocessor       */
/* definitions. You should define the desired mode(s) *before* including      */
/* this header.                                                               */
/*                                                                            */
/* The UNIT_TEST_MEMORY_TRACKING definition is an orthogonal flag that        */
/* exclusively controls whether malloc, free, etc., are overridden.           */
/*                                                                            */
/* --- Primary Modes ---                                                      */
/*                                                                            */
/* 1. Default Mode (Memory Tracking Only)                                     */
/*    - How to use: #include "UnitTest.h"                                     */
/*    - What it does: Overrides malloc, calloc, realloc, and free to enable   */
/*      memory leak detection. Test-writing APIs (TEST_CASE, ASSERT) are      */
/*      not available.                                                        */
/*    - Use case: Include this in your regular project source files (.c)      */
/*      to track their memory usage during tests.                             */
/*                                                                            */
/* 2. UNIT_TEST_DECLARATION (Writing Tests, No Memory Tracking)               */
/*    - How to use: #define UNIT_TEST_DECLARATION                             */
/*                  #include "UnitTest.h"                                     */
/*    - What it does: Provides all macros (TEST_CASE, ASSERT) and types       */
/*      needed to *write* test files. It does NOT override memory functions.  */
/*    - Use case: Use this in `_test.c` files to define tests without         */
/*      tracking memory allocations made directly within that file.           */
/*                                                                            */
/* 3. UNIT_TEST_IMPLEMENTATION (Test Runner, No Memory Tracking)              */
/*    - How to use: #define UNIT_TEST_IMPLEMENTATION                          */
/*                  #include "UnitTest.h"                                     */
/*    - What it does: Includes the full implementation of the test runner.    */
/*      This automatically enables UNIT_TEST_DECLARATION. It does NOT         */
/*      override memory functions on its own.                                 */
/*                                                                            */
/* --- Combining with UNIT_TEST_MEMORY_TRACKING ---                           */
/*                                                                            */
/* You can combine the declaration/implementation modes with memory tracking. */
/*                                                                            */
/*   A. UNIT_TEST_DECLARATION + UNIT_TEST_MEMORY_TRACKING                     */
/*      - What it does: Provides test-writing APIs AND overrides memory       */
/*        functions.                                                          */
/*      - Use case: The standard mode for a `_test.c` file, allowing you to   */
/*        write tests and track their memory usage.                           */
/*                                                                            */
/*   B. UNIT_TEST_IMPLEMENTATION + UNIT_TEST_MEMORY_TRACKING                  */
/*      - What it does: Includes the full test runner implementation AND      */
/*        overrides memory functions.                                         */
/*      - Use case: The standard mode for your main test runner file.         */
/*                                                                            */
/* Example Project Structure:                                                 */
/*                                                                            */
/*   // In my_data_structure.c (your project code)                            */
/*   #include "UnitTest.h" // Default mode: enables memory function overrides */
/*   // ... implementation of data structure ...                              */
/*                                                                            */
/*   // In test_data_structure.c (a test file)                                */
/*   #define UNIT_TEST_DECLARATION                                            */
/*   #define UNIT_TEST_MEMORY_TRACKING // Also track memory in this file      */
/*   #include "UnitTest.h"                                                    */
/*   TEST_CASE(MyDS, ShouldDoSomething) { ... }                               */
/*                                                                            */
/*   // In main.c (the test runner executable)                                */
/*   #define UNIT_TEST_IMPLEMENTATION                                         */
/*   #define UNIT_TEST_MEMORY_TRACKING // Also track memory in the runner     */
/*   #include "UnitTest.h"                                                    */
/*   int main(int argc, char* argv[]) {                                       */
/*       return UT_RUN_ALL_TESTS();                                           */
/*   }                                                                        */
/*============================================================================*/

#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include <time.h> // For timing execution

/*============================================================================*/
/* SECTION 0: MODE SELECTION & FRAMEWORK CONSTANTS                            */
/*============================================================================*/
// The implementation requires the declarations.
#ifdef UNIT_TEST_IMPLEMENTATION
#define UNIT_TEST_DECLARATION
#endif

// A. Define UNIT_TEST_MEMORY_TRACKING in the default case
// where neither DECLARATION nor IMPLEMENTATION is defined.
#if (!defined(UNIT_TEST_DECLARATION) && !defined(UNIT_TEST_IMPLEMENTATION))
#define UNIT_TEST_MEMORY_TRACKING
#endif

// B. Define UT_MEMORY_TRACKING_ENABLED if the memory tracking *code* (structs,
// function definitions) is needed. This is required if we are overriding
// memory functions OR if we are building the full implementation (which
// needs to run the leak checker).
#if defined(UNIT_TEST_MEMORY_TRACKING) || defined(UNIT_TEST_IMPLEMENTATION)
#define UT_MEMORY_TRACKING_ENABLED
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// Unified assert macro definition
#undef assert
#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define _UT_assert_print(expr) fprintf(stderr, "Assertion failed: %s on file %s line %d\n", #expr, __FILE__, __LINE__)
#ifdef _WIN32
#define assert(expr) \
    ((expr) ? (void)0 : (_UT_assert_print(expr), exit(_UT_ASSERT_EXIT_CODE)))
#else // POSIX
#define assert(expr) \
    ((expr) ? (void)0 : (_UT_assert_print(expr), abort()))
#endif // _WIN32
#endif // NDEBUG

/*============================================================================*/
/* SECTION 1: CORE DEFINITIONS & PLATFORM ABSTRACTION                         */
/* (Included for both DECLARATION and IMPLEMENTATION)                         */
/*============================================================================*/
#if defined(UNIT_TEST_DECLARATION)

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define UT_IS_TTY _isatty(_fileno(stdout))
#else // POSIX
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#define UT_IS_TTY isatty(STDOUT_FILENO)
#endif

#ifndef UT_TEST_TIMEOUT_SECONDS
#define UT_TEST_TIMEOUT_SECONDS 2
#endif

#endif // UNIT_TEST_DECLARATION

#if defined(UNIT_TEST_IMPLEMENTATION)
#define _UT_STDERR_BUFFER_SIZE 4096
#define _UT_SUITE_RESULTS_DETAILS_SIZE 1024
#define _UT_MAX_SUITES 128
#define _UT_SERIALIZATION_BUFFER_SIZE 8192

// Forward declarations
typedef void (*_UT_TestFunction)(void);
typedef struct _UT_TestInfo _UT_TestInfo;
typedef struct
{
    const char *expected_msg;
    int expected_signal;
    int expected_exit_code;
    float min_similarity;
} _UT_DeathExpect;

struct _UT_TestInfo
{
    const char *suite_name;
    const char *test_name;
    _UT_TestFunction func;
    const _UT_DeathExpect *death_expect;
    _UT_TestInfo *next;
};

#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 1.5: RESULT DATA MODEL                                             */
/* (Included for both DECLARATION and IMPLEMENTATION)                         */
/*============================================================================*/
#if defined(UNIT_TEST_IMPLEMENTATION)

// Enum for the status of a test result
typedef enum
{
    _UT_STATUS_PENDING,
    _UT_STATUS_PASSED,
    _UT_STATUS_FAILED,
    _UT_STATUS_CRASHED,
    _UT_STATUS_TIMEOUT,
    _UT_STATUS_DEATH_TEST_PASSED
} _UT_TestStatus;

// Represents a single assertion failure
typedef struct _UT_AssertionFailure
{
    char *file;
    int line;
    char *condition_str;
    char *expected_str;
    char *actual_str;
    struct _UT_AssertionFailure *next;
} _UT_AssertionFailure;

// Represents the complete result of a single test case
typedef struct _UT_TestResult
{
    const char *suite_name;
    const char *test_name;
    _UT_TestStatus status;
    double duration_ms;
    char *captured_output;          // Combined stdout/stderr from child process
    _UT_AssertionFailure *failures; // Linked list of failures
    struct _UT_TestResult *next;
} _UT_TestResult;

// Represents the aggregated results for a test suite
typedef struct _UT_SuiteResult
{
    const char *name;
    int total_tests;
    int passed_tests;
    char details[_UT_SUITE_RESULTS_DETAILS_SIZE];
    int details_idx;
    _UT_TestResult *test_results_head;
    _UT_TestResult *test_results_tail;
    struct _UT_SuiteResult *next;
} _UT_SuiteResult;

// Represents the entire test run
typedef struct
{
    int total_suites;
    int total_tests;
    int passed_tests;
    double total_duration_ms;
    _UT_SuiteResult *suites_head;
    _UT_SuiteResult *suites_tail;
} _UT_TestRun;

#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 2: GLOBAL STATE AND COLOR MANAGEMENT                               */
/*============================================================================*/
#if defined(UNIT_TEST_IMPLEMENTATION)

static _UT_TestInfo *_UT_registry_head = NULL;
static _UT_TestInfo *g_UT_registry_tail = NULL;
static int _UT_use_color = 1;
static int _UT_is_ci_mode = 0;

// Global state for the currently running test (in the child process)
static _UT_TestResult *g_UT_current_test_result = NULL;

#define KNRM (_UT_use_color ? "\x1B[0m" : "")
#define KRED (_UT_use_color ? "\x1B[31m" : "")
#define KGRN (_UT_use_color ? "\x1B[32m" : "")
#define KYEL (_UT_use_color ? "\x1B[33m" : "")
#define KBLU (_UT_use_color ? "\x1B[34m" : "")

static void _UT_init_colors(void)
{
    const char *no_color = getenv("NO_COLOR");
    _UT_use_color = UT_IS_TTY && !no_color;
#ifdef _WIN32
    if (_UT_use_color)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE)
        {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode))
            {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
#endif
}
#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 3: ADVANCED MEMORY TRACKING                                        */
/*============================================================================*/
#ifdef UT_MEMORY_TRACKING_ENABLED

// Shared global state for memory tracking.
// Declared as 'extern' so all modules see the same variables.
// Defined once in the file that sets UNIT_TEST_IMPLEMENTATION.
extern int UT_alloc_count, UT_free_count;
extern size_t g_UT_total_bytes_allocated;
extern size_t g_UT_total_bytes_freed;

#ifdef UNIT_TEST_IMPLEMENTATION
// Node for the linked list that tracks memory allocations.
typedef struct _UT_MemInfo
{
    void *address;
    size_t size;
    const char *file;
    int line;
    int is_baseline;
    struct _UT_MemInfo *next;
} _UT_MemInfo;

// Actual definitions of the global variables. This code will only be
// compiled into the single .c file that defines UNIT_TEST_IMPLEMENTATION.
_UT_MemInfo *_UT_mem_head = NULL;
int UT_alloc_count = 0, UT_free_count = 0;
int _UT_mem_tracking_enabled = 0;
int _UT_mem_tracking_is_active = 1;
int _UT_leak_UT_check_enabled = 1;
size_t g_UT_total_bytes_allocated = 0;
size_t g_UT_total_bytes_freed = 0;

#endif // UNIT_TEST_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4090 4091)
#endif

// Forward declaration for failure recording to avoid dependency issues.
#if defined(UNIT_TEST_DECLARATION)
void _UT_record_failure(const char *file, int line, const char *cond_str, const char *exp_str, const char *act_str);
#endif

/**
 * @brief Dynamically enables memory tracking at runtime.
 *        Does nothing if UT_MEMORY_TRACKING_ENABLED was not defined at compile time.
 */
void UT_enable_memory_tracking(void);

/**
 * @brief Dynamically disables memory tracking at runtime.
 *        Allocations and frees will not be tracked until it is re-enabled.
 */
void UT_disable_memory_tracking(void);

/**
 * @brief Disables the final memory leak check for the current test.
 *        Useful for tests that intentionally don't free setup memory.
 */
void UT_disable_leak_check(void);

/**
 * @brief Marks all currently tracked memory blocks as 'baseline'.
 *
 * This function iterates through all allocations currently known to the memory
 * tracker and flags them. These flagged blocks will be ignored by the end-of-test
 * memory leak check. However, they remain tracked, allowing functions under test
 * to legally free them.
 */
void UT_mark_memory_as_baseline(void);

// Wrapper declarations
void *_UT_malloc(size_t size, const char *file, int line);
void *_UT_calloc(size_t num, size_t size, const char *file, int line);
void *_UT_realloc(void *old_ptr, size_t new_size, const char *file, int line);
void _UT_free(void *ptr, const char *file, int line);

#ifdef UNIT_TEST_MEMORY_TRACKING
// Hijack standard memory functions to use our tracking wrappers.
#define malloc(size) _UT_malloc(size, __FILE__, __LINE__)
#define calloc(num, size) _UT_calloc(num, size, __FILE__, __LINE__)
#define realloc(ptr, size) _UT_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) _UT_free(ptr, __FILE__, __LINE__)
#endif // UNIT_TEST_MEMORY_TRACKING

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif // UT_MEMORY_TRACKING_ENABLED

#ifdef UNIT_TEST_IMPLEMENTATION
#ifdef UT_MEMORY_TRACKING_ENABLED

// ============================================================================
// Undefine macros for the implementation of the memory wrappers
// to prevent infinite recursion. All calls to malloc, calloc, etc. inside this
// block will refer to the original standard library functions.
// ============================================================================
#pragma push_macro("malloc")
#pragma push_macro("calloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef calloc
#undef realloc
#undef free

// Helper to safely duplicate a string, using the real malloc.
static char *_UT_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char *new_s = (char *)malloc(len);
    if (new_s)
        memcpy(new_s, s, len);
    return new_s;
}

// DEFINITIONS of memory utility functions. Not static.
void UT_enable_memory_tracking(void) { _UT_mem_tracking_is_active = 1; }
void UT_disable_memory_tracking(void) { _UT_mem_tracking_is_active = 0; }
void UT_disable_leak_check(void) { _UT_leak_UT_check_enabled = 0; }
void UT_mark_memory_as_baseline(void)
{
    if (!_UT_mem_tracking_enabled)
        return;
    _UT_MemInfo *current = _UT_mem_head;
    while (current != NULL)
    {
        current->is_baseline = 1;
        current = current->next;
    }
}

// Resets the memory tracking state for a new test run.
static void _UT_init_memory_tracking(void)
{
    while (_UT_mem_head != NULL)
    {
        _UT_MemInfo *temp = _UT_mem_head;
        _UT_mem_head = _UT_mem_head->next;
        free(temp); // Use original free to release tracking nodes.
    }
    UT_alloc_count = 0;
    UT_free_count = 0;
    g_UT_total_bytes_allocated = 0; // Reset byte counter
    g_UT_total_bytes_freed = 0;     // Reset byte counter
    _UT_mem_tracking_enabled = 1;
    _UT_mem_tracking_is_active = 1; // Ensure tracking is active by default.
    _UT_leak_UT_check_enabled = 1;  // Ensure leak checking is active by default for each test.
}

// Checks for leaks and records them as an assertion failure instead of exiting.
static void _UT_check_for_leaks(void)
{
    _UT_mem_tracking_enabled = 0; // Disable tracking during check.
    _UT_MemInfo *current = _UT_mem_head;
    int leaks_found = 0;
    char leak_details[1024] = "Memory leak detected.";

    while (current != NULL)
    {
        if (current->is_baseline == 0)
        {
            leaks_found = 1;
            char leak_info[256];
            snprintf(leak_info, sizeof(leak_info), "\n      - %zu bytes allocated at %s:%d", current->size, current->file, current->line);
            strncat(leak_details, leak_info, sizeof(leak_details) - strlen(leak_details) - 1);
        }
        current = current->next;
    }

    if (leaks_found)
    {
        _UT_record_failure("Memory Tracker", 0, "No memory leaks", "0 un-freed allocations", leak_details);
    }
}

// Wrapper for malloc that adds allocation details to the tracking list.
void *_UT_malloc(size_t size, const char *file, int line)
{
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active)
    {
        return malloc(size);
    }
    void *ptr = malloc(size);
    if (ptr)
    {
        g_UT_total_bytes_allocated += size; // Track allocated bytes
        _UT_mem_tracking_is_active = 0;
        _UT_MemInfo *info = (_UT_MemInfo *)malloc(sizeof(_UT_MemInfo));
        _UT_mem_tracking_is_active = 1;
        if (info)
        {
            info->address = ptr;
            info->size = size;
            info->file = file;
            info->line = line;
            info->is_baseline = 0;
            info->next = _UT_mem_head;
            _UT_mem_head = info;
            UT_alloc_count++;
        }
    }
    return ptr;
}

// Wrapper for calloc that adds allocation details to the tracking list.
void *_UT_calloc(size_t num, size_t size, const char *file, int line)
{
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active)
    {
        return calloc(num, size);
    }
    void *ptr = calloc(num, size);
    if (ptr)
    {
        size_t total_size = num * size;
        g_UT_total_bytes_allocated += total_size; // Track allocated bytes
        _UT_mem_tracking_is_active = 0;
        _UT_MemInfo *info = (_UT_MemInfo *)malloc(sizeof(_UT_MemInfo));
        _UT_mem_tracking_is_active = 1;
        if (info)
        {
            info->address = ptr;
            info->size = total_size;
            info->file = file;
            info->line = line;
            info->is_baseline = 0;
            info->next = _UT_mem_head;
            _UT_mem_head = info;
            UT_alloc_count++;
        }
    }
    return ptr;
}

// realloc failure records an assertion and exits, as it's a critical error.
void *_UT_realloc(void *old_ptr, size_t new_size, const char *file, int line)
{
    if (old_ptr == NULL)
    {
        return _UT_malloc(new_size, file, line);
    }
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active)
    {
        return realloc(old_ptr, new_size);
    }

    _UT_MemInfo *c = _UT_mem_head;
    while (c != NULL && c->address != old_ptr)
    {
        c = c->next;
    }

    if (c == NULL)
    {
        fprintf(stderr, "FATAL: realloc of invalid pointer (%p) at %s:%d\n", old_ptr, file, line);
        exit(120); // Special exit code for fatal error
    }

    size_t old_size = c->size;
    void *new_ptr = realloc(old_ptr, new_size);

    if (new_ptr)
    {
        // Update total bytes allocated by the difference
        if (new_size > old_size)
        {
            g_UT_total_bytes_allocated += (new_size - old_size);
        }
        else
        {
            // If shrinking, this is effectively a partial free
            g_UT_total_bytes_freed += (old_size - new_size);
        }
        c->address = new_ptr;
        c->size = new_size;
        c->file = file;
        c->line = line;
    }
    // Note: If realloc fails, new_ptr is NULL, and we don't update tracking.
    // The original pointer old_ptr is still valid and must be freed.

    return new_ptr;
}

// free failure records an assertion and exits, as it's a critical error.
void _UT_free(void *ptr, const char *file, int line)
{
    if (ptr == NULL)
    {
        if (_UT_mem_tracking_enabled && _UT_mem_tracking_is_active)
        {
            fprintf(stderr, "FATAL: Attempt to free NULL pointer at %s:%d\n", file, line);
            exit(121); // Special exit code
        }
        return;
    }
    if (!_UT_mem_tracking_enabled || !_UT_mem_tracking_is_active)
    {
        free(ptr);
        return;
    }

    _UT_MemInfo *c = _UT_mem_head, *p = NULL;
    while (c != NULL && c->address != ptr)
    {
        p = c;
        c = c->next;
    }

    if (c == NULL)
    {
        fprintf(stderr, "FATAL: Invalid or double-freed pointer (%p) at %s:%d\n", ptr, file, line);
        exit(122); // Special exit code
    }

    g_UT_total_bytes_freed += c->size; // Track freed bytes

    if (p == NULL)
        _UT_mem_head = c->next;
    else
        p->next = c->next;

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

#endif // UT_MEMORY_TRACKING_ENABLED
#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 4: TEST REGISTRATION API                                           */
/* (Included for DECLARATION and IMPLEMENTATION)                              */
/*============================================================================*/
#if defined(UNIT_TEST_DECLARATION)

#ifdef _WIN32
// On Windows (MSVC), initializers run in reverse. We prepend to the list
// to reverse the order again, resulting in the correct final order.
static void _UT_register_test(_UT_TestInfo *test_info)
{
    test_info->next = _UT_registry_head;
    _UT_registry_head = test_info;
}
#else
// On POSIX (GCC/Clang), constructors run in forward order. We append
// to the tail to preserve this order.
static void _UT_register_test(_UT_TestInfo *test_info)
{
    if (_UT_registry_head == NULL)
    {
        _UT_registry_head = test_info;
        g_UT_registry_tail = test_info;
    }
    else
    {
        g_UT_registry_tail->next = test_info;
        g_UT_registry_tail = test_info;
    }
}
#endif

#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
#define _TEST_INITIALIZER(f)                                                    \
    static void __cdecl f(void);                                                \
    __declspec(dllexport, allocate(".CRT$XCU")) void(__cdecl * f##_)(void) = f; \
    static void __cdecl f(void)
#else
#define _TEST_INITIALIZER(f)                          \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
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
#define TEST_CASE(SuiteName, TestDescription)                                                                  \
    static void _UT_CONCAT(test_func_, __LINE__)(void);                                                        \
    _TEST_INITIALIZER(_UT_CONCAT(test_registrar_, __LINE__))                                                   \
    {                                                                                                          \
        static _UT_TestInfo ti = {#SuiteName, #TestDescription, _UT_CONCAT(test_func_, __LINE__), NULL, NULL}; \
        _UT_register_test(&ti);                                                                                \
    }                                                                                                          \
    static void _UT_CONCAT(test_func_, __LINE__)(void)

// Helper macros for conditionally suppressing GCC warnings
#ifdef __GNUC__
#define _UT_GCC_DIAG_PUSH _Pragma("GCC diagnostic push")
#define _UT_GCC_DIAG_POP _Pragma("GCC diagnostic pop")
#define _UT_GCC_DIAG_IGNORE_OVERRIDE_INIT _Pragma("GCC diagnostic ignored \"-Woverride-init\"")
#else
#define _UT_GCC_DIAG_PUSH
#define _UT_GCC_DIAG_POP
#define _UT_GCC_DIAG_IGNORE_OVERRIDE_INIT
#endif

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
#define TEST_DEATH_CASE(SuiteName, TestDescription, ...)                                                                                          \
    static void _UT_CONCAT(test_func_, __LINE__)(void);                                                                                           \
    _UT_GCC_DIAG_PUSH                                                                                                                             \
    _UT_GCC_DIAG_IGNORE_OVERRIDE_INIT                                                                                                             \
    _TEST_INITIALIZER(_UT_CONCAT(test_registrar_, __LINE__))                                                                                      \
    {                                                                                                                                             \
        static _UT_DeathExpect de = {.expected_msg = NULL, .expected_signal = 0, .expected_exit_code = -1, .min_similarity = 0.95f, __VA_ARGS__}; \
        static _UT_TestInfo ti = {#SuiteName, #TestDescription, _UT_CONCAT(test_func_, __LINE__), &de, NULL};                                     \
        _UT_register_test(&ti);                                                                                                                   \
    }                                                                                                                                             \
    _UT_GCC_DIAG_POP                                                                                                                              \
    static void _UT_CONCAT(test_func_, __LINE__)(void)

#endif // UNIT_TEST_DECLARATION

/*============================================================================*/
/* SECTION 5: ASSERTION MACROS                                                */
/* (Included for DECLARATION and IMPLEMENTATION)                              */
/*============================================================================*/
#if defined(UNIT_TEST_DECLARATION)

// Internal helper function to record a failure. Not a macro.
// Implementation is provided only when UNIT_TEST_IMPLEMENTATION is defined.
void _UT_record_failure(const char *file, int line, const char *cond_str, const char *exp_str, const char *act_str);

/**
 * @brief Asserts that a condition is true.
 *
 * If the condition evaluates to false, the test fails, records the failure, and continues execution.
 *
 * @param condition The expression to evaluate.
 */
#define ASSERT(condition)                                                        \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            _UT_record_failure(__FILE__, __LINE__, #condition, "true", "false"); \
        }                                                                        \
    } while (0)

/**
 * @brief Asserts that a condition is false.
 *
 * If the condition evaluates to true, the test fails, records the failure, and continues execution.
 *
 * @param condition The expression to evaluate.
 */
#define REFUTE(condition)                                                                 \
    do                                                                                    \
    {                                                                                     \
        if (condition)                                                                    \
        {                                                                                 \
            _UT_record_failure(__FILE__, __LINE__, "!(" #condition ")", "false", "true"); \
        }                                                                                 \
    } while (0)

/**
 * @brief Asserts that two integer values are equal.
 *
 * If the values are not equal, the test fails and records both the expected and actual values.
 *
 * @param expected The expected integer value.
 * @param actual The actual integer value to check.
 */
#define EQUAL_INT(expected, actual)                                                         \
    do                                                                                      \
    {                                                                                       \
        int e = (expected);                                                                 \
        int a = (actual);                                                                   \
        if (e != a)                                                                         \
        {                                                                                   \
            char e_buf[32], a_buf[32];                                                      \
            snprintf(e_buf, 32, "%d", e);                                                   \
            snprintf(a_buf, 32, "%d", a);                                                   \
            _UT_record_failure(__FILE__, __LINE__, #expected " == " #actual, e_buf, a_buf); \
        }                                                                                   \
    } while (0)

/**
 * @brief Asserts that two character values are equal.
 *
 * If the characters are not equal, the test fails and records both.
 *
 * @param expected The expected char value.
 * @param actual The actual char value to check.
 */
#define EQUAL_CHAR(expected, actual)                                                        \
    do                                                                                      \
    {                                                                                       \
        char e = (expected);                                                                \
        char a = (actual);                                                                  \
        if (e != a)                                                                         \
        {                                                                                   \
            char e_buf[4] = {'\'', e, '\'', 0};                                             \
            char a_buf[4] = {'\'', a, '\'', 0};                                             \
            _UT_record_failure(__FILE__, __LINE__, #expected " == " #actual, e_buf, a_buf); \
        }                                                                                   \
    } while (0)

/**
 * @brief Asserts that two pointer values are equal.
 *
 * If the pointers are not equal, the test fails and records both pointer addresses.
 *
 * @param expected The expected pointer value.
 * @param actual The actual pointer value to check.
 */
#define EQUAL_POINTER(expected, actual)                                                     \
    do                                                                                      \
    {                                                                                       \
        const void *e = (const void *)(expected);                                           \
        const void *a = (const void *)(actual);                                             \
        if (e != a)                                                                         \
        {                                                                                   \
            char e_buf[32], a_buf[32];                                                      \
            snprintf(e_buf, 32, "%p", e);                                                   \
            snprintf(a_buf, 32, "%p", a);                                                   \
            _UT_record_failure(__FILE__, __LINE__, #expected " == " #actual, e_buf, a_buf); \
        }                                                                                   \
    } while (0)

/**
 * @brief Asserts that a pointer value is NULL.
 * @param actual The actual pointer value to check.
 */
#define EQUAL_NULL(actual) EQUAL_POINTER(NULL, (actual))

/**
 * @brief Asserts that a pointer value is not NULL.
 * @param actual The actual pointer value to check.
 */
#define NON_EQUAL_NULL(actual)                                                                      \
    do                                                                                              \
    {                                                                                               \
        const void *a = (const void *)(actual);                                                     \
        if (a == NULL)                                                                              \
        {                                                                                           \
            _UT_record_failure(__FILE__, __LINE__, #actual " != NULL", "non-NULL pointer", "NULL"); \
        }                                                                                           \
    } while (0)

/**
 * @brief Asserts that two C-style strings are equal.
 *
 * Uses strcmp for comparison. The test fails if the strings are different, or if either pointer is NULL.
 *
 * @param expected The expected string value.
 * @param actual The actual string value to check.
 */
#define EQUAL_STRING(expected, actual)                                                                        \
    do                                                                                                        \
    {                                                                                                         \
        const char *e = (expected);                                                                           \
        const char *a = (actual);                                                                             \
        if (!e || !a || strcmp(e, a) != 0)                                                                    \
        {                                                                                                     \
            _UT_record_failure(__FILE__, __LINE__, #expected " == " #actual, e ? e : "NULL", a ? a : "NULL"); \
        }                                                                                                     \
    } while (0)

/**
 * @brief Asserts that two custom data types are equal using a provided comparison function.
 * @param expected The expected value.
 * @param actual The actual value.
 * @param compare_fn A function pointer `int (*)(expected, actual)` that returns true if the items are equal.
 * @param print_fn A function pointer `void (*)(char* buf, size_t size, val)` that prints the value into a buffer.
 */
#define EQUAL_BY(expected, actual, compare_fn, print_fn)                                           \
    do                                                                                             \
    {                                                                                              \
        __typeof__(expected) _exp = (expected);                                                    \
        __typeof__(actual) _act = (actual);                                                        \
        if (!compare_fn(_exp, _act))                                                               \
        {                                                                                          \
            char _exp_str[1024] = {0}, _act_str[1024] = {0};                                       \
            char _cond_str[1024] = {0};                                                            \
            print_fn(_exp_str, sizeof(_exp_str), _exp);                                            \
            print_fn(_act_str, sizeof(_act_str), _act);                                            \
            snprintf(_cond_str, sizeof(_cond_str), "%s(%s, %s)", #compare_fn, #expected, #actual); \
            _UT_record_failure(__FILE__, __LINE__, _cond_str, _exp_str, _act_str);                 \
        }                                                                                          \
    } while (0)

/**
 * @brief Asserts that a value satisfies a given property (predicate).
 * @param value The value to test.
 * @param predicate_fn A function pointer `int (*)(value)` that returns true if the property holds.
 * @param print_fn A function pointer `void (*)(char* buf, size_t size, val)` that prints the value.
 * @param help_text A descriptive string explaining the property that was violated.
 */
#define PROPERTY(value, predicate_fn, print_fn, help_text)                                 \
    do                                                                                     \
    {                                                                                      \
        __typeof__(value) _val = (value);                                                  \
        if (!predicate_fn(_val))                                                           \
        {                                                                                  \
            char _val_str[1024] = {0};                                                     \
            char _cond_str[1024] = {0};                                                    \
            char _exp_str[1024] = {0};                                                     \
            print_fn(_val_str, sizeof(_val_str), _val);                                    \
            snprintf(_cond_str, sizeof(_cond_str), "%s(%s)", #predicate_fn, #value);       \
            snprintf(_exp_str, sizeof(_exp_str), "A value that satisfies: %s", help_text); \
            _UT_record_failure(__FILE__, __LINE__, _cond_str, _exp_str, _val_str);         \
        }                                                                                  \
    } while (0)

// Declarations for print helpers. Definitions are in the implementation block.
void _UT_print_int(char *buf, size_t size, int val);
void _UT_print_char(char *buf, size_t size, char val);
void _UT_print_string(char *buf, size_t size, const char *val);

#define PROPERTY_INT(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_int, help_text)
#define PROPERTY_CHAR(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_char, help_text)
#define PROPERTY_STRING(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _UT_print_string, help_text)

#ifdef UT_MEMORY_TRACKING_ENABLED
/**
 * @brief (Memory Tracking) Asserts that the total number of malloc/calloc/realloc calls matches the expected count.
 * @param expected The expected number of allocations.
 */
#define ASSERT_ALLOC_COUNT(expected) EQUAL_INT(expected, UT_alloc_count)

/**
 * @brief (Memory Tracking) Asserts that the total number of free calls matches the expected count.
 * @param expected The expected number of successful frees.
 */
#define ASSERT_FREE_COUNT(expected) EQUAL_INT(expected, UT_free_count)
#endif

#ifdef _WIN32
/**
 * @brief Defines a test case that is expected to fail with a standard C assertion.
 *        This test only checks that the process terminates with the correct exit code.
 * @param SuiteName The name of the test suite.
 * @param TestDescription A descriptive name for the test case.
 */
#define TEST_ASSERTION_FAILURE(SuiteName, TestDescription) TEST_DEATH_CASE(SuiteName, TestDescription, .expected_exit_code = _UT_ASSERT_EXIT_CODE, .expected_msg = NULL)
/**
 * @brief Defines a test case that is expected to fail with a standard C assertion and
 *        verifies that the assertion message in stderr contains a specific substring.
 * @param SuiteName The name of the test suite.
 * @param TestDescription A descriptive name for the test case.
 * @param expected_substring The substring to find within the stderr output.
 */
#define TEST_ASSERTION_FAILURE_WITH_MESSAGE(SuiteName, TestDescription, expected_substring) TEST_DEATH_CASE(SuiteName, TestDescription, .expected_exit_code = _UT_ASSERT_EXIT_CODE, .expected_msg = (expected_substring))
#else
/**
 * @brief Defines a test case that is expected to fail with a standard C assertion.
 *        This test only checks that the process is terminated by a SIGABRT signal.
 * @param SuiteName The name of the test suite.
 * @param TestDescription A descriptive name for the test case.
 */
#define TEST_ASSERTION_FAILURE(SuiteName, TestDescription) TEST_DEATH_CASE(SuiteName, TestDescription, .expected_signal = SIGABRT, .expected_msg = NULL)
/**
 * @brief Defines a test case that is expected to fail with a standard C assertion and
 *        verifies that the assertion message in stderr contains a specific substring.
 * @param SuiteName The name of the test suite.
 * @param TestDescription A descriptive name for the test case.
 * @param expected_substring The substring to find within the stderr output.
 */
#define TEST_ASSERTION_FAILURE_WITH_MESSAGE(SuiteName, TestDescription, expected_substring) TEST_DEATH_CASE(SuiteName, TestDescription, .expected_signal = SIGABRT, .expected_msg = (expected_substring))
#endif

#ifndef UT_DEFAULT_FLOAT_TOLERANCE
#define UT_DEFAULT_FLOAT_TOLERANCE 1e-5f
#endif
#ifndef UT_DEFAULT_DOUBLE_TOLERANCE
#define UT_DEFAULT_DOUBLE_TOLERANCE 1e-9
#endif

/**
 * @brief Asserts that two float values are within a given tolerance of each other.
 * @param expected The expected float value.
 * @param actual The actual float value.
 * @param tolerance The maximum allowed difference.
 */
#define NEAR_FLOAT(expected, actual, tolerance)                                                           \
    do                                                                                                    \
    {                                                                                                     \
        float e = (expected);                                                                             \
        float a = (actual);                                                                               \
        float t = (tolerance);                                                                            \
        if (fabsf(e - a) > t)                                                                             \
        {                                                                                                 \
            char e_buf[64], a_buf[64], cond_str[256];                                                     \
            snprintf(e_buf, sizeof(e_buf), "%f", e);                                                      \
            snprintf(a_buf, sizeof(a_buf), "%f (difference: %e)", a, fabsf(e - a));                       \
            snprintf(cond_str, sizeof(cond_str), "fabsf(%s - %s) <= %s", #expected, #actual, #tolerance); \
            _UT_record_failure(__FILE__, __LINE__, cond_str, e_buf, a_buf);                               \
        }                                                                                                 \
    } while (0)

/**
 * @brief Asserts that two double values are within a given tolerance of each other.
 * @param expected The expected double value.
 * @param actual The actual double value.
 * @param tolerance The maximum allowed difference.
 */
#define NEAR_DOUBLE(expected, actual, tolerance)                                                         \
    do                                                                                                   \
    {                                                                                                    \
        double e = (expected);                                                                           \
        double a = (actual);                                                                             \
        double t = (tolerance);                                                                          \
        if (fabs(e - a) > t)                                                                             \
        {                                                                                                \
            char e_buf[64], a_buf[64], cond_str[256];                                                    \
            snprintf(e_buf, sizeof(e_buf), "%lf", e);                                                    \
            snprintf(a_buf, sizeof(a_buf), "%lf (difference: %e)", a, fabs(e - a));                      \
            snprintf(cond_str, sizeof(cond_str), "fabs(%s - %s) <= %s", #expected, #actual, #tolerance); \
            _UT_record_failure(__FILE__, __LINE__, cond_str, e_buf, a_buf);                              \
        }                                                                                                \
    } while (0)

#define EQUAL_FLOAT(expected, actual) NEAR_FLOAT(expected, actual, UT_DEFAULT_FLOAT_TOLERANCE)
#define EQUAL_DOUBLE(expected, actual) NEAR_DOUBLE(expected, actual, UT_DEFAULT_DOUBLE_TOLERANCE)

#ifdef UT_MEMORY_TRACKING_ENABLED
/**
 * @brief (Memory Tracking) Asserts the exact number of allocations and frees that occur within a code block.
 * @param code_block The block of code to execute and monitor.
 * @param expected_allocs The exact number of new allocations expected within the block.
 * @param expected_frees The exact number of frees expected within the block.
 */
#define ASSERT_MEMORY_CHANGES(code_block, expected_allocs, expected_frees)                                               \
    do                                                                                                                   \
    {                                                                                                                    \
        int _allocs_before_ = UT_alloc_count;                                                                            \
        int _frees_before_ = UT_free_count;                                                                              \
        {                                                                                                                \
            code_block;                                                                                                  \
        }                                                                                                                \
        int _alloc_delta_ = UT_alloc_count - _allocs_before_;                                                            \
        int _free_delta_ = UT_free_count - _frees_before_;                                                               \
        if (_alloc_delta_ != (expected_allocs))                                                                          \
        {                                                                                                                \
            char _a_exp_buf_[32], _a_act_buf_[32];                                                                       \
            snprintf(_a_exp_buf_, 32, "%d", (int)(expected_allocs));                                                     \
            snprintf(_a_act_buf_, 32, "%d", _alloc_delta_);                                                              \
            _UT_record_failure(__FILE__, __LINE__, "Allocation count mismatch in code block", _a_exp_buf_, _a_act_buf_); \
        }                                                                                                                \
        if (_free_delta_ != (expected_frees))                                                                            \
        {                                                                                                                \
            char _f_exp_buf_[32], _f_act_buf_[32];                                                                       \
            snprintf(_f_exp_buf_, 32, "%d", (int)(expected_frees));                                                      \
            snprintf(_f_act_buf_, 32, "%d", _free_delta_);                                                               \
            _UT_record_failure(__FILE__, __LINE__, "Free count mismatch in code block", _f_exp_buf_, _f_act_buf_);       \
        }                                                                                                                \
    } while (0)

/**
 * @brief (Memory Tracking) Asserts memory changes and marks new allocations as baseline.
 * @param code_block The block of code to execute and monitor.
 * @param expected_allocs The exact number of new allocations expected within the block.
 * @param expected_frees The exact number of frees expected within the block.
 */
#define ASSERT_AND_MARK_MEMORY_CHANGES(code_block, expected_allocs, expected_frees)                                      \
    do                                                                                                                   \
    {                                                                                                                    \
        int _allocs_before_ = UT_alloc_count;                                                                            \
        int _frees_before_ = UT_free_count;                                                                              \
        {                                                                                                                \
            code_block;                                                                                                  \
        }                                                                                                                \
        int _alloc_delta_ = UT_alloc_count - _allocs_before_;                                                            \
        int _free_delta_ = UT_free_count - _frees_before_;                                                               \
        if (_alloc_delta_ != (expected_allocs))                                                                          \
        {                                                                                                                \
            char _a_exp_buf_[32], _a_act_buf_[32];                                                                       \
            snprintf(_a_exp_buf_, 32, "%d", (int)(expected_allocs));                                                     \
            snprintf(_a_act_buf_, 32, "%d", _alloc_delta_);                                                              \
            _UT_record_failure(__FILE__, __LINE__, "Allocation count mismatch in code block", _a_exp_buf_, _a_act_buf_); \
        }                                                                                                                \
        if (_free_delta_ != (expected_frees))                                                                            \
        {                                                                                                                \
            char _f_exp_buf_[32], _f_act_buf_[32];                                                                       \
            snprintf(_f_exp_buf_, 32, "%d", (int)(expected_frees));                                                      \
            snprintf(_f_act_buf_, 32, "%d", _free_delta_);                                                               \
            _UT_record_failure(__FILE__, __LINE__, "Free count mismatch in code block", _f_exp_buf_, _f_act_buf_);       \
        }                                                                                                                \
        _UT_MemInfo *current = _UT_mem_head;                                                                             \
        for (int i = 0; i < _alloc_delta_ && current != NULL; ++i)                                                       \
        {                                                                                                                \
            if (current->is_baseline == 0)                                                                               \
            {                                                                                                            \
                current->is_baseline = 1;                                                                                \
            }                                                                                                            \
            current = current->next;                                                                                     \
        }                                                                                                                \
    } while (0)

/**
 * @brief (Memory Tracking) Asserts memory changes and marks new allocations as baseline, including byte counts.
 *
 * This macro asserts the exact number of allocations and frees, as well as the total number of bytes
 * allocated (via malloc/calloc) and freed that occur within a given code block. After the assertions,
 * it marks any new memory allocations as 'baseline' so they are ignored by subsequent leak checks.
 * Note: Resizing memory with `realloc` updates an existing allocation's size but is not counted as a new
 * allocation or a free in this macro's accounting.
 *
 * @param code_block The block of code to execute and monitor.
 * @param expected_allocs The exact number of new allocations expected within the block.
 * @param expected_frees The exact number of frees expected within the block.
 * @param expected_bytes_allocd The total number of bytes expected to be allocated.
 * @param expected_bytes_freed The total number of bytes expected to be freed.
 */
#define ASSERT_AND_MARK_MEMORY_CHANGES_BYTES(code_block, expected_allocs, expected_frees, expected_bytes_allocd, expected_bytes_freed) \
    do                                                                                                                                 \
    {                                                                                                                                  \
        /* Store memory state before executing the code block */                                                                       \
        int _allocs_before_ = UT_alloc_count;                                                                                          \
        int _frees_before_ = UT_free_count;                                                                                            \
        size_t _bytes_allocd_before_ = g_UT_total_bytes_allocated; /* Assumes g_UT_total_bytes_allocated exists */                     \
        size_t _bytes_freed_before_ = g_UT_total_bytes_freed;      /* Assumes g_UT_total_bytes_freed exists */                         \
                                                                                                                                       \
        {                                                                                                                              \
            code_block;                                                                                                                \
        }                                                                                                                              \
                                                                                                                                       \
        /* Calculate the change (delta) in memory state */                                                                             \
        int _alloc_delta_ = UT_alloc_count - _allocs_before_;                                                                          \
        int _free_delta_ = UT_free_count - _frees_before_;                                                                             \
        size_t _bytes_allocd_delta_ = g_UT_total_bytes_allocated - _bytes_allocd_before_;                                              \
        size_t _bytes_freed_delta_ = g_UT_total_bytes_freed - _bytes_freed_before_;                                                    \
                                                                                                                                       \
        /* Assert the change in the number of allocations */                                                                           \
        if (_alloc_delta_ != (expected_allocs))                                                                                        \
        {                                                                                                                              \
            char _a_exp_buf_[32], _a_act_buf_[32];                                                                                     \
            snprintf(_a_exp_buf_, 32, "%d", (int)(expected_allocs));                                                                   \
            snprintf(_a_act_buf_, 32, "%d", _alloc_delta_);                                                                            \
            _UT_record_failure(__FILE__, __LINE__, "Allocation count mismatch in code block", _a_exp_buf_, _a_act_buf_);               \
        }                                                                                                                              \
        /* Assert the change in the number of frees */                                                                                 \
        if (_free_delta_ != (expected_frees))                                                                                          \
        {                                                                                                                              \
            char _f_exp_buf_[32], _f_act_buf_[32];                                                                                     \
            snprintf(_f_exp_buf_, 32, "%d", (int)(expected_frees));                                                                    \
            snprintf(_f_act_buf_, 32, "%d", _free_delta_);                                                                             \
            _UT_record_failure(__FILE__, __LINE__, "Free count mismatch in code block", _f_exp_buf_, _f_act_buf_);                     \
        }                                                                                                                              \
        /* Assert the change in the number of bytes allocated */                                                                       \
        if (_bytes_allocd_delta_ != (expected_bytes_allocd))                                                                           \
        {                                                                                                                              \
            char _ba_exp_buf_[48], _ba_act_buf_[48];                                                                                   \
            snprintf(_ba_exp_buf_, 48, "%zu bytes", (size_t)(expected_bytes_allocd));                                                  \
            snprintf(_ba_act_buf_, 48, "%zu bytes", _bytes_allocd_delta_);                                                             \
            _UT_record_failure(__FILE__, __LINE__, "Bytes allocated mismatch in code block", _ba_exp_buf_, _ba_act_buf_);              \
        }                                                                                                                              \
        /* Assert the change in the number of bytes freed */                                                                           \
        if (_bytes_freed_delta_ != (expected_bytes_freed))                                                                             \
        {                                                                                                                              \
            char _bf_exp_buf_[48], _bf_act_buf_[48];                                                                                   \
            snprintf(_bf_exp_buf_, 48, "%zu bytes", (size_t)(expected_bytes_freed));                                                   \
            snprintf(_bf_act_buf_, 48, "%zu bytes", _bytes_freed_delta_);                                                              \
            _UT_record_failure(__FILE__, __LINE__, "Bytes freed mismatch in code block", _bf_exp_buf_, _bf_act_buf_);                  \
        }                                                                                                                              \
                                                                                                                                       \
        /* Mark all new allocations within the block as baseline for future leak checks */                                             \
        _UT_MemInfo *current = _UT_mem_head;                                                                                           \
        for (int i = 0; i < _alloc_delta_ && current != NULL; ++i)                                                                     \
        {                                                                                                                              \
            if (current->is_baseline == 0)                                                                                             \
            {                                                                                                                          \
                current->is_baseline = 1;                                                                                              \
            }                                                                                                                          \
            current = current->next;                                                                                                   \
        }                                                                                                                              \
    } while (0)
#endif // UT_MEMORY_TRACKING_ENABLED

#endif // UNIT_TEST_DECLARATION

#ifdef UNIT_TEST_IMPLEMENTATION
// Definitions for print helpers. Not static.
void _UT_print_int(char *buf, size_t size, int val) { snprintf(buf, size, "%d", val); }
void _UT_print_char(char *buf, size_t size, char val) { snprintf(buf, size, "'%c'", val); }
void _UT_print_string(char *buf, size_t size, const char *val) { snprintf(buf, size, "\"%s\"", val); }

static int _ut_min3(int a, int b, int c)
{
    if (a < b && a < c)
        return a;
    if (b < a && b < c)
        return b;
    return c;
}

/**
 * @brief (Auxiliar function) Calculates the Levenshtein distance between two strings,
 * ignoring case differences. This helper can remain static as it's only used below.
 * This is a space-optimized implementation.
 */
static int _ut_levenshtein_distance(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return 9999;
    int s1len = (int)strlen(s1);
    int s2len = (int)strlen(s2);
    int *v0 = (int *)calloc(s2len + 1, sizeof(int));
    int *v1 = (int *)calloc(s2len + 1, sizeof(int));
    for (int i = 0; i <= s2len; i++)
    {
        v0[i] = i;
    }
    for (int i = 0; i < s1len; i++)
    {
        v1[0] = i + 1;
        for (int j = 0; j < s2len; j++)
        {
            int cost = (tolower((unsigned char)s1[i]) == tolower((unsigned char)s2[j])) ? 0 : 1;
            v1[j + 1] = _ut_min3(v1[j] + 1, v0[j + 1] + 1, v0[j] + cost);
        }
        for (int j = 0; j <= s2len; j++)
        {
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
 * This function's definition is not static, so it can be linked by test files.
 */
float _ut_calculate_similarity_ratio(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return 0.0f;
    int s1len = (int)strlen(s1);
    int s2len = (int)strlen(s2);
    if (s1len == 0 && s2len == 0)
        return 1.0f;
    int max_len = (s1len > s2len) ? s1len : s2len;
    if (max_len == 0)
        return 1.0f; // Both empty
    int distance = _ut_levenshtein_distance(s1, s2);
    return 1.0f - ((float)distance / max_len);
}

// Internal helper function to record a failure. Not static.
void _UT_record_failure(const char *file, int line, const char *cond_str, const char *exp_str, const char *act_str)
{
    if (g_UT_current_test_result)
    {
#ifdef UT_MEMORY_TRACKING_ENABLED
        UT_disable_memory_tracking(); // Disable tracking for internal allocations
#endif
        _UT_AssertionFailure *failure = (_UT_AssertionFailure *)calloc(1, sizeof(_UT_AssertionFailure));
        if (failure)
        {
            failure->file = _UT_strdup(file);
            failure->line = line;
            failure->condition_str = _UT_strdup(cond_str);
            failure->expected_str = _UT_strdup(exp_str);
            failure->actual_str = _UT_strdup(act_str);
            // Prepend to the list
            failure->next = g_UT_current_test_result->failures;
            g_UT_current_test_result->failures = failure;
        }
#ifdef UT_MEMORY_TRACKING_ENABLED
        UT_enable_memory_tracking(); // Re-enable tracking
#endif
    }
}
#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 6: STDOUT CAPTURE AND ASSERTIONS                                   */
/* (Included for DECLARATION and IMPLEMENTATION)                              */
/*============================================================================*/
#if defined(UNIT_TEST_DECLARATION)
#define _STDOUT_CAPTURE_BUFFER_SIZE 8192
static char _UT_stdout_capture_buffer[_STDOUT_CAPTURE_BUFFER_SIZE];

// Declaration for the similarity function, defined in the implementation block.
float _ut_calculate_similarity_ratio(const char *s1, const char *s2);

#define ASSERT_STDOUT_EQUAL(code_block, expected_str)                                                       \
    do                                                                                                      \
    {                                                                                                       \
        _UT_start_capture_stdout();                                                                         \
        {                                                                                                   \
            code_block;                                                                                     \
        }                                                                                                   \
        _UT_stop_capture_stdout(_UT_stdout_capture_buffer, sizeof(_UT_stdout_capture_buffer));              \
        const char *e = (expected_str);                                                                     \
        if (!e || strcmp(e, _UT_stdout_capture_buffer) != 0)                                                \
        {                                                                                                   \
            char cond_str[256];                                                                             \
            snprintf(cond_str, sizeof(cond_str), _UT_TAG_STDOUT "output of '%s' equals '%s'", #code_block, #expected_str); \
            _UT_record_failure(__FILE__, __LINE__, cond_str, e, _UT_stdout_capture_buffer);                 \
        }                                                                                                   \
    } while (0)

#define ASSERT_STDOUT_EQUIVALENT(code_block, expected_str)                                                                      \
    do                                                                                                                          \
    {                                                                                                                           \
        _UT_start_capture_stdout();                                                                                             \
        {                                                                                                                       \
            code_block;                                                                                                         \
        }                                                                                                                       \
        _UT_stop_capture_stdout(_UT_stdout_capture_buffer, sizeof(_UT_stdout_capture_buffer));                                  \
        char expected_normalized[_STDOUT_CAPTURE_BUFFER_SIZE];                                                                  \
        strncpy(expected_normalized, expected_str, sizeof(expected_normalized) - 1);                                            \
        expected_normalized[sizeof(expected_normalized) - 1] = '\0';                                                            \
        char actual_normalized[_STDOUT_CAPTURE_BUFFER_SIZE];                                                                    \
        strncpy(actual_normalized, _UT_stdout_capture_buffer, sizeof(actual_normalized) - 1);                                   \
        actual_normalized[sizeof(actual_normalized) - 1] = '\0';                                                                \
        _UT_normalize_string_for_comparison(expected_normalized);                                                               \
        _UT_normalize_string_for_comparison(actual_normalized);                                                                 \
        if (strcmp(expected_normalized, actual_normalized) != 0)                                                                \
        {                                                                                                                       \
            char condition_str[256];                                                                                            \
            snprintf(condition_str, sizeof(condition_str), _UT_TAG_STDOUT "output of '%s' is equivalent to '%s'", #code_block, #expected_str); \
            _UT_record_failure(__FILE__, __LINE__, condition_str, expected_str, _UT_stdout_capture_buffer);                     \
        }                                                                                                                       \
    } while (0)

#define ASSERT_STDOUT_SIMILAR(code_block, expected_str, min_similarity)                                                                                 \
    do                                                                                                                                                  \
    {                                                                                                                                                   \
        _UT_start_capture_stdout();                                                                                                                     \
        {                                                                                                                                               \
            code_block;                                                                                                                                 \
        }                                                                                                                                               \
        _UT_stop_capture_stdout(_UT_stdout_capture_buffer, sizeof(_UT_stdout_capture_buffer));                                                          \
        const char *e = (expected_str);                                                                                                                 \
        float actual_similarity = _ut_calculate_similarity_ratio(e, _UT_stdout_capture_buffer);                                                         \
        if (actual_similarity < (min_similarity))                                                                                                       \
        {                                                                                                                                               \
            char expected_buf[256], actual_buf[sizeof(_UT_stdout_capture_buffer) + 128], condition_str[256];                                            \
            snprintf(expected_buf, sizeof(expected_buf), "A string with at least %.2f%% similarity to \"%s\"", (min_similarity) * 100.0f, e);           \
            snprintf(actual_buf, sizeof(actual_buf), "A string with %.2f%% similarity: \"%s\"", actual_similarity * 100.0f, _UT_stdout_capture_buffer); \
            snprintf(condition_str, sizeof(condition_str), _UT_TAG_STDOUT "similarity(output_of(%s), \"%s\") >= %.2f", #code_block, #expected_str, (min_similarity));  \
            _UT_record_failure(__FILE__, __LINE__, condition_str, expected_buf, actual_buf);                                                            \
        }                                                                                                                                               \
    } while (0)

#endif // UNIT_TEST_DECLARATION

#ifdef UNIT_TEST_IMPLEMENTATION

#ifdef _WIN32
static int _UT_stdout_original_fd = -1;
static HANDLE _UT_stdout_pipe_read = NULL;
static HANDLE _UT_stdout_pipe_write = NULL;

void _UT_start_capture_stdout(void)
{
    fflush(stdout);

    // 1. Save a duplicate of the original C-level file descriptor for stdout.
    _UT_stdout_original_fd = _dup(_fileno(stdout));

    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&_UT_stdout_pipe_read, &_UT_stdout_pipe_write, &sa, 0))
    {
        return; // Pipe creation failed
    }

    // 2. Create a new C-level file descriptor from the pipe's write handle.
    //    IMPORTANT: Use _O_BINARY to prevent Windows from changing '\n' to '\r\n'.
    int pipe_write_fd = _open_osfhandle((intptr_t)_UT_stdout_pipe_write, _O_WRONLY | _O_BINARY);
    if (pipe_write_fd == -1)
    {
        // Cleanup on failure
        CloseHandle(_UT_stdout_pipe_read);
        CloseHandle(_UT_stdout_pipe_write);
        return;
    }

    // 3. Redirect stdout to the new pipe. This is the key step.
    _dup2(pipe_write_fd, _fileno(stdout));

    // 4. The handle returned by _open_osfhandle is now redundant because _dup2
    //    made a copy, so we must close it.
    _close(pipe_write_fd);

    // 5. Force the newly redirected stdout to be unbuffered. This prevents
    //    output (especially the final newline) from getting stuck in a buffer.
    setvbuf(stdout, NULL, _IONBF, 0);
}

void _UT_stop_capture_stdout(char *buffer, size_t size)
{
    fflush(stdout);

    // 1. Restore the original stdout file descriptor. This automatically closes
    //    the C runtime's connection to our pipe's write handle, signaling EOF
    //    to the reading end.
    if (_UT_stdout_original_fd != -1)
    {
        _dup2(_UT_stdout_original_fd, _fileno(stdout));
        _close(_UT_stdout_original_fd);
        _UT_stdout_original_fd = -1;
    }

    // 2. Read all output from the pipe. This is now a safe blocking read
    //    because the write end is guaranteed to be closed.
    DWORD bytes_read = 0;
    if (_UT_stdout_pipe_read != NULL)
    {
        ReadFile(_UT_stdout_pipe_read, buffer, (DWORD)size - 1, &bytes_read, NULL);
        buffer[bytes_read] = '\0';
    }
    else
    {
        buffer[0] = '\0';
    }

    // 3. Clean up the WinAPI pipe handles.
    if (_UT_stdout_pipe_read)
        CloseHandle(_UT_stdout_pipe_read);
    if (_UT_stdout_pipe_write)
        CloseHandle(_UT_stdout_pipe_write);

    _UT_stdout_pipe_read = NULL;
    _UT_stdout_pipe_write = NULL;
}
#else
static int _UT_stdout_pipe[2] = {-1, -1}, _UT_stdout_original_fd = -1;
void _UT_start_capture_stdout(void)
{
    fflush(stdout);
    _UT_stdout_original_fd = dup(STDOUT_FILENO);
    if (pipe(_UT_stdout_pipe) == -1)
        return;
    dup2(_UT_stdout_pipe[1], STDOUT_FILENO);
    close(_UT_stdout_pipe[1]);
}
void _UT_stop_capture_stdout(char *buffer, size_t size)
{
    fflush(stdout);
    dup2(_UT_stdout_original_fd, STDOUT_FILENO);
    close(_UT_stdout_original_fd);
    ssize_t bytes_read = 0, total_bytes = 0;
    int flags = fcntl(_UT_stdout_pipe[0], F_GETFL, 0);
    fcntl(_UT_stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);
    while ((bytes_read = read(_UT_stdout_pipe[0], buffer + total_bytes, size - 1 - total_bytes)) > 0)
    {
        total_bytes += bytes_read;
    }
    buffer[total_bytes] = '\0';
    close(_UT_stdout_pipe[0]);
}
#endif

// Internal helper to normalize a string: trims whitespace and collapses internal whitespace.
void _UT_normalize_string_for_comparison(char *str)
{
    if (!str)
        return;
    char *read_ptr = str, *write_ptr = str;
    int in_whitespace = 1;
    while (*read_ptr)
    {
        if (isspace((unsigned char)*read_ptr))
        {
            if (!in_whitespace)
            {
                *write_ptr++ = ' ';
                in_whitespace = 1;
            }
        }
        else
        {
            *write_ptr++ = *read_ptr;
            in_whitespace = 0;
        }
        read_ptr++;
    }
    if (write_ptr > str && *(write_ptr - 1) == ' ')
    {
        write_ptr--;
    }
    *write_ptr = '\0';
}
#endif // UNIT_TEST_IMPLEMENTATION

/*============================================================================*/
/* SECTION 7, 8, 9: FULL TEST RUNNER IMPLEMENTATION                           */
/* (Only included when UNIT_TEST_IMPLEMENTATION is defined)                   */
/*============================================================================*/
#ifdef UNIT_TEST_IMPLEMENTATION

// Define internal constants for serialization, CLI args, etc.
#define _UT_SERIALIZATION_MARKER '\x1F'
#define _UT_KEY_STATUS "status="
#define _UT_KEY_STATUS_LEN (sizeof(_UT_KEY_STATUS) - 1)
#define _UT_KEY_FAILURE "failure="
#define _UT_KEY_FAILURE_LEN (sizeof(_UT_KEY_FAILURE) - 1)
#define _UT_ARG_RUN_TEST "--run_test"
#define _UT_ARG_SUITE_FILTER "--suite="
#define _UT_ARG_SUITE_FILTER_LEN (sizeof(_UT_ARG_SUITE_FILTER) - 1)
#define _UT_TAG_STDOUT "[STDOUT]"
#define _UT_TAG_STDOUT_LEN (sizeof(_UT_TAG_STDOUT) - 1)
#define _UT_ASSERT_EXIT_CODE 64353

/*============================================================================*/
/* SECTION 7: THE UT_RUN_ALL_TESTS IMPLEMENTATION (FULL VERSION)              */
/*============================================================================*/

// --- Helper functions for serializing/deserializing test results ---
// These are simple text-based functions for IPC between parent and child.
static void _UT_serialize_result(FILE *stream, _UT_TestResult *result)
{
    fprintf(stream, _UT_KEY_STATUS "%d%c", result->status, _UT_SERIALIZATION_MARKER);
    _UT_AssertionFailure *f = result->failures;
    while (f)
    {
        // A simple serialization format: key=value pairs, failures are pipe-delimited
        fprintf(stream, _UT_KEY_FAILURE "%s|%d|%s|%s|%s%c", f->file, f->line, f->condition_str, f->expected_str ? f->expected_str : "", f->actual_str ? f->actual_str : "", _UT_SERIALIZATION_MARKER);
        f = f->next;
    }
    fprintf(stream, "end_of_data%c", _UT_SERIALIZATION_MARKER);
}

static _UT_TestResult *_UT_deserialize_result(const char *buffer, _UT_TestInfo *test_info)
{
    UT_disable_memory_tracking();
    _UT_TestResult *result = (_UT_TestResult *)calloc(1, sizeof(_UT_TestResult));
    result->suite_name = test_info->suite_name;
    result->test_name = test_info->test_name;

    char line[4096]; // Increased line buffer size
    const char *p = buffer;
    while (p && *p)
    {
        const char *next_p = strchr(p, _UT_SERIALIZATION_MARKER);
        size_t len = next_p ? (size_t)(next_p - p) : strlen(p);
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        strncpy(line, p, len);
        line[len] = '\0';
        p = next_p ? next_p + 1 : NULL;

        if (strncmp(line, _UT_KEY_STATUS, _UT_KEY_STATUS_LEN) == 0)
        {
            result->status = (_UT_TestStatus)atoi(line + _UT_KEY_STATUS_LEN);
        }
        else if (strncmp(line, _UT_KEY_FAILURE, _UT_KEY_FAILURE_LEN) == 0)
        {
            _UT_AssertionFailure *f = (_UT_AssertionFailure *)calloc(1, sizeof(_UT_AssertionFailure));
            _UT_AssertionFailure *tail = NULL; // To keep original failure order
            char *part = line + _UT_KEY_FAILURE_LEN;
            char *next_part = strchr(part, '|');
            if (next_part)
            {
                *next_part = '\0';
                f->file = _UT_strdup(part);
                part = next_part + 1;
            }
            next_part = strchr(part, '|');
            if (next_part)
            {
                *next_part = '\0';
                f->line = atoi(part);
                part = next_part + 1;
            }
            next_part = strchr(part, '|');
            if (next_part)
            {
                *next_part = '\0';
                f->condition_str = _UT_strdup(part);
                part = next_part + 1;
            }
            next_part = strchr(part, '|');
            if (next_part)
            {
                *next_part = '\0';
                f->expected_str = _UT_strdup(part);
                part = next_part + 1;
            }
            f->actual_str = _UT_strdup(part);

            // Append to list to preserve order
            if (!result->failures)
            {
                result->failures = f;
            }
            else
            {
                tail = result->failures;
                while (tail->next)
                    tail = tail->next;
                tail->next = f;
            }
        }
    }
    UT_enable_memory_tracking();
    return result;
}

// --- Memory management for the result data model ---
static void _UT_free_test_result(_UT_TestResult *tr)
{
    if (!tr)
        return;
    UT_disable_memory_tracking();
    _UT_AssertionFailure *f = tr->failures;
    while (f)
    {
        _UT_AssertionFailure *next_f = f->next;
        free(f->file);
        free(f->condition_str);
        free(f->expected_str);
        free(f->actual_str);
        free(f);
        f = next_f;
    }
    free(tr->captured_output);
    free(tr);
    UT_enable_memory_tracking();
}

// Internal function to run all tests. Use the UT_RUN_ALL_TESTS() macro in your main function.
int _UT_RUN_ALL_TESTS_impl(int argc, char *argv[]);

#define UT_RUN_ALL_TESTS() _UT_RUN_ALL_TESTS_impl(argc, argv)

#ifdef _WIN32
// ============================================================================
// PROCESS RUNNER FOR WINDOWS
// ============================================================================
static _UT_TestResult *_UT_run_process_win(_UT_TestInfo *test, const char *executable_path)
{
    char command_line[2048];
    snprintf(command_line, sizeof(command_line), "\"%s\" %s \"%s\" \\\"%s\\\"", executable_path, _UT_ARG_RUN_TEST, test->suite_name, test->test_name);
    HANDLE h_read = NULL, h_write = NULL;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    if (!CreatePipe(&h_read, &h_write, &sa, 0))
    { /* Handle error */
        return NULL;
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = h_write;
    si.hStdOutput = h_write;
    si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(h_read);
        CloseHandle(h_write);
        return NULL;
    }
    CloseHandle(h_write);

    char output_buffer[_UT_SERIALIZATION_BUFFER_SIZE] = {0};
    UT_disable_memory_tracking();
    _UT_TestResult *result = (_UT_TestResult *)calloc(1, sizeof(_UT_TestResult));
    UT_enable_memory_tracking();
    result->suite_name = test->suite_name;
    result->test_name = test->test_name;

    DWORD wait_result = WaitForSingleObject(pi.hProcess, UT_TEST_TIMEOUT_SECONDS * 1000);
    if (wait_result == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        result->status = _UT_STATUS_TIMEOUT;
        result->captured_output = _UT_strdup("Test exceeded timeout.");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(h_read);
        return result;
    }

    DWORD bytes_read = 0;
    ReadFile(h_read, output_buffer, sizeof(output_buffer) - 1, &bytes_read, NULL);
    output_buffer[bytes_read] = '\0';
    result->captured_output = _UT_strdup(output_buffer);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(h_read);

    const _UT_DeathExpect *de = test->death_expect;
    if (de)
    {
        int exit_ok = 1, msg_ok = 1;
        if (de->expected_exit_code != -1 && exit_code != (DWORD)de->expected_exit_code)
            exit_ok = 0;
        if (de->expected_msg && _ut_calculate_similarity_ratio(de->expected_msg, output_buffer) < de->min_similarity)
            msg_ok = 0;

        if (exit_code != 0 && exit_ok && msg_ok)
        {
            result->status = _UT_STATUS_DEATH_TEST_PASSED;
        }
        else
        {
            result->status = _UT_STATUS_FAILED;
        }
        return result;
    }

    if (exit_code >= 120 && exit_code <= 122)
    { // Our custom fatal error codes
        result->status = _UT_STATUS_CRASHED;
        return result;
    }

    // If we got here, it was a normal test. We free the preliminary result and deserialize the real one.
    _UT_free_test_result(result);
    _UT_TestResult *final_result = _UT_deserialize_result(output_buffer, test);
    UT_disable_memory_tracking();
    final_result->captured_output = _UT_strdup(output_buffer); // Store raw output for debugging
    UT_enable_memory_tracking();
    return final_result;
}

#else // POSIX
// ============================================================================
// REFACTORED PROCESS RUNNER FOR POSIX
// ============================================================================
static int _UT_wait_with_timeout(pid_t pid, int *status, int timeout_sec)
{
    struct timespec start, now, sleep_ts = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;)
    {
        pid_t r = waitpid(pid, status, WNOHANG);
        if (r == pid)
            return 0;
        if (r == -1 && errno != EINTR)
            return -1;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((now.tv_sec - start.tv_sec) >= timeout_sec)
        {
            kill(pid, SIGKILL);
            while (waitpid(pid, status, 0) == -1 && errno == EINTR)
                ;
            return 1;
        }
        nanosleep(&sleep_ts, NULL);
    }
}

static _UT_TestResult *_UT_run_process_posix(_UT_TestInfo *test, const char *executable_path)
{
    int out_pipe[2];
    if (pipe(out_pipe) == -1)
        return NULL;
    pid_t pid = fork();
    if (pid == -1)
    {
        close(out_pipe[0]);
        close(out_pipe[1]);
        return NULL;
    }

    if (pid == 0)
    { // Child
        close(out_pipe[0]);
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(out_pipe[1], STDERR_FILENO);
        close(out_pipe[1]);
        char *child_argv[] = {(char *)executable_path, _UT_ARG_RUN_TEST, (char *)test->suite_name, (char *)test->test_name, NULL};
        execv(executable_path, child_argv);
        perror("execv failed");
        exit(127);
    }
    else
    { // Parent
        close(out_pipe[1]);
        char output_buffer[_UT_SERIALIZATION_BUFFER_SIZE] = {0};
        int status;

        UT_disable_memory_tracking();
        _UT_TestResult *result = (_UT_TestResult *)calloc(1, sizeof(_UT_TestResult));
        UT_enable_memory_tracking();
        result->suite_name = test->suite_name;
        result->test_name = test->test_name;

        int r = _UT_wait_with_timeout(pid, &status, UT_TEST_TIMEOUT_SECONDS);
        ssize_t bytes_read = read(out_pipe[0], output_buffer, sizeof(output_buffer) - 1);
        if (bytes_read > 0)
            output_buffer[bytes_read] = '\0';
        close(out_pipe[0]);

        UT_disable_memory_tracking();
        result->captured_output = _UT_strdup(output_buffer);
        UT_enable_memory_tracking();

        if (r == 1)
        { // Timeout
            result->status = _UT_STATUS_TIMEOUT;
            return result;
        }

        const _UT_DeathExpect *de = test->death_expect;
        if (de)
        {
            int sig_ok = 1, exit_ok = 1, msg_ok = 1;
            if (WIFSIGNALED(status))
            {
                if (de->expected_signal == 0 || WTERMSIG(status) != de->expected_signal)
                    sig_ok = 0;
            }
            else if (WIFEXITED(status))
            {
                if (de->expected_exit_code == -1 || WEXITSTATUS(status) != de->expected_exit_code)
                    exit_ok = 0;
            }
            else
            {
                sig_ok = exit_ok = 0;
            }
            if (de->expected_msg && _ut_calculate_similarity_ratio(de->expected_msg, output_buffer) < de->min_similarity)
                msg_ok = 0;

            if ((WIFSIGNALED(status) || WIFEXITED(status)) && sig_ok && exit_ok && msg_ok)
            {
                result->status = _UT_STATUS_DEATH_TEST_PASSED;
            }
            else
            {
                result->status = _UT_STATUS_FAILED;
            }
            return result;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) >= 120 && WEXITSTATUS(status) <= 122)
        {
            result->status = _UT_STATUS_CRASHED; // Our custom fatal error codes
            return result;
        }
        else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            result->status = _UT_STATUS_CRASHED; // Generic crash/signal
            return result;
        }

        _UT_free_test_result(result);
        _UT_TestResult *final_result = _UT_deserialize_result(output_buffer, test);
        UT_disable_memory_tracking();
        final_result->captured_output = _UT_strdup(output_buffer);
        UT_enable_memory_tracking();
        return final_result;
    }
}
#endif

/*============================================================================*/
/* SECTION 8: REPORTER INTERFACE AND IMPLEMENTATIONS                          */
/*============================================================================*/

typedef struct
{
    void (*on_run_start)(const _UT_TestRun *run);
    void (*on_suite_start)(const _UT_SuiteResult *suite);
    void (*on_test_finish)(const _UT_TestResult *test);
    void (*on_suite_finish)(const _UT_SuiteResult *suite);
    void (*on_run_finish)(const _UT_TestRun *run, _UT_SuiteResult **all_suites, int suite_count);
} _UT_Reporter;

// --- CONSOLE REPORTER IMPLEMENTATION ---

/**
 * @brief Prints a string to a stream, enclosing it in quotes and escaping
 *        control characters like \n, \r, \t, etc.
 */
static void _UT_print_escaped_string(FILE *stream, const char *str)
{
    if (str == NULL)
    {
        fprintf(stream, "NULL");
        return;
    }

    fputc('"', stream);
    for (const char *p = str; *p != '\0'; p++)
    {
        switch (*p)
        {
        case '\n':
            fprintf(stream, "\\n");
            break;
        case '\r':
#ifndef _WIN32
            fprintf(stream, "\\r");
#endif
            break;
        case '\t':
            fprintf(stream, "\\t");
            break;
        case '\\':
            fprintf(stream, "\\\\");
            break;
        case '"':
            fprintf(stream, "\\\"");
            break;
        default:
            // For other non-printable characters, print their hex value
            if (isprint((unsigned char)*p))
            {
                fputc(*p, stream);
            }
            else
            {
                fprintf(stream, "\\x%02x", (unsigned char)*p);
            }
            break;
        }
    }
    fputc('"', stream);
}

static void _UT_console_on_suite_start(const _UT_SuiteResult *suite)
{
    printf("%sTests for %s%s\n", KBLU, suite->name, KNRM);
    printf("%s", KBLU);
    for (int i = 0; i < (int)(strlen(suite->name) + 10); ++i)
        printf("=");
    printf("%s", KNRM);
}

static void _UT_console_on_test_finish(const _UT_TestResult *test)
{
    switch (test->status)
    {
    case _UT_STATUS_PASSED:
        printf("\n   %sPASSED%s (%.2f ms)\n", KGRN, KNRM, test->duration_ms);
        break;
    case _UT_STATUS_DEATH_TEST_PASSED:
        printf("\n   %sPASSED (death test)%s (%.2f ms)\n", KGRN, KNRM, test->duration_ms);
        break;
    case _UT_STATUS_FAILED:
        printf("\n   %sFAILED%s (%.2f ms)\n", KRED, KNRM, test->duration_ms);
        _UT_AssertionFailure *f = test->failures;
        while (f)
        {
            // Check if this is a tagged stdout assertion
            if (strncmp(f->condition_str, _UT_TAG_STDOUT, _UT_TAG_STDOUT_LEN) == 0)
            {
                // -- PATH 1: STDOUT assertion with escaped printing --
                // Print condition string, skipping the tag for cleaner output
                fprintf(stderr, "   Assertion failed: %s\n      At: %s:%d\n", f->condition_str + _UT_TAG_STDOUT_LEN, f->file, f->line);

                if (f->expected_str)
                {
                    fprintf(stderr, "   Expected: %s", KGRN);
                    _UT_print_escaped_string(stderr, f->expected_str);
                    fprintf(stderr, "%s\n", KNRM);
                }
                if (f->actual_str)
                {
                    fprintf(stderr, "   Got: %s", KRED);
                    _UT_print_escaped_string(stderr, f->actual_str);
                    fprintf(stderr, "%s\n", KNRM);
                }
            }
            else
            {
                // -- PATH 2: All other assertions with standard printing --
                fprintf(stderr, "   Assertion failed: %s\n      At: %s:%d\n", f->condition_str, f->file, f->line);

                if (f->expected_str)
                    fprintf(stderr, "   Expected: %s%s%s\n", KGRN, f->expected_str, KNRM);
                if (f->actual_str)
                    fprintf(stderr, "   Got: %s%s%s\n", KRED, f->actual_str, KNRM);
            }
            f = f->next;
        }
        break;
    case _UT_STATUS_CRASHED:
        printf("\n   %sCRASHED%s (%.2f ms)\n", KRED, KNRM, test->duration_ms);
        fprintf(stderr, "   Test process terminated unexpectedly.\n   Output:\n---\n%s\n---\n", test->captured_output ? test->captured_output : " (none)");
        break;
    case _UT_STATUS_TIMEOUT:
        printf("\n   %sTIMEOUT%s (%.2f ms)\n", KYEL, KNRM, test->duration_ms);
        break;
    default:
        printf("\n   %sUNKNOWN STATUS%s\n", KYEL, KNRM);
        break;
    }
}

static void _UT_print_colored_details(const char *details)
{
    printf("Details: ");
    for (size_t i = 0; i < strlen(details); i++)
    {
        if (details[i] == '+')
            printf("%s+%s", KGRN, KNRM);
        else if (details[i] == '-')
            printf("%s-%s", KRED, KNRM);
        else
            printf("%c", details[i]);
    }
}

static void _UT_console_on_suite_finish(const _UT_SuiteResult *suite)
{
    if (suite->total_tests == 0)
        return;
    int failed = suite->total_tests - suite->passed_tests;
    printf("\n%sPassed%s: %s%d%s, %sFailed%s: %s%d%s, Total: %d, ", KGRN, KNRM, KGRN, suite->passed_tests, KNRM, KRED, KNRM, KRED, failed, KNRM, suite->total_tests);
    _UT_print_colored_details(suite->details);
    printf("\n\n");
}

static void _UT_console_on_run_finish(const _UT_TestRun *run, _UT_SuiteResult **all_suites, int suite_count)
{
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("%s Overall Summary%s\n", KBLU, KNRM);
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("Suites run:    %d\n", run->total_suites);
    printf("Total tests:   %d\n", run->total_tests);
    printf("%sPassed:        %d%s\n", KGRN, run->passed_tests, KNRM);
    printf("%sFailed:        %d%s\n", KRED, run->total_tests - run->passed_tests, KNRM);
    printf("Success rate:  %.2f%%\n", run->total_tests > 0 ? ((double)run->passed_tests / run->total_tests) * 100.0 : 100.0);
    printf("Total time:    %.2f ms\n", run->total_duration_ms);
    printf("%s========================================%s\n", KBLU, KNRM);

    if (_UT_is_ci_mode)
    {
        printf("\n");
        for (int i = 0; i < suite_count; ++i)
        {
            printf("%d/%d%s", all_suites[i]->passed_tests, all_suites[i]->total_tests, i == suite_count - 1 ? "" : " ");
        }
        printf("\n");
        for (int i = 0; i < suite_count; ++i)
        {
            for (int j = 0; j < all_suites[i]->total_tests; ++j)
            {
                printf("%c%s", all_suites[i]->details[j], j == all_suites[i]->total_tests - 1 ? "" : ";");
            }
            if (i < suite_count - 1)
                printf(";;");
        }
        printf("\n");
        for (int i = 0; i < suite_count; ++i)
        {
            printf("%d%s", all_suites[i]->passed_tests, i == suite_count - 1 ? "" : ";");
        }
        printf("\n");
        for (int i = 0; i < suite_count; ++i)
        {
            double r = all_suites[i]->total_tests > 0 ? (double)all_suites[i]->passed_tests / all_suites[i]->total_tests : 1.0;
            printf("%.3f%s", r, i == suite_count - 1 ? "" : ";");
        }
        printf("\n");
    }
}

// The instance of the console reporter.
static _UT_Reporter _UT_ConsoleReporter = {
    .on_run_start = NULL, // No action needed
    .on_suite_start = _UT_console_on_suite_start,
    .on_test_finish = _UT_console_on_test_finish,
    .on_suite_finish = _UT_console_on_suite_finish,
    .on_run_finish = _UT_console_on_run_finish,
};

/*============================================================================*/
/* SECTION 9: MAIN TEST RUNNER                                                */
/*============================================================================*/

int _UT_RUN_ALL_TESTS_impl(int argc, char *argv[])
{
    // ================== CHILD PROCESS EXECUTION ==================
    if (argc == 4 && strcmp(argv[1], _UT_ARG_RUN_TEST) == 0)
    {
        _UT_TestInfo *current = _UT_registry_head;
        while (current)
        {
            if (strcmp(current->suite_name, argv[2]) == 0 && strcmp(current->test_name, argv[3]) == 0)
            {
                setvbuf(stdout, NULL, _IONBF, 0);
                setvbuf(stderr, NULL, _IONBF, 0);

#ifdef UT_MEMORY_TRACKING_ENABLED
                UT_disable_memory_tracking();
#endif
                g_UT_current_test_result = (_UT_TestResult *)calloc(1, sizeof(_UT_TestResult));
#ifdef UT_MEMORY_TRACKING_ENABLED
                UT_enable_memory_tracking();
                _UT_init_memory_tracking();
#endif

                current->func();

#ifdef UT_MEMORY_TRACKING_ENABLED
                if (_UT_leak_UT_check_enabled)
                {
                    _UT_check_for_leaks();
                }
#endif

                if (g_UT_current_test_result->failures == NULL)
                {
                    g_UT_current_test_result->status = _UT_STATUS_PASSED;
                }
                else
                {
                    g_UT_current_test_result->status = _UT_STATUS_FAILED;
                }
                _UT_serialize_result(stdout, g_UT_current_test_result);

                _UT_free_test_result(g_UT_current_test_result);
                return 0;
            }
            current = current->next;
        }
        fprintf(stderr, "Error: Test '%s.%s' not found in registry.\n", argv[2], argv[3]);
        return 1;
    }

    // ================== PARENT PROCESS (TEST RUNNER) ==================

    _UT_init_colors();
    _UT_is_ci_mode = getenv("CI") != NULL;         // Enable CI mode if CI env var is set
    _UT_Reporter *reporter = &_UT_ConsoleReporter; // Default reporter

    const char *suite_filter = NULL;
    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], _UT_ARG_SUITE_FILTER, _UT_ARG_SUITE_FILTER_LEN) == 0)
        {
            suite_filter = argv[i] + _UT_ARG_SUITE_FILTER_LEN;
            break;
        }
    }

    _UT_TestRun test_run = {0};
    _UT_SuiteResult *all_suites_array[_UT_MAX_SUITES] = {0};

    const char *executable_path = argv[0];
    _UT_TestInfo *current_test_info = _UT_registry_head;

    _UT_SuiteResult *current_suite_result = NULL;
    const char *current_suite_name = "";

    struct timespec run_start_time, run_end_time;
    clock_gettime(CLOCK_MONOTONIC, &run_start_time);

    if (reporter->on_run_start)
        reporter->on_run_start(&test_run);

    while (current_test_info)
    {
        if (!suite_filter || strcmp(current_test_info->suite_name, suite_filter) == 0)
        {
            // If suite has changed, finalize the old one and start a new one
            if (strcmp(current_suite_name, current_test_info->suite_name) != 0)
            {
                if (current_suite_result && reporter->on_suite_finish)
                {
                    reporter->on_suite_finish(current_suite_result);
                }

                current_suite_name = current_test_info->suite_name;
                UT_disable_memory_tracking();
                current_suite_result = (_UT_SuiteResult *)calloc(1, sizeof(_UT_SuiteResult));
                UT_enable_memory_tracking();
                current_suite_result->name = current_suite_name;

                // Add to the flat array for final CI summary
                if (test_run.total_suites < _UT_MAX_SUITES)
                {
                    all_suites_array[test_run.total_suites] = current_suite_result;
                }
                test_run.total_suites++;

                if (reporter->on_suite_start)
                    reporter->on_suite_start(current_suite_result);
            }

            // Print progress indicator
            printf("\n%s: ", current_test_info->test_name);
            fflush(stdout);

            struct timespec test_start_time, test_end_time;
            clock_gettime(CLOCK_MONOTONIC, &test_start_time);

#ifdef _WIN32
            _UT_TestResult *result = _UT_run_process_win(current_test_info, executable_path);
#else
            _UT_TestResult *result = _UT_run_process_posix(current_test_info, executable_path);
#endif

            clock_gettime(CLOCK_MONOTONIC, &test_end_time);
            result->duration_ms = (test_end_time.tv_sec - test_start_time.tv_sec) * 1000.0 + (test_end_time.tv_nsec - test_start_time.tv_nsec) / 1000000.0;

            // Update stats
            current_suite_result->total_tests++;
            test_run.total_tests++;
            if (result->status == _UT_STATUS_PASSED || result->status == _UT_STATUS_DEATH_TEST_PASSED)
            {
                current_suite_result->passed_tests++;
                test_run.passed_tests++;
                if (current_suite_result->details_idx < _UT_SUITE_RESULTS_DETAILS_SIZE - 1)
                {
                    current_suite_result->details[current_suite_result->details_idx++] = '+';
                }
            }
            else
            {
                if (current_suite_result->details_idx < _UT_SUITE_RESULTS_DETAILS_SIZE - 1)
                {
                    current_suite_result->details[current_suite_result->details_idx++] = '-';
                }
            }

            // Report and free immediately
            if (reporter->on_test_finish)
                reporter->on_test_finish(result);
            _UT_free_test_result(result);
        }
        current_test_info = current_test_info->next;
    }

    // Finalize the very last suite
    if (current_suite_result && reporter->on_suite_finish)
    {
        reporter->on_suite_finish(current_suite_result);
    }

    clock_gettime(CLOCK_MONOTONIC, &run_end_time);
    test_run.total_duration_ms = (run_end_time.tv_sec - run_start_time.tv_sec) * 1000.0 + (run_end_time.tv_nsec - run_start_time.tv_nsec) / 1000000.0;

    // Final overall summary
    if (reporter->on_run_finish)
        reporter->on_run_finish(&test_run, all_suites_array, test_run.total_suites);

    // Cleanup
    UT_disable_memory_tracking();
    for (int i = 0; i < test_run.total_suites; ++i)
    {
        free(all_suites_array[i]);
    }
    UT_enable_memory_tracking();

    return (test_run.total_tests - test_run.passed_tests) > 0 ? 1 : 0;
}
#endif // UNIT_TEST_IMPLEMENTATION

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // UNIT_TEST_H
