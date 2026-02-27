/**
 * Internal configuration definitions.
 */

#ifndef AIVORY_CONFIG_H
#define AIVORY_CONFIG_H

#include "monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default values */
#define AIVORY_DEFAULT_BACKEND_URL "wss://api.aivory.net/monitor/agent"
#define AIVORY_DEFAULT_ENVIRONMENT "production"
#define AIVORY_DEFAULT_SAMPLING_RATE 1.0
#define AIVORY_DEFAULT_MAX_CAPTURE_DEPTH 10
#define AIVORY_DEFAULT_MAX_STRING_LENGTH 1000
#define AIVORY_DEFAULT_MAX_COLLECTION_SIZE 100
#define AIVORY_DEFAULT_HEARTBEAT_INTERVAL 30000 /* ms */
#define AIVORY_DEFAULT_RECONNECT_DELAY 1000 /* ms */
#define AIVORY_MAX_RECONNECT_ATTEMPTS 10
#define AIVORY_MAX_STACK_FRAMES 50
#define AIVORY_MESSAGE_QUEUE_SIZE 100

/* Environment variable names */
#define AIVORY_ENV_API_KEY "AIVORY_API_KEY"
#define AIVORY_ENV_BACKEND_URL "AIVORY_BACKEND_URL"
#define AIVORY_ENV_ENVIRONMENT "AIVORY_ENVIRONMENT"
#define AIVORY_ENV_SAMPLING_RATE "AIVORY_SAMPLING_RATE"
#define AIVORY_ENV_DEBUG "AIVORY_DEBUG"

/**
 * Internal agent state.
 */
typedef struct aivory_agent {
    aivory_config_t config;
    char *agent_id;
    char *hostname;
    char *custom_context;
    char *user_json;
    void *connection; /* websocket connection */
    bool initialized;
    bool connected;
} aivory_agent_t;

/**
 * Get the global agent instance.
 */
aivory_agent_t *aivory_get_agent(void);

/**
 * Generate a unique agent ID.
 */
char *aivory_generate_agent_id(void);

/**
 * Get the system hostname.
 */
char *aivory_get_hostname(void);

/**
 * Check if event should be sampled.
 */
bool aivory_should_sample(const aivory_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AIVORY_CONFIG_H */
