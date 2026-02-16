/**
 * AIVory C Agent Test Application
 *
 * Generates various exception-like conditions to test signal capture and stack traces.
 * Note: C doesn't have runtime exceptions, so we simulate via signal handlers.
 *
 * Usage:
 *   cd monitor-agents/agent-c
 *   mkdir -p build && cd build
 *   cmake .. && make
 *   AIVORY_API_KEY=test-key-123 AIVORY_BACKEND_URL=ws://localhost:19999/api/monitor/agent/v1 AIVORY_DEBUG=true ./examples/test_app
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "aivory/agent.h"

typedef struct {
    char user_id[64];
    char email[128];
    int active;
} UserContext;

void test_manual_error(int iteration) {
    // Create some local variables
    char test_var[64];
    snprintf(test_var, sizeof(test_var), "test-value-%d", iteration);
    int count = iteration * 10;
    const char* items[] = {"apple", "banana", "cherry"};
    UserContext user = {.active = 1};
    snprintf(user.user_id, sizeof(user.user_id), "user-%d", iteration);
    strcpy(user.email, "test@example.com");

    // Use variables to prevent unused warnings
    (void)count;
    (void)items;
    (void)user;

    printf("Triggering manual error report...\n");
    printf("Local variables: test_var=%s, count=%d\n", test_var, count);

    // Report a manual error
    aivory_report_error("TestError", "Manual test error", NULL);
}

void test_signal_error(int iteration) {
    // This would typically cause SIGSEGV, but we'll report it manually for safety
    char test_var[64];
    snprintf(test_var, sizeof(test_var), "test-value-%d", iteration);

    printf("Simulating segmentation fault scenario...\n");
    printf("In a real scenario, accessing NULL would trigger SIGSEGV\n");

    // Report simulated error
    aivory_report_error("SIGSEGV", "Simulated segmentation fault", NULL);
}

void test_abort_error(int iteration) {
    char test_var[64];
    snprintf(test_var, sizeof(test_var), "test-value-%d", iteration);

    printf("Simulating abort scenario...\n");
    printf("In a real scenario, failed assertion would trigger SIGABRT\n");

    // Report simulated error
    aivory_report_error("SIGABRT", "Simulated abort from assertion failure", NULL);
}

int main(int argc, char* argv[]) {
    printf("===========================================\n");
    printf("AIVory C Agent Test Application\n");
    printf("===========================================\n");

    // Initialize the agent
    aivory_config_t config = {
        .api_key = getenv("AIVORY_API_KEY") ? getenv("AIVORY_API_KEY") : "test-key-123",
        .backend_url = getenv("AIVORY_BACKEND_URL") ? getenv("AIVORY_BACKEND_URL") : "ws://localhost:19999/api/monitor/agent/v1",
        .environment = getenv("AIVORY_ENVIRONMENT") ? getenv("AIVORY_ENVIRONMENT") : "development",
        .debug = getenv("AIVORY_DEBUG") && strcmp(getenv("AIVORY_DEBUG"), "true") == 0
    };

    if (aivory_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize AIVory agent\n");
        return 1;
    }

    // Set user context
    aivory_set_user("test-user-001", "tester@example.com", "tester");

    // Wait for agent to connect
    printf("Waiting for agent to connect...\n");
    sleep(3);
    printf("Starting exception tests...\n\n");

    // Test 1: Manual error
    printf("--- Test 1 ---\n");
    test_manual_error(0);
    printf("Error reported\n\n");
    sleep(3);

    // Test 2: Simulated SIGSEGV
    printf("--- Test 2 ---\n");
    test_signal_error(1);
    printf("Error reported\n\n");
    sleep(3);

    // Test 3: Simulated SIGABRT
    printf("--- Test 3 ---\n");
    test_abort_error(2);
    printf("Error reported\n\n");
    sleep(3);

    printf("===========================================\n");
    printf("Test complete. Check database for exceptions.\n");
    printf("Note: C captures stack traces but not local variables.\n");
    printf("===========================================\n");

    // Keep running briefly to allow final messages to send
    sleep(2);

    // Shutdown cleanly
    aivory_shutdown();

    return 0;
}
