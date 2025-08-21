#ifndef ROBUST_TEST_H
#define ROBUST_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/*============================================================================*/
/* SECTION 1: CORE DEFINITIONS & PLATFORM ABSTRACTION                         */
/*============================================================================*/

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define IS_TTY _isatty(_fileno(stdout))
#else // POSIX
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <time.h>
    #include <errno.h>
    #define IS_TTY isatty(STDOUT_FILENO)
#endif

#ifndef _TEST_TIMEOUT_SECONDS
    #define _TEST_TIMEOUT_SECONDS 2
#endif

#define _STDERR_BUFFER_SIZE 4096
#define _SUITE_RESULTS_BUFFER_SIZE 1024
#define _MAX_SUITES 128

// Forward declarations
typedef void (*_TestFunction)(void);
typedef struct _TestInfo _TestInfo;
typedef struct { const char* expected_msg; int expected_signal; int expected_exit_code; } _DeathExpect;

struct _TestInfo {
    const char* suite_name;
    const char* test_name;
    _TestFunction func;
    const _DeathExpect* death_expect;
    _TestInfo* next;
};

/*============================================================================*/
/* SECTION 2: GLOBAL STATE AND COLOR MANAGEMENT                               */
/*============================================================================*/

static _TestInfo* g_test_registry_head = NULL;
static _TestInfo* g_test_registry_tail = NULL;
static int _g_use_color = 1;
static int _g_is_ci_mode = 0;

typedef struct { int passed; int total; char details[_SUITE_RESULTS_BUFFER_SIZE]; } _SuiteResult;
static _SuiteResult _g_all_suite_results[_MAX_SUITES];
static int _g_suite_count = 0;

#define KNRM (_g_use_color ? "\x1B[0m" : "")
#define KRED (_g_use_color ? "\x1B[31m" : "")
#define KGRN (_g_use_color ? "\x1B[32m" : "")
#define KYEL (_g_use_color ? "\x1B[33m" : "")
#define KBLU (_g_use_color ? "\x1B[34m" : "")

static void _init_colors(void) {
    const char* no_color = getenv("NO_COLOR");
    _g_use_color = IS_TTY && !no_color;
#ifdef _WIN32
    if (_g_use_color) {
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
#ifdef MEMORY_TRACKING_ENABLED
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4090 4091)
#endif
#pragma push_macro("malloc")
#pragma push_macro("calloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef calloc
#undef realloc
#undef free
typedef struct _MemInfo { void* address; size_t size; const char* file; int line; struct _MemInfo* next; } _MemInfo;
static _MemInfo* _g_mem_head = NULL;
static int _g_alloc_count = 0, g_free_count = 0;
static int _g_mem_tracking_enabled = 0;
static void _init_memory_tracking(void) { while (_g_mem_head != NULL) { _MemInfo* temp = _g_mem_head; _g_mem_head = _g_mem_head->next; free(temp); } _g_alloc_count = 0; g_free_count = 0; _g_mem_tracking_enabled = 1; }
static void _check_for_leaks(void) { _g_mem_tracking_enabled = 0; if (_g_mem_head != NULL) { fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM); fprintf(stderr, "   Reason: Memory leak detected.\n"); _MemInfo* current = _g_mem_head; while (current != NULL) { fprintf(stderr, "      - %zu bytes allocated at %s:%d\n", current->size, current->file, current->line); current = current->next; } exit(1); } }
static void* _test_malloc(size_t size, const char* file, int line) { void* ptr = malloc(size); if (_g_mem_tracking_enabled && ptr) { _g_mem_tracking_enabled = 0; _MemInfo* info = (_MemInfo*)malloc(sizeof(_MemInfo)); _g_mem_tracking_enabled = 1; if (info) { info->address = ptr; info->size = size; info->file = file; info->line = line; info->next = _g_mem_head; _g_mem_head = info; _g_alloc_count++; } } return ptr; }
static void* _test_calloc(size_t num, size_t size, const char* file, int line) { void* ptr = calloc(num, size); if (_g_mem_tracking_enabled && ptr) { _g_mem_tracking_enabled = 0; _MemInfo* info = (_MemInfo*)malloc(sizeof(_MemInfo)); _g_mem_tracking_enabled = 1; if (info) { info->address = ptr; info->size = num * size; info->file = file; info->line = line; info->next = _g_mem_head; _g_mem_head = info; _g_alloc_count++; } } return ptr; }
static void* _test_realloc(void* old_ptr, size_t new_size, const char* file, int line) { if (old_ptr == NULL) return _test_malloc(new_size, file, line); if (_g_mem_tracking_enabled) { _MemInfo* c = _g_mem_head; while (c != NULL && c->address != old_ptr) { c = c->next; } if (c == NULL) { fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: realloc of invalid pointer (%p) at %s:%d\n", KRED, KNRM, old_ptr, file, line); exit(1); } } void* new_ptr = realloc(old_ptr, new_size); if (_g_mem_tracking_enabled && new_ptr) { _MemInfo* c = _g_mem_head; while (c != NULL) { if (c->address == old_ptr) { c->address = new_ptr; c->size = new_size; c->file = file; c->line = line; break; } c = c->next; } } return new_ptr; }
static void _test_free(void* ptr, const char* file, int line) { if (ptr == NULL) { if (_g_mem_tracking_enabled) { fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: Attempt to free NULL pointer at %s:%d\n", KRED, KNRM, file, line); exit(1); } return; } if (_g_mem_tracking_enabled) { _MemInfo *c = _g_mem_head, *p = NULL; while (c != NULL && c->address != ptr) { p = c; c = c->next; } if (c == NULL) { fprintf(stderr, "\n   %sTEST FAILED!%s\n   Reason: Invalid or double-freed pointer (%p) at %s:%d\n", KRED, KNRM, ptr, file, line); exit(1); } if (p == NULL) _g_mem_head = c->next; else p->next = c->next; _g_mem_tracking_enabled = 0; free(c); _g_mem_tracking_enabled = 1; g_free_count++; } free(ptr); }
#pragma pop_macro("free")
#pragma pop_macro("realloc")
#pragma pop_macro("calloc")
#pragma pop_macro("malloc")
#define malloc(size) _test_malloc(size, __FILE__, __LINE__)
#define calloc(num, size) _test_calloc(num, size, __FILE__, __LINE__)
#define realloc(ptr, size) _test_realloc(ptr, size, __FILE__, __LINE__)
#define free(ptr) _test_free(ptr, __FILE__, __LINE__)
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif // MEMORY_TRACKING_ENABLED

/*============================================================================*/
/* SECTION 4: TEST REGISTRATION API                                           */
/*============================================================================*/
static void _register_test(_TestInfo* test_info) { if (g_test_registry_head == NULL) { g_test_registry_head = test_info; g_test_registry_tail = test_info; } else { g_test_registry_tail->next = test_info; g_test_registry_tail = test_info; } }
#ifdef _MSC_VER
    #pragma section(".CRT$XCU", read)
    #define _TEST_INITIALIZER(f) static void __cdecl f(void); __declspec(dllexport, allocate(".CRT$XCU")) void (__cdecl* f##_)(void) = f; static void __cdecl f(void)
#else
    #define _TEST_INITIALIZER(f) static void f(void) __attribute__((constructor)); static void f(void)
#endif
#define _PASTE(a, b) a##b
#define _CONCAT(a, b) _PASTE(a, b)

/**
 * @brief Defines a standard test case.
 *
 * This macro creates and registers a test function within a given test suite.
 * The code block that follows the macro constitutes the body of the test.
 *
 * @param SuiteName The name of the test suite to which this test belongs.
 * @param TestDescription A descriptive name for the test case.
 */
#define TEST_CASE(SuiteName, TestDescription) static void _CONCAT(test_func_, __LINE__)(void); _TEST_INITIALIZER(_CONCAT(test_registrar_, __LINE__)) { static _TestInfo ti = { #SuiteName, TestDescription, _CONCAT(test_func_, __LINE__), NULL, NULL }; _register_test(&ti); } static void _CONCAT(test_func_, __LINE__)(void)

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
#define TEST_DEATH_CASE(SuiteName, TestDescription, ...) static void _CONCAT(test_func_, __LINE__)(void); _TEST_INITIALIZER(_CONCAT(test_registrar_, __LINE__)) { static _DeathExpect de = { .expected_msg = NULL, .expected_signal = 0, .expected_exit_code = -1, __VA_ARGS__ }; static _TestInfo ti = { #SuiteName, TestDescription, _CONCAT(test_func_, __LINE__), &de, NULL }; _register_test(&ti); } static void _CONCAT(test_func_, __LINE__)(void)

/*============================================================================*/
/* SECTION 5: ASSERTION MACROS                                                */
/*============================================================================*/
#define _ASSERT_GENERIC(condition, condition_str, expected_str, actual_str) do { if (!(condition)) { fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM); fprintf(stderr, "   Assertion failed: %s\n      At: %s:%d\n", condition_str, __FILE__, __LINE__); if ((expected_str)[0]) fprintf(stderr, "   Expected: %s%s%s\n", KGRN, expected_str, KNRM); if ((actual_str)[0]) fprintf(stderr, "   Got: %s%s%s\n", KRED, actual_str, KNRM); exit(1); } } while (0)
#define _ASSERT_GENERIC_PROP(condition, condition_str, help_text, actual_val_str) do { if (!(condition)) { fprintf(stderr, "\n   %sTEST FAILED!%s\n", KRED, KNRM); fprintf(stderr, "   Property failed: %s\n      At: %s:%d\n", condition_str, __FILE__, __LINE__); fprintf(stderr, "   Reason: %s%s%s\n", KGRN, help_text, KNRM); fprintf(stderr, "   Actual value: %s%s%s\n", KRED, actual_val_str, KNRM); exit(1); } } while (0)

/**
 * @brief Asserts that a condition is true.
 *
 * If the condition evaluates to false, the test fails immediately and prints an error message.
 *
 * @param condition The expression to evaluate.
 */
#define ASSERT(condition) _ASSERT_GENERIC(!!(condition), #condition, "true", (condition) ? "true" : "false")

/**
 * @brief Asserts that a condition is false.
 *
 * If the condition evaluates to true, the test fails immediately and prints an error message.
 *
 * @param condition The expression to evaluate.
 */
#define REFUTE(condition) _ASSERT_GENERIC(!(condition), #condition, "false", (condition) ? "true" : "false")

/**
 * @brief Asserts that two integer values are equal.
 *
 * If the values are not equal, the test fails and prints both the expected and actual values.
 *
 * @param expected The expected integer value.
 * @param actual The actual integer value to check.
 */
#define EQUAL_INT(expected, actual) do { int e = (expected); int a = (actual); char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two character values are equal.
 *
 * If the characters are not equal, the test fails and prints both.
 *
 * @param expected The expected char value.
 * @param actual The actual char value to check.
 */
#define EQUAL_CHAR(expected, actual) do { char e = (expected); char a = (actual); char e_buf[4] = {'\'', e, '\'', 0}, a_buf[4] = {'\'', a, '\'', 0}; _ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two C-style strings are equal.
 *
 * Uses strcmp for comparison. The test fails if the strings are different, or if either pointer is NULL.
 *
 * @param expected The expected string value.
 * @param actual The actual string value to check.
 */
#define EQUAL_STRING(expected, actual) do { const char* e = (expected); const char* a = (actual); _ASSERT_GENERIC(e && a && strcmp(e, a) == 0, #expected " == " #actual, e ? e : "NULL", a ? a : "NULL"); } while (0)

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
#define EQUAL_BY(expected, actual, compare_fn, print_fn) do { __typeof__(expected) _exp = (expected); __typeof__(actual) _act = (actual); if (!compare_fn(_exp, _act)) { char _exp_str[1024] = {0}, _act_str[1024] = {0}; print_fn(_exp_str, sizeof(_exp_str), _exp); print_fn(_act_str, sizeof(_act_str), _act); _ASSERT_GENERIC(0, #compare_fn "(" #expected ", " #actual ")", _exp_str, _act_str); } } while (0)

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
#define PROPERTY(value, predicate_fn, print_fn, help_text) do { __typeof__(value) _val = (value); if (!predicate_fn(_val)) { char _val_str[1024] = {0}; print_fn(_val_str, sizeof(_val_str), _val); char _cond_str[1024]; snprintf(_cond_str, sizeof(_cond_str), "%s(%s)", #predicate_fn, #value); _ASSERT_GENERIC_PROP(0, _cond_str, help_text, _val_str); } } while(0)

static void _robust_test_print_int(char* buf, size_t size, int val) { snprintf(buf, size, "%d", val); }
static void _robust_test_print_char(char* buf, size_t size, char val) { snprintf(buf, size, "'%c'", val); }
static void _robust_test_print_string(char* buf, size_t size, const char* val) { snprintf(buf, size, "\"%s\"", val); }

/**
 * @brief Asserts that an integer value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_INT(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_int, help_text)

/**
 * @brief Asserts that a character value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_CHAR(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_char, help_text)

/**
 * @brief Asserts that a string value satisfies a given property.
 * @see PROPERTY
 */
#define PROPERTY_STRING(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_string, help_text)

#ifdef MEMORY_TRACKING_ENABLED
/**
 * @brief (Memory Tracking) Asserts that the total number of malloc/calloc/realloc calls matches the expected count.
 *
 * This macro is only available when the `MEMORY_TRACKING_ENABLED` flag is defined during compilation.
 *
 * @param expected The expected number of allocations.
 */
#define ASSERT_ALLOC_COUNT(expected) do { int e = (expected), a = _g_alloc_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, "_g_alloc_count == " #expected, e_buf, a_buf); } while(0)

/**
 * @brief (Memory Tracking) Asserts that the total number of free calls matches the expected count.
 *
 * This macro is only available when the `MEMORY_TRACKING_ENABLED` flag is defined during compilation.
 *
 * @param expected The expected number of successful frees.
 */
#define ASSERT_FREE_COUNT(expected) do { int e = (expected), a = g_free_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, "g_free_count == " #expected, e_buf, a_buf); } while(0)
#endif

/*============================================================================*/
/* SECTION 6: THE RUN_ALL_TESTS IMPLEMENTATION (FULL VERSION)                 */
/*============================================================================*/

// Internal function to run all tests. Use the RUN_ALL_TESTS() macro in your main function.
int _run_all_tests_impl(int argc, char* argv[]);

/**
 * @brief Runs all registered test cases.
 *
 * This macro should be called from the `main` function of your test executable.
 * It discovers and executes all tests defined with `TEST_CASE` and `TEST_DEATH_CASE`,
 * summarizes the results, and returns an exit code.
 *
 * @return 0 if all tests pass, 1 otherwise.
 */
#define RUN_ALL_TESTS() _run_all_tests_impl(argc, argv)

#ifdef _WIN32
static int _run_test_process_win(_TestInfo* test, const char* executable_path, char* stderr_buffer) {
    char command_line[2048];
    snprintf(command_line, sizeof(command_line), "\"%s\" --run_test \"%s\" \"%s\"", executable_path, test->suite_name, test->test_name);
    HANDLE h_read = NULL, h_write = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&h_read, &h_write, &sa, 0)) { snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "CreatePipe failed."); return -1; }
    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.hStdError = h_write; si.hStdOutput = h_write; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: CreateProcess failed (error %lu).", KRED, KNRM, GetLastError());
        CloseHandle(h_read); CloseHandle(h_write); return 0;
    }
    CloseHandle(h_write);
    DWORD wait_result = WaitForSingleObject(pi.hProcess, _TEST_TIMEOUT_SECONDS * 1000);
    DWORD bytes_read = 0; ReadFile(h_read, stderr_buffer, _STDERR_BUFFER_SIZE - 1, &bytes_read, NULL);
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exceeded timeout of %d seconds.", KRED, KNRM, _TEST_TIMEOUT_SECONDS);
        return 0;
    }
    DWORD exit_code; GetExitCodeProcess(pi.hProcess, &exit_code);
    if (test->death_expect) {
        const _DeathExpect* de = test->death_expect;
        int msg_ok = !de->expected_msg || strstr(stderr_buffer, de->expected_msg);
        int exit_ok = de->expected_exit_code == -1 || exit_code == (DWORD)de->expected_exit_code;
        if (exit_code != 0 && msg_ok && exit_ok) return 2; // Death test pass
        char temp_buffer[2048];
        snprintf(temp_buffer, sizeof(temp_buffer), "\n   %sTEST FAILED!%s\n   Reason: Death test criteria not met (exit code 0x%X).", KRED, KNRM, (unsigned int)exit_code);
        if (!msg_ok && de->expected_msg) { char msg_temp[512]; snprintf(msg_temp, sizeof(msg_temp), "\n   Expected message substring: \"%s\"", de->expected_msg); strcat(temp_buffer, msg_temp); }
        if (bytes_read > 0) { strcat(temp_buffer, "\n   Got output:\n---\n"); strcat(temp_buffer, stderr_buffer); strcat(temp_buffer, "\n---\n"); }
        strcpy(stderr_buffer, temp_buffer);
        return 0;
    }
    if (exit_code == 0) return 1; // Normal pass
    if (strlen(stderr_buffer) == 0) { snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exited with code 0x%X.", KRED, KNRM, (unsigned int)exit_code); }
    return 0;
}
#else
static int _wait_with_timeout(pid_t pid, int* status, int timeout_sec) { struct timespec start, now, sleep_ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 }; clock_gettime(CLOCK_MONOTONIC, &start); for (;;) { pid_t r = waitpid(pid, status, WNOHANG); if (r == pid) return 0; if (r == -1 && errno != EINTR) return -1; clock_gettime(CLOCK_MONOTONIC, &now); if ((now.tv_sec - start.tv_sec) >= timeout_sec) { kill(pid, SIGKILL); while (waitpid(pid, status, 0) == -1 && errno == EINTR); return 1; } nanosleep(&sleep_ts, NULL); } }
static int _run_test_process_posix(_TestInfo* test, const char* executable_path, char* stderr_buffer) {
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
        close(err_pipe[1]); int status; int r = _wait_with_timeout(pid, &status, _TEST_TIMEOUT_SECONDS);
        ssize_t off = 0, bytes_read;
        int flags = fcntl(err_pipe[0], F_GETFL); fcntl(err_pipe[0], F_SETFL, flags | O_NONBLOCK);
        while ((bytes_read = read(err_pipe[0], stderr_buffer + off, _STDERR_BUFFER_SIZE - 1 - off)) > 0) { off += bytes_read; }
        close(err_pipe[0]);
        if (r == 1) { snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exceeded timeout of %d seconds.", KRED, KNRM, _TEST_TIMEOUT_SECONDS); return 0; }
        else if (r == -1) { perror("waitpid"); return -1; }
        if (test->death_expect) {
            const _DeathExpect* de = test->death_expect;
            int sig_ok = 1, exit_ok = 1, msg_ok = 1;
            if (WIFSIGNALED(status)) { if(de->expected_signal != 0 && WTERMSIG(status) != de->expected_signal) sig_ok = 0; }
            else { if(de->expected_signal != 0) sig_ok = 0; }
            if (WIFEXITED(status)) { if(de->expected_exit_code != -1 && WEXITSTATUS(status) != de->expected_exit_code) exit_ok = 0; }
            else { if(de->expected_exit_code != -1) exit_ok = 0; }
            if(de->expected_msg && !strstr(stderr_buffer, de->expected_msg)) msg_ok = 0;
            if (sig_ok && exit_ok && msg_ok && (WIFSIGNALED(status) || WIFEXITED(status))) { return 2; }
            else {
                char temp_buffer[2048];
                snprintf(temp_buffer, sizeof(temp_buffer), "\n   %sTEST FAILED!%s\n   Reason: Death test criteria not met.", KRED, KNRM);
                if (!msg_ok && de->expected_msg) { char msg_temp[512]; snprintf(msg_temp, sizeof(msg_temp), "\n   Expected message substring: \"%s\"", de->expected_msg); strcat(temp_buffer, msg_temp); }
                if (off > 0) { strcat(temp_buffer, "\n   Got output:\n---\n"); strcat(temp_buffer, stderr_buffer); strcat(temp_buffer, "\n---"); }
                strcpy(stderr_buffer, temp_buffer);
                return 0;
            }
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
        if (strlen(stderr_buffer) == 0) {
            if (WIFSIGNALED(status)) { snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Terminated by signal: %s.", KRED, KNRM, strsignal(WTERMSIG(status))); }
            else { snprintf(stderr_buffer, _STDERR_BUFFER_SIZE, "\n   %sTEST FAILED!%s\n   Reason: Exited with code %d.", KRED, KNRM, WEXITSTATUS(status)); }
        }
        return 0;
    }
}
#endif

static void _print_colored_details(const char* details) {
    printf("Details: ");
    for (size_t i = 0; i < strlen(details); i++) {
        if (details[i] == '+') printf("%s+%s", KGRN, KNRM);
        else if (details[i] == '-') printf("%s-%s", KRED, KNRM);
        else printf("%c", details[i]);
    }
}

static void _finalize_suite(const char* name, int total, int failed, const char* details) {
    if (total == 0) return;
    printf("\n\n%sPassed%s: %s%d%s, %sFailed%s: %s%d%s, Total: %d, ", KGRN, KNRM, KGRN, total - failed, KNRM, KRED, KNRM, KRED, failed, KNRM, total);
    _print_colored_details(details);
    printf("\n\n");
    if (_g_suite_count < _MAX_SUITES) {
        _g_all_suite_results[_g_suite_count].passed = total - failed;
        _g_all_suite_results[_g_suite_count].total = total;
        strncpy(_g_all_suite_results[_g_suite_count].details, details, _SUITE_RESULTS_BUFFER_SIZE -1);
        _g_all_suite_results[_g_suite_count].details[_SUITE_RESULTS_BUFFER_SIZE -1] = '\0';
    }
    _g_suite_count++;
}

int _run_all_tests_impl(int argc, char* argv[]) {
    if (argc == 4 && strcmp(argv[1], "--run_test") == 0) {
        _TestInfo* current = g_test_registry_head;
        while (current) {
            if (strcmp(current->suite_name, argv[2]) == 0 && strcmp(current->test_name, argv[3]) == 0) {
                setvbuf(stdout, NULL, _IONBF, 0); setvbuf(stderr, NULL, _IONBF, 0);
                #ifdef MEMORY_TRACKING_ENABLED
                _init_memory_tracking();
                #endif
                current->func();
                #ifdef MEMORY_TRACKING_ENABLED
                _check_for_leaks();
                #endif
                return 0;
            }
            current = current->next;
        }
        fprintf(stderr, "Error: Test '%s.%s' not found in registry.\n", argv[2], argv[3]); return 1;
    }
    _g_is_ci_mode = 1; // argc > 1;
    int total = 0, passed = 0, failed = 0;
    int suite_total = 0, suite_failed = 0;
    char suite_results[_SUITE_RESULTS_BUFFER_SIZE];
    int suite_results_idx = 0;
    const char* current_suite = "";
    const char* executable_path = argv[0];
    _init_colors();
    _TestInfo* current_test = g_test_registry_head;
    while (current_test) {
        if (strcmp(current_suite, current_test->suite_name) != 0) {
            _finalize_suite(current_suite, suite_total, suite_failed, suite_results);
            current_suite = current_test->suite_name;
            printf("%sTests for %s%s\n", KBLU, current_suite, KNRM);
            for(int i=0; i < (int)(strlen(current_suite) + 10); ++i) printf("%s=%s", KBLU, KNRM);
            suite_total = 0; suite_failed = 0; suite_results_idx = 0; 
            for(int i=0; i<_SUITE_RESULTS_BUFFER_SIZE; i++) suite_results[i] = '\0';
        }
        printf("\n%s: ", current_test->test_name);
        fflush(stdout);
        char stderr_buffer[_STDERR_BUFFER_SIZE] = {0};
        #ifdef _WIN32
            int test_result = _run_test_process_win(current_test, executable_path, stderr_buffer);
        #else
            int test_result = _run_test_process_posix(current_test, executable_path, stderr_buffer);
        #endif
        if (test_result > 0) { // 1 for pass, 2 for death test pass
            if (test_result == 2) { printf("\n   %sTEST PASSED SUCCESSFULLY!%s (Abnormal exit expected)", KGRN, KNRM); }
            else { printf("\n   %sTEST PASSED SUCCESSFULLY!%s", KGRN, KNRM); }
            passed++; if (suite_results_idx < _SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '+';
        } else { // 0 for fail, -1 for error
            failed++; suite_failed++; if (suite_results_idx < _SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '-';
            printf("%s", stderr_buffer);
        }
        total++; suite_total++; current_test = current_test->next;
    }
    suite_results[suite_results_idx] = '\0';
    _finalize_suite(current_suite, suite_total, suite_failed, suite_results);
    double rate = total > 0 ? ((double)passed / total) * 100.0 : 100.0;
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("%s Overall Summary%s\n", KBLU, KNRM);
    printf("%s========================================%s\n", KBLU, KNRM);
    printf("Suites run:    %d\n", _g_suite_count);
    printf("Total tests:   %d\n", total);
    printf("%sPassed:        %d%s\n", KGRN, passed, KNRM);
    printf("%sFailed:        %d%s\n", KRED, failed, KNRM);
    printf("Success rate:  %.2f%%\n", rate);
    printf("%s========================================%s\n", KBLU, KNRM);
    if(_g_is_ci_mode) {
        printf("\n");
        for(int i=0; i < _g_suite_count; ++i) { printf("%d/%d%s", _g_all_suite_results[i].passed, _g_all_suite_results[i].total, i == _g_suite_count - 1 ? "" : " "); }
        printf("\n");
        for(int i=0; i < _g_suite_count; ++i) { for(int j=0; j < _g_all_suite_results[i].total; ++j) { printf("%c%s", _g_all_suite_results[i].details[j], j == _g_all_suite_results[i].total - 1 ? "" : ";"); } if (i < _g_suite_count - 1) printf(";;"); }
        printf("\n");
        for(int i=0; i < _g_suite_count; ++i) { printf("%d%s", _g_all_suite_results[i].passed, i == _g_suite_count - 1 ? "" : ";"); }
        printf("\n");
        for(int i=0; i < _g_suite_count; ++i) { double r = _g_all_suite_results[i].total > 0 ? (double)_g_all_suite_results[i].passed / _g_all_suite_results[i].total : 1.0; printf("%.3f%s", r, i == _g_suite_count - 1 ? "" : ";"); }
        printf("\n");
    }
    return failed > 0 ? 1 : 0;
}
#endif // ROBUST_TEST_H