/**
 * Main agent implementation.
 */

#include "aivory/monitor.h"
#include "aivory/config.h"
#include "aivory/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

/* Global agent instance */
static aivory_agent_t g_agent = {0};
static pthread_mutex_t g_agent_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
extern int aivory_ws_connect(aivory_agent_t *agent);
extern void aivory_ws_disconnect(aivory_agent_t *agent);
extern void aivory_ws_send_exception(aivory_agent_t *agent, const char *json);
extern void aivory_install_signal_handlers(aivory_agent_t *agent);
extern void aivory_uninstall_signal_handlers(void);
extern char *aivory_capture_backtrace(int skip);
extern char *aivory_calculate_fingerprint(const char *type, const char *backtrace);

aivory_config_t aivory_config_default(void) {
    aivory_config_t config = {0};

    /* Load from environment or use defaults */
    const char *env_key = getenv(AIVORY_ENV_API_KEY);
    config.api_key = env_key ? env_key : "";

    const char *env_url = getenv(AIVORY_ENV_BACKEND_URL);
    config.backend_url = env_url ? env_url : AIVORY_DEFAULT_BACKEND_URL;

    const char *env_env = getenv(AIVORY_ENV_ENVIRONMENT);
    config.environment = env_env ? env_env : AIVORY_DEFAULT_ENVIRONMENT;

    const char *env_rate = getenv(AIVORY_ENV_SAMPLING_RATE);
    config.sampling_rate = env_rate ? atof(env_rate) : AIVORY_DEFAULT_SAMPLING_RATE;

    config.max_capture_depth = AIVORY_DEFAULT_MAX_CAPTURE_DEPTH;
    config.max_string_length = AIVORY_DEFAULT_MAX_STRING_LENGTH;
    config.max_collection_size = AIVORY_DEFAULT_MAX_COLLECTION_SIZE;

    const char *env_debug = getenv(AIVORY_ENV_DEBUG);
    config.debug = env_debug && strcmp(env_debug, "true") == 0;

    config.capture_signals = true;

    return config;
}

int aivory_init(const aivory_config_t *config) {
    pthread_mutex_lock(&g_agent_mutex);

    if (g_agent.initialized) {
        fprintf(stderr, "[AIVory Monitor] Agent already initialized\n");
        pthread_mutex_unlock(&g_agent_mutex);
        return -1;
    }

    if (!config || !config->api_key || config->api_key[0] == '\0') {
        fprintf(stderr, "[AIVory Monitor] API key is required\n");
        pthread_mutex_unlock(&g_agent_mutex);
        return -1;
    }

    /* Copy configuration */
    memcpy(&g_agent.config, config, sizeof(aivory_config_t));

    /* Generate agent ID */
    g_agent.agent_id = aivory_generate_agent_id();
    g_agent.hostname = aivory_get_hostname();

    /* Install signal handlers if enabled */
    if (config->capture_signals) {
        aivory_install_signal_handlers(&g_agent);
    }

    /* Connect to backend */
    if (aivory_ws_connect(&g_agent) != 0) {
        fprintf(stderr, "[AIVory Monitor] Failed to connect to backend\n");
        /* Continue anyway, will retry */
    }

    g_agent.initialized = true;

    printf("[AIVory Monitor] Agent v%s initialized\n", AIVORY_VERSION_STRING);
    printf("[AIVory Monitor] Environment: %s\n", config->environment);

    pthread_mutex_unlock(&g_agent_mutex);
    return 0;
}

void aivory_shutdown(void) {
    pthread_mutex_lock(&g_agent_mutex);

    if (!g_agent.initialized) {
        pthread_mutex_unlock(&g_agent_mutex);
        return;
    }

    printf("[AIVory Monitor] Shutting down agent\n");

    /* Uninstall signal handlers */
    aivory_uninstall_signal_handlers();

    /* Disconnect */
    aivory_ws_disconnect(&g_agent);

    /* Free resources */
    free(g_agent.agent_id);
    free(g_agent.hostname);
    free(g_agent.custom_context);
    free(g_agent.user_json);

    memset(&g_agent, 0, sizeof(g_agent));

    pthread_mutex_unlock(&g_agent_mutex);
}

bool aivory_is_initialized(void) {
    return g_agent.initialized;
}

void aivory_capture_error(const char *message, const char *file, int line) {
    aivory_capture_error_with_context(message, file, line, NULL);
}

void aivory_capture_error_with_context(const char *message, const char *file, int line,
                                       const char *context_json) {
    if (!g_agent.initialized) {
        return;
    }

    if (!aivory_should_sample(&g_agent.config)) {
        return;
    }

    /* Capture backtrace */
    char *backtrace = aivory_capture_backtrace(2); /* Skip this function and caller */

    /* Calculate fingerprint */
    char *fingerprint = aivory_calculate_fingerprint("Error", backtrace);

    /* Get timestamp */
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    /* Build JSON */
    char *json = malloc(65536);
    if (!json) {
        free(backtrace);
        free(fingerprint);
        return;
    }

    int len = snprintf(json, 65536,
        "{"
        "\"type\":\"exception\","
        "\"payload\":{"
        "\"id\":\"%s\","
        "\"exception_type\":\"Error\","
        "\"message\":\"%s\","
        "\"fingerprint\":\"%s\","
        "\"stack_trace\":%s,"
        "\"local_variables\":{},"
        "\"context\":{%s%s%s%s},"
        "\"captured_at\":\"%s\","
        "\"agent_id\":\"%s\","
        "\"environment\":\"%s\","
        "\"runtime_info\":{"
        "\"runtime\":\"c\","
        "\"platform\":\"%s\","
        "\"arch\":\"%s\""
        "}"
        "},"
        "\"timestamp\":%ld"
        "}",
        g_agent.agent_id,
        message ? message : "",
        fingerprint ? fingerprint : "",
        backtrace ? backtrace : "[]",
        file ? "\"file\":\"" : "",
        file ? file : "",
        file ? "\"," : "",
        context_json ? context_json : "",
        timestamp,
        g_agent.agent_id,
        g_agent.config.environment,
#ifdef __linux__
        "linux",
#elif __APPLE__
        "darwin",
#elif _WIN32
        "windows",
#else
        "unknown",
#endif
#ifdef __x86_64__
        "x86_64",
#elif __aarch64__
        "arm64",
#elif __i386__
        "x86",
#else
        "unknown",
#endif
        (long)time(NULL) * 1000
    );

    if (len > 0 && len < 65536) {
        aivory_ws_send_exception(&g_agent, json);
    }

    free(json);
    free(backtrace);
    free(fingerprint);
}

void aivory_set_context(const char *context_json) {
    pthread_mutex_lock(&g_agent_mutex);

    free(g_agent.custom_context);
    g_agent.custom_context = context_json ? strdup(context_json) : NULL;

    pthread_mutex_unlock(&g_agent_mutex);
}

void aivory_set_user(const char *user_id, const char *email, const char *username) {
    pthread_mutex_lock(&g_agent_mutex);

    free(g_agent.user_json);

    if (user_id || email || username) {
        char *json = malloc(1024);
        if (json) {
            int len = snprintf(json, 1024, "{");
            if (user_id) {
                len += snprintf(json + len, 1024 - len, "\"id\":\"%s\",", user_id);
            }
            if (email) {
                len += snprintf(json + len, 1024 - len, "\"email\":\"%s\",", email);
            }
            if (username) {
                len += snprintf(json + len, 1024 - len, "\"username\":\"%s\",", username);
            }
            /* Remove trailing comma */
            if (json[len - 1] == ',') {
                json[len - 1] = '}';
            } else {
                json[len] = '}';
                json[len + 1] = '\0';
            }
            g_agent.user_json = json;
        }
    } else {
        g_agent.user_json = NULL;
    }

    pthread_mutex_unlock(&g_agent_mutex);
}

void aivory_clear_user(void) {
    aivory_set_user(NULL, NULL, NULL);
}

aivory_agent_t *aivory_get_agent(void) {
    return &g_agent;
}

char *aivory_generate_agent_id(void) {
    char *id = malloc(64);
    if (id) {
        time_t now = time(NULL);
        unsigned int rand_val;

#ifdef __linux__
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) {
            fread(&rand_val, sizeof(rand_val), 1, f);
            fclose(f);
        } else {
            rand_val = (unsigned int)now ^ (unsigned int)getpid();
        }
#else
        rand_val = (unsigned int)now ^ (unsigned int)getpid();
#endif

        snprintf(id, 64, "agent-%lx-%08x", (long)now, rand_val);
    }
    return id;
}

char *aivory_get_hostname(void) {
    char *hostname = malloc(256);
    if (hostname) {
        if (gethostname(hostname, 256) != 0) {
            strcpy(hostname, "unknown");
        }
    }
    return hostname;
}

bool aivory_should_sample(const aivory_config_t *config) {
    if (config->sampling_rate >= 1.0) {
        return true;
    }
    if (config->sampling_rate <= 0.0) {
        return false;
    }

    /* Simple random sampling */
    return ((double)rand() / RAND_MAX) < config->sampling_rate;
}
