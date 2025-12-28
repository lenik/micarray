#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    const char *name;
    const char *executable;
} test_case_t;

static test_case_t test_cases[] = {
    {"Configuration Parser", "./test_config"},
    {"Noise Reduction", "./test_noise_reduction"},
    {"Localization", "./test_localization"},
    {"Logging System", "./test_logging"},
    {"Library Integration", "./test_libmicarray"}
};

static const int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

static int run_test(const char *test_name, const char *executable) {
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Running: %s\n", test_name);
    printf("" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl(executable, executable, (char*)NULL);
        perror("execl failed");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                printf("\n✅ %s: PASSED\n", test_name);
                return 0;
            } else {
                printf("\n❌ %s: FAILED (exit code: %d)\n", test_name, exit_code);
                return 1;
            }
        } else {
            printf("\n❌ %s: FAILED (abnormal termination)\n", test_name);
            return 1;
        }
    } else {
        perror("fork failed");
        return 1;
    }
}

int main(int argc, char *argv[]) {
    printf("libmicarray Test Suite\n");
    printf("=====================\n");
    printf("Running %d test modules...\n", num_tests);
    
    int passed = 0;
    int failed = 0;
    
    // Check if specific test was requested
    if (argc > 1) {
        const char *requested_test = argv[1];
        
        for (int i = 0; i < num_tests; i++) {
            if (strstr(test_cases[i].name, requested_test) != NULL ||
                strstr(test_cases[i].executable, requested_test) != NULL) {
                
                if (run_test(test_cases[i].name, test_cases[i].executable) == 0) {
                    passed++;
                } else {
                    failed++;
                }
                break;
            }
        }
        
        if (passed == 0 && failed == 0) {
            printf("No test matching '%s' found.\n", requested_test);
            printf("Available tests:\n");
            for (int i = 0; i < num_tests; i++) {
                printf("  - %s (%s)\n", test_cases[i].name, test_cases[i].executable);
            }
            return 1;
        }
    } else {
        // Run all tests
        for (int i = 0; i < num_tests; i++) {
            if (run_test(test_cases[i].name, test_cases[i].executable) == 0) {
                passed++;
            } else {
                failed++;
            }
        }
    }
    
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("TEST SUMMARY\n");
    printf("" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Total tests: %d\n", passed + failed);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    
    if (failed == 0) {
        printf("\n🎉 All tests passed!\n");
        return 0;
    } else {
        printf("\n💥 Some tests failed!\n");
        return 1;
    }
}
