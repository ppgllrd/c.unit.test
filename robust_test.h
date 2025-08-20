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
static int _g_use_color = 0;

#define KNRM (_g_use_color ? "\x1B[0m" : "")
#define KRED (_g_use_color ? "\x1B[31m" : "")
#define KGRN (_g_use_color ? "\x1B[32m" : "")
#define KYEL (_g_use_color ? "\x1B[33m" : "")
#define KBLU (_g_use_color ? "\x1B[34m" : "")

/**
 * @brief Initializes color detection based on TTY and NO_COLOR environment variable.
 *        On Windows, it also enables virtual terminal processing for ANSI codes.
 */
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
static void _check_for_leaks(void) { _g_mem_tracking_enabled = 0; if (_g_mem_head != NULL) { fprintf(stderr, "    %s[ FAIL ] [MEMORY LEAK]%s:\n", KRED, KNRM); _MemInfo* current = _g_mem_head; while (current != NULL) { fprintf(stderr, "      - %zu bytes allocated at %s:%d\n", current->size, current->file, current->line); current = current->next; } exit(1); } }
static void* _test_malloc(size_t size, const char* file, int line) { void* ptr = malloc(size); if (_g_mem_tracking_enabled && ptr) { _g_mem_tracking_enabled = 0; _MemInfo* info = (_MemInfo*)malloc(sizeof(_MemInfo)); _g_mem_tracking_enabled = 1; if (info) { info->address = ptr; info->size = size; info->file = file; info->line = line; info->next = _g_mem_head; _g_mem_head = info; _g_alloc_count++; } } return ptr; }
static void* _test_calloc(size_t num, size_t size, const char* file, int line) { void* ptr = calloc(num, size); if (_g_mem_tracking_enabled && ptr) { _g_mem_tracking_enabled = 0; _MemInfo* info = (_MemInfo*)malloc(sizeof(_MemInfo)); _g_mem_tracking_enabled = 1; if (info) { info->address = ptr; info->size = num * size; info->file = file; info->line = line; info->next = _g_mem_head; _g_mem_head = info; _g_alloc_count++; } } return ptr; }
static void* _test_realloc(void* old_ptr, size_t new_size, const char* file, int line) { if (old_ptr == NULL) return _test_malloc(new_size, file, line); if (_g_mem_tracking_enabled) { _MemInfo* c = _g_mem_head; while (c != NULL && c->address != old_ptr) { c = c->next; } if (c == NULL) { fprintf(stderr, "    %s[ FAIL ]%s realloc of invalid pointer (%p) at %s:%d\n", KRED, KNRM, old_ptr, file, line); exit(1); } } void* new_ptr = realloc(old_ptr, new_size); if (_g_mem_tracking_enabled && new_ptr) { _MemInfo* c = _g_mem_head; while (c != NULL) { if (c->address == old_ptr) { c->address = new_ptr; c->size = new_size; c->file = file; c->line = line; break; } c = c->next; } } return new_ptr; }
static void _test_free(void* ptr, const char* file, int line) { if (ptr == NULL) { if (_g_mem_tracking_enabled) { fprintf(stderr, "    %s[ FAIL ]%s Attempt to free NULL pointer at %s:%d\n", KRED, KNRM, file, line); exit(1); } return; } if (_g_mem_tracking_enabled) { _MemInfo *c = _g_mem_head, *p = NULL; while (c != NULL && c->address != ptr) { p = c; c = c->next; } if (c == NULL) { fprintf(stderr, "    %s[ FAIL ]%s Invalid or double-freed pointer (%p) at %s:%d\n", KRED, KNRM, ptr, file, line); exit(1); } if (p == NULL) _g_mem_head = c->next; else p->next = c->next; _g_mem_tracking_enabled = 0; free(c); _g_mem_tracking_enabled = 1; g_free_count++; } free(ptr); }
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
 * @param SuiteName The name of the test suite (a C identifier).
 * @param TestDescription A string literal describing the test's purpose.
 * @example TEST_CASE(Arithmetic, "Adds two positive numbers") { ... }
 */
#define TEST_CASE(SuiteName, TestDescription) \
    static void _CONCAT(test_func_, __LINE__)(void); \
    _TEST_INITIALIZER(_CONCAT(test_registrar_, __LINE__)) { \
        static _TestInfo ti = { #SuiteName, TestDescription, _CONCAT(test_func_, __LINE__), NULL, NULL }; \
        _register_test(&ti); \
    } static void _CONCAT(test_func_, __LINE__)(void)

/**
 * @brief Defines a "death test" that is expected to terminate abnormally.
 * @param SuiteName The name of the test suite (a C identifier).
 * @param TestDescription A string literal describing the test's purpose.
 * @param ... Designated initializers for the _DeathExpect struct (e.g., .expected_signal = SIGSEGV).
 * @example TEST_DEATH_CASE(Crashes, "Handles null pointer", .expected_signal = SIGSEGV) { ... }
 */
#define TEST_DEATH_CASE(SuiteName, TestDescription, ...) \
    static void _CONCAT(test_func_, __LINE__)(void); \
    _TEST_INITIALIZER(_CONCAT(test_registrar_, __LINE__)) { \
        static _DeathExpect de = { .expected_msg = NULL, .expected_signal = 0, .expected_exit_code = -1, __VA_ARGS__ }; \
        static _TestInfo ti = { #SuiteName, TestDescription, _CONCAT(test_func_, __LINE__), &de, NULL }; \
        _register_test(&ti); \
    } static void _CONCAT(test_func_, __LINE__)(void)

/*============================================================================*/
/* SECTION 5: ASSERTION MACROS                                                */
/*============================================================================*/

// Internal helper macros for reporting assertion failures.
#define _ASSERT_GENERIC(condition, condition_str, expected_str, actual_str) do { if (!(condition)) { fprintf(stderr, "    %s[ FAIL ]%s %s\n      %sAt: %s:%d%s\n", KRED, KNRM, condition_str, KRED, __FILE__, __LINE__, KNRM); if ((expected_str)[0]) fprintf(stderr, "      %sExpected: %s%s\n", KRED, expected_str, KNRM); if ((actual_str)[0]) fprintf(stderr, "      %sGot: %s%s\n", KRED, actual_str, KNRM); exit(1); } } while (0)
#define _ASSERT_GENERIC_PROP(condition, condition_str, help_text, actual_val_str) do { if (!(condition)) { fprintf(stderr, "    %s[ FAIL ]%s Property failed: %s\n      %sAt: %s:%d%s\n", KRED, KNRM, condition_str, KRED, __FILE__, __LINE__, KNRM); fprintf(stderr, "      %sReason: %s%s\n", KRED, help_text, KNRM); fprintf(stderr, "      %sActual value: %s%s\n", KRED, actual_val_str, KNRM); exit(1); } } while (0)

/**
 * @brief Asserts that a condition is true.
 * @param condition The expression to evaluate.
 * @example ASSERT(x > 0);
 */
#define ASSERT(condition) _ASSERT_GENERIC(!!(condition), #condition, "true", (condition) ? "true" : "false")

/**
 * @brief Asserts that a condition is false.
 * @param condition The expression to evaluate.
 * @example REFUTE(x < 0);
 */
#define REFUTE(condition) _ASSERT_GENERIC(!(condition), #condition, "false", (condition) ? "true" : "false")

/**
 * @brief Asserts that two integers are equal.
 * @param expected The expected integer value.
 * @param actual The actual integer value.
 * @example EQUAL_INT(5, my_func());
 */
#define EQUAL_INT(expected, actual) do { int e = (expected); int a = (actual); char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two characters are equal.
 * @param expected The expected character value.
 * @param actual The actual character value.
 * @example EQUAL_CHAR('a', my_func());
 */
#define EQUAL_CHAR(expected, actual) do { char e = (expected); char a = (actual); char e_buf[4] = {e, 0}, a_buf[4] = {a, 0}; _ASSERT_GENERIC(e == a, #expected " == " #actual, e_buf, a_buf); } while (0)

/**
 * @brief Asserts that two C-strings are equal. Does not handle NULL pointers.
 * @param expected The expected string value.
 * @param actual The actual string value.
 * @example EQUAL_STRING("hello", my_func());
 */
#define EQUAL_STRING(expected, actual) do { const char* e = (expected); const char* a = (actual); _ASSERT_GENERIC(strcmp(e, a) == 0, #expected " == " #actual, e, a); } while (0)

/**
 * @brief Asserts that two values are equal using a custom comparison function.
 * @param expected The expected value.
 * @param actual The actual value.
 * @param compare_fn A function `int(type, type)` that returns true if values are equal.
 * @param print_fn A function `void(char* buf, size_t size, type val)` to format the value for display.
 * @example EQUAL_BY(point_a, point_b, are_points_equal, print_point_to_buffer);
 */
#define EQUAL_BY(expected, actual, compare_fn, print_fn) do { __typeof__(expected) _exp = (expected); __typeof__(actual) _act = (actual); if (!compare_fn(_exp, _act)) { char _exp_str[1024] = {0}, _act_str[1024] = {0}; print_fn(_exp_str, sizeof(_exp_str), _exp); print_fn(_act_str, sizeof(_act_str), _act); _ASSERT_GENERIC(0, #compare_fn "(" #expected ", " #actual ")", _exp_str, _act_str); } } while (0)

/**
 * @brief Asserts that a value satisfies a given property (predicate).
 * @param value The value to test.
 * @param predicate_fn A function `int(type)` that returns true if the property holds.
 * @param print_fn A function `void(char* buf, size_t size, type val)` to format the value for display.
 * @param help_text A string explaining what the property is.
 * @example PROPERTY(my_number, is_even, print_int_to_buffer, "The number should be even");
 */
#define PROPERTY(value, predicate_fn, print_fn, help_text) do { __typeof__(value) _val = (value); if (!predicate_fn(_val)) { char _val_str[1024] = {0}; print_fn(_val_str, sizeof(_val_str), _val); char _cond_str[1024]; snprintf(_cond_str, sizeof(_cond_str), "%s(%s)", #predicate_fn, #value); _ASSERT_GENERIC_PROP(0, _cond_str, help_text, _val_str); } } while(0)

// --- Helper print functions for basic types ---
static void _robust_test_print_int(char* buf, size_t size, int val) { snprintf(buf, size, "%d", val); }
static void _robust_test_print_char(char* buf, size_t size, char val) { snprintf(buf, size, "'%c'", val); }
static void _robust_test_print_string(char* buf, size_t size, const char* val) { snprintf(buf, size, "\"%s\"", val); }

/**
 * @brief Asserts a property on an integer, using a default print function.
 * @example PROPERTY_INT(my_number, is_even, "The number should be even");
 */
#define PROPERTY_INT(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_int, help_text)

/**
 * @brief Asserts a property on a character, using a default print function.
 * @example PROPERTY_CHAR(my_char, is_alpha, "The character should be alphabetic");
 */
#define PROPERTY_CHAR(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_char, help_text)

/**
 * @brief Asserts a property on a C-string, using a default print function.
 * @example PROPERTY_STRING(my_str, is_long, "The string should be long");
 */
#define PROPERTY_STRING(value, predicate_fn, help_text) PROPERTY(value, predicate_fn, _robust_test_print_string, help_text)

#ifdef MEMORY_TRACKING_ENABLED
/**
 * @brief Asserts that the total number of memory allocations matches `expected`.
 * @note Only available when MEMORY_TRACKING_ENABLED is defined.
 */
#define ASSERT_ALLOC_COUNT(expected) do { int e = (expected), a = _g_alloc_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, "_g_alloc_count == " #expected, e_buf, a_buf); } while(0)
/**
 * @brief Asserts that the total number of memory frees matches `expected`.
 * @note Only available when MEMORY_TRACKING_ENABLED is defined.
 */
#define ASSERT_FREE_COUNT(expected) do { int e = (expected), a = g_free_count; char e_buf[32], a_buf[32]; snprintf(e_buf, 32, "%d", e); snprintf(a_buf, 32, "%d", a); _ASSERT_GENERIC(e == a, "g_free_count == " #expected, e_buf, a_buf); } while(0)
#endif

/*============================================================================*/
/* SECTION 6: THE RUN_ALL_TESTS IMPLEMENTATION (FULL VERSION)                 */
/*============================================================================*/

int _run_all_tests_impl(int argc, char* argv[]);
#define RUN_ALL_TESTS() _run_all_tests_impl(argc, argv)

#ifdef _WIN32
static int _run_test_process_win(_TestInfo* test, const char* executable_path) {
    char command_line[2048];
    snprintf(command_line, sizeof(command_line), "\"%s\" --run_test \"%s\" \"%s\"", executable_path, test->suite_name, test->test_name);
    HANDLE h_read = NULL, h_write = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&h_read, &h_write, &sa, 0)) { fprintf(stderr, "    %s[ ERROR ]%s CreatePipe failed.\n", KRED, KNRM); return 0; }
    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.hStdError = h_write; si.hStdOutput = h_write; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "    %s[ ERROR ]%s CreateProcess failed (error %lu).\n", KRED, KNRM, GetLastError());
        CloseHandle(h_read); CloseHandle(h_write); return 0;
    }
    CloseHandle(h_write);
    DWORD wait_result = WaitForSingleObject(pi.hProcess, _TEST_TIMEOUT_SECONDS * 1000);
    char stderr_buffer[_STDERR_BUFFER_SIZE] = {0};
    DWORD bytes_read = 0; ReadFile(h_read, stderr_buffer, _STDERR_BUFFER_SIZE - 1, &bytes_read, NULL);
    int passed = 0;
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        fprintf(stderr, "    %s[ FAIL ] [TIMEOUT]%s Exceeded %d seconds.\n", KRED, KNRM, _TEST_TIMEOUT_SECONDS);
    } else {
        DWORD exit_code; GetExitCodeProcess(pi.hProcess, &exit_code);
        if (test->death_expect) {
            const _DeathExpect* de = test->death_expect;
            int msg_ok = !de->expected_msg || strstr(stderr_buffer, de->expected_msg);
            int exit_ok = de->expected_exit_code == -1 || exit_code == (DWORD)de->expected_exit_code;
            if (exit_code != 0 && msg_ok && exit_ok) {
                passed = 1; printf("    %s[ PASS ]%s Expected abnormal exit verified.\n", KGRN, KNRM);
            } else {
                fprintf(stderr, "    %s[ FAIL ]%s Death test criteria not met (exit code 0x%X).\n", KRED, KNRM, (unsigned int)exit_code);
                if (!msg_ok) fprintf(stderr, "      Expected message substring: \"%s\"\n", de->expected_msg);
                if (bytes_read > 0) fprintf(stderr, "      Got output:\n---\n%s---\n", stderr_buffer);
            }
        } else {
            if (exit_code == 0) {
                passed = 1; printf("    %s[ PASS ]%s\n", KGRN, KNRM);
            } else {
                fprintf(stderr, "    %s[ FAIL ]%s Exited with code 0x%X.\n", KRED, KNRM, (unsigned int)exit_code);
                if (bytes_read > 0) fprintf(stderr, "%s", stderr_buffer);
            }
        }
    }
    CloseHandle(h_read); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return passed;
}
#else
static int _wait_with_timeout(pid_t pid, int* status, int timeout_sec) { struct timespec start, now, sleep_ts = { .tv_sec = 0, .tv_nsec = 50 * 1000 * 1000 }; clock_gettime(CLOCK_MONOTONIC, &start); for (;;) { pid_t r = waitpid(pid, status, WNOHANG); if (r == pid) return 0; if (r == -1 && errno != EINTR) return -1; clock_gettime(CLOCK_MONOTONIC, &now); if ((now.tv_sec - start.tv_sec) >= timeout_sec) { kill(pid, SIGKILL); while (waitpid(pid, status, 0) == -1 && errno == EINTR); return 1; } nanosleep(&sleep_ts, NULL); } }
static int _run_test_process_posix(_TestInfo* test, const char* executable_path) {
    int err_pipe[2] = {-1, -1};
    if (pipe(err_pipe) == -1) { perror("pipe"); return 0; }
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); close(err_pipe[0]); close(err_pipe[1]); return 0; }
    if (pid == 0) {
        close(err_pipe[0]); dup2(err_pipe[1], STDOUT_FILENO); dup2(err_pipe[1], STDERR_FILENO); close(err_pipe[1]);
        char* child_argv[] = {(char*)executable_path, "--run_test", (char*)test->suite_name, (char*)test->test_name, NULL};
        execv(executable_path, child_argv);
        perror("execv failed"); exit(127);
    } else {
        close(err_pipe[1]); int status; int r = _wait_with_timeout(pid, &status, _TEST_TIMEOUT_SECONDS);
        char stderr_buffer[_STDERR_BUFFER_SIZE] = {0};
        ssize_t off = 0, bytes_read;
        int flags = fcntl(err_pipe[0], F_GETFL); fcntl(err_pipe[0], F_SETFL, flags | O_NONBLOCK);
        while ((bytes_read = read(err_pipe[0], stderr_buffer + off, _STDERR_BUFFER_SIZE - 1 - off)) > 0) { off += bytes_read; }
        close(err_pipe[0]);
        int passed = 0;
        if (r == 1) { fprintf(stderr, "    %s[ FAIL ] [TIMEOUT]%s Exceeded %d seconds.\n", KRED, KNRM, _TEST_TIMEOUT_SECONDS); }
        else if (r == -1) { perror("waitpid"); }
        else if (test->death_expect) {
            const _DeathExpect* de = test->death_expect;
            int sig_ok = 1, exit_ok = 1, msg_ok = 1;
            if (WIFSIGNALED(status)) { if(de->expected_signal != 0 && WTERMSIG(status) != de->expected_signal) sig_ok = 0; }
            else { if(de->expected_signal != 0) sig_ok = 0; }
            if (WIFEXITED(status)) { if(de->expected_exit_code != -1 && WEXITSTATUS(status) != de->expected_exit_code) exit_ok = 0; }
            else { if(de->expected_exit_code != -1) exit_ok = 0; }
            if(de->expected_msg && !strstr(stderr_buffer, de->expected_msg)) msg_ok = 0;
            if (sig_ok && exit_ok && msg_ok && (WIFSIGNALED(status) || WIFEXITED(status))) {
                passed = 1; printf("    %s[ PASS ]%s Expected abnormal exit verified.\n", KGRN, KNRM);
            } else {
                fprintf(stderr, "    %s[ FAIL ]%s Death test criteria not met.\n", KRED, KNRM);
                if (!msg_ok) fprintf(stderr, "      Expected message substring: \"%s\"\n", de->expected_msg);
                if (off > 0) fprintf(stderr, "      Got output:\n---\n%s---\n", stderr_buffer);
            }
        } else {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                passed = 1; printf("    %s[ PASS ]%s\n", KGRN, KNRM);
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "    %s[ FAIL ] [CRASH]%s Terminated by signal: %s.\n", KRED, KNRM, strsignal(WTERMSIG(status)));
                if (off > 0) fprintf(stderr, "%s", stderr_buffer);
            } else {
                fprintf(stderr, "    %s[ FAIL ]%s Exited with code %d.\n", KRED, KNRM, WEXITSTATUS(status));
                if (off > 0) fprintf(stderr, "%s", stderr_buffer);
            }
        }
        return passed;
    }
}
#endif

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
    int total = 0, passed = 0, failed = 0, suites = 0;
    int suite_total = 0, suite_failed = 0;
    char suite_results[_SUITE_RESULTS_BUFFER_SIZE];
    int suite_results_idx = 0;
    const char* current_suite = "";
    const char* executable_path = argv[0];
    _init_colors();
    _TestInfo* current_test = g_test_registry_head;
    while (current_test) {
        if (strcmp(current_suite, current_test->suite_name) != 0) {
            if (suite_total > 0) {
                suite_results[suite_results_idx] = '\0';
                printf("\n  %sDetails: %s%s%s\n", KBLU, suite_failed > 0 ? KRED : KGRN, suite_results, KNRM);
                printf("  Suite Summary for '%s': %d/%d passed.\n", current_suite, suite_total - suite_failed, suite_total);
            }
            current_suite = current_test->suite_name;
            printf("\n%s--- SUITE: %s ---%s\n", KBLU, current_suite, KNRM);
            suite_total = 0; suite_failed = 0; suite_results_idx = 0; suites++;
        }
        printf("  %s- TEST: %s%s%s\n", KYEL, current_test->test_name, current_test->death_expect ? " (death test)" : "", KNRM);
        fflush(stdout);
        #ifdef _WIN32
            int test_passed = _run_test_process_win(current_test, executable_path);
        #else
            int test_passed = _run_test_process_posix(current_test, executable_path);
        #endif
        if (test_passed) {
            passed++; if (suite_results_idx < _SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '+';
        } else {
            failed++; suite_failed++; if (suite_results_idx < _SUITE_RESULTS_BUFFER_SIZE - 1) suite_results[suite_results_idx++] = '-';
        }
        total++; suite_total++; current_test = current_test->next;
    }
    if (suite_total > 0) {
        suite_results[suite_results_idx] = '\0';
        printf("\n  %sDetails: %s%s%s\n", KBLU, suite_failed > 0 ? KRED : KGRN, suite_results, KNRM);
        printf("  Suite Summary for '%s': %d/%d passed.\n", current_suite, suite_total - suite_failed, suite_total);
    }
    double rate = total > 0 ? ((double)passed / total) * 100.0 : 100.0;
    printf("\n%s======================================\n", KBLU);
    printf(" Overall Summary\n");
    printf("======================================%s\n", KNRM);
    printf("Suites run:    %d\n", suites);
    printf("Total tests:   %d\n", total);
    printf("%sPassed:        %d%s\n", KGRN, passed, KNRM);
    printf("%sFailed:        %d%s\n", KRED, failed, KNRM);
    printf("Success rate:  %.2f%%\n", rate);
    printf("%s--------------------------------------%s\n", KBLU, KNRM);
    return failed > 0 ? 1 : 0;
}

#endif // ROBUST_TEST_H