/**
 * AIVory Monitor C Agent
 *
 * Remote debugging with AI-powered fix generation.
 *
 * Usage:
 *   #include <aivory/monitor.h>
 *
 *   int main() {
 *       aivory_config_t config = aivory_config_default();
 *       config.api_key = "your-api-key";
 *       config.environment = "production";
 *
 *       aivory_init(&config);
 *
 *       // Your code here...
 *       // Signals (SIGSEGV, SIGABRT) will be automatically captured
 *
 *       // Manual error capture
 *       aivory_capture_error("Error message", __FILE__, __LINE__);
 *
 *       aivory_shutdown();
 *       return 0;
 *   }
 */

#ifndef AIVORY_MONITOR_H
#define AIVORY_MONITOR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version */
#define AIVORY_VERSION_MAJOR 1
#define AIVORY_VERSION_MINOR 0
#define AIVORY_VERSION_PATCH 0
#define AIVORY_VERSION_STRING "1.0.0"

/* Forward declarations */
typedef struct aivory_config aivory_config_t;
typedef struct aivory_context aivory_context_t;
typedef struct aivory_exception aivory_exception_t;
typedef struct aivory_stack_frame aivory_stack_frame_t;

/**
 * Agent configuration.
 */
struct aivory_config {
    const char *api_key;           /* Required: AIVory API key */
    const char *backend_url;       /* Backend WebSocket URL (default: wss://api.aivory.net/monitor/agent) */
    const char *environment;       /* Environment name (default: production) */
    double sampling_rate;          /* Sampling rate 0.0-1.0 (default: 1.0) */
    int max_capture_depth;         /* Max variable capture depth (default: 3) */
    int max_string_length;         /* Max string length (default: 1000) */
    int max_collection_size;       /* Max collection size (default: 100) */
    bool debug;                    /* Enable debug logging (default: false) */
    bool capture_signals;          /* Capture signal errors (default: true) */
};

/**
 * Stack frame information.
 */
struct aivory_stack_frame {
    const char *function_name;
    const char *file_name;
    const char *file_path;
    int line_number;
    bool is_native;
    bool source_available;
};

/**
 * Captured exception.
 */
struct aivory_exception {
    char id[40];
    char *exception_type;
    char *message;
    char fingerprint[20];
    aivory_stack_frame_t *stack_trace;
    int stack_trace_count;
    char *captured_at;
};

/**
 * Returns a default configuration.
 */
aivory_config_t aivory_config_default(void);

/**
 * Initializes the AIVory Monitor agent.
 *
 * @param config Agent configuration
 * @return 0 on success, -1 on error
 */
int aivory_init(const aivory_config_t *config);

/**
 * Shuts down the agent.
 */
void aivory_shutdown(void);

/**
 * Checks if the agent is initialized.
 *
 * @return true if initialized
 */
bool aivory_is_initialized(void);

/**
 * Captures an error.
 *
 * @param message Error message
 * @param file Source file (use __FILE__)
 * @param line Source line (use __LINE__)
 */
void aivory_capture_error(const char *message, const char *file, int line);

/**
 * Captures an error with additional context.
 *
 * @param message Error message
 * @param file Source file
 * @param line Source line
 * @param context_json Additional context as JSON string
 */
void aivory_capture_error_with_context(const char *message, const char *file, int line,
                                       const char *context_json);

/**
 * Sets custom context that will be sent with all captures.
 *
 * @param context_json Context as JSON string
 */
void aivory_set_context(const char *context_json);

/**
 * Sets user information.
 *
 * @param user_id User ID (can be NULL)
 * @param email Email (can be NULL)
 * @param username Username (can be NULL)
 */
void aivory_set_user(const char *user_id, const char *email, const char *username);

/**
 * Clears user information.
 */
void aivory_clear_user(void);

/* Convenience macros */
#define AIVORY_CAPTURE_ERROR(msg) \
    aivory_capture_error((msg), __FILE__, __LINE__)

#define AIVORY_CAPTURE_ERROR_CTX(msg, ctx) \
    aivory_capture_error_with_context((msg), __FILE__, __LINE__, (ctx))

#ifdef __cplusplus
}
#endif

#endif /* AIVORY_MONITOR_H */
