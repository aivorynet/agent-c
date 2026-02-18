/**
 * WebSocket connection to AIVory backend using libwebsockets.
 */

#include "aivory/config.h"
#include "aivory/types.h"

#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* Max message size */
#define MAX_MESSAGE_SIZE 65536

/* Connection state */
typedef struct {
    aivory_connection_state_t state;
    pthread_t thread;
    pthread_mutex_t mutex;
    aivory_queue_t queue;
    int reconnect_attempts;
    bool should_stop;
    bool authenticated;

    /* libwebsockets context and connection */
    struct lws_context *lws_ctx;
    struct lws *wsi;

    /* Send buffer */
    unsigned char *send_buffer;
    size_t send_buffer_len;
    bool has_pending_send;

    /* Agent reference */
    aivory_agent_t *agent;

    /* Heartbeat tracking */
    time_t last_heartbeat;
} ws_connection_t;

static ws_connection_t *g_connection = NULL;

/* Forward declarations */
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len);

/* Protocol definition */
static struct lws_protocols protocols[] = {
    {
        "aivory-monitor",
        ws_callback,
        0,
        MAX_MESSAGE_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

/* Queue operations */
static void queue_init(aivory_queue_t *queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_size = AIVORY_MESSAGE_QUEUE_SIZE;
    queue->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t *)queue->mutex, NULL);
}

static void queue_destroy(aivory_queue_t *queue) {
    pthread_mutex_lock((pthread_mutex_t *)queue->mutex);

    aivory_queue_entry_t *entry = queue->head;
    while (entry) {
        aivory_queue_entry_t *next = entry->next;
        free(entry->json);
        free(entry);
        entry = next;
    }

    pthread_mutex_unlock((pthread_mutex_t *)queue->mutex);
    pthread_mutex_destroy((pthread_mutex_t *)queue->mutex);
    free(queue->mutex);
}

static void queue_push(aivory_queue_t *queue, const char *json) {
    pthread_mutex_lock((pthread_mutex_t *)queue->mutex);

    /* If queue is full, remove oldest */
    if (queue->count >= queue->max_size && queue->head) {
        aivory_queue_entry_t *old = queue->head;
        queue->head = old->next;
        if (!queue->head) {
            queue->tail = NULL;
        }
        free(old->json);
        free(old);
        queue->count--;
    }

    aivory_queue_entry_t *entry = malloc(sizeof(aivory_queue_entry_t));
    if (entry) {
        entry->json = strdup(json);
        entry->next = NULL;

        if (queue->tail) {
            queue->tail->next = entry;
        } else {
            queue->head = entry;
        }
        queue->tail = entry;
        queue->count++;
    }

    pthread_mutex_unlock((pthread_mutex_t *)queue->mutex);
}

static char *queue_pop(aivory_queue_t *queue) {
    pthread_mutex_lock((pthread_mutex_t *)queue->mutex);

    char *json = NULL;
    if (queue->head) {
        aivory_queue_entry_t *entry = queue->head;
        queue->head = entry->next;
        if (!queue->head) {
            queue->tail = NULL;
        }
        json = entry->json;
        free(entry);
        queue->count--;
    }

    pthread_mutex_unlock((pthread_mutex_t *)queue->mutex);
    return json;
}

static bool queue_has_messages(aivory_queue_t *queue) {
    pthread_mutex_lock((pthread_mutex_t *)queue->mutex);
    bool has_messages = (queue->count > 0);
    pthread_mutex_unlock((pthread_mutex_t *)queue->mutex);
    return has_messages;
}

/* Build registration message */
static char *build_register_message(aivory_agent_t *agent) {
    char *json = malloc(2048);
    if (!json) return NULL;

    snprintf(json, 2048,
        "{\"type\":\"register\",\"payload\":{"
        "\"api_key\":\"%s\","
        "\"agent_id\":\"%s\","
        "\"hostname\":\"%s\","
        "\"environment\":\"%s\","
        "\"agent_version\":\"1.0.0\","
        "\"runtime\":\"c\","
        "\"runtime_version\":\"C11\","
        "\"platform\":\"%s\","
        "\"arch\":\"%s\""
        "},\"timestamp\":%ld}",
        agent->config.api_key,
        agent->agent_id,
        agent->hostname,
        agent->config.environment,
#ifdef __linux__
        "linux",
#elif defined(__APPLE__)
        "darwin",
#elif defined(_WIN32)
        "windows",
#else
        "unknown",
#endif
#ifdef __x86_64__
        "x64",
#elif defined(__i386__)
        "x86",
#elif defined(__aarch64__)
        "arm64",
#elif defined(__arm__)
        "arm",
#else
        "unknown",
#endif
        (long)time(NULL) * 1000
    );

    return json;
}

/* Build heartbeat message */
static char *build_heartbeat_message(void) {
    char *json = malloc(256);
    if (!json) return NULL;

    snprintf(json, 256,
        "{\"type\":\"heartbeat\",\"payload\":{\"timestamp\":%ld},\"timestamp\":%ld}",
        (long)time(NULL) * 1000,
        (long)time(NULL) * 1000
    );

    return json;
}

/* Send a message over WebSocket */
static int ws_send_message(const char *json) {
    if (!g_connection || !g_connection->wsi || !json) {
        return -1;
    }

    size_t len = strlen(json);
    if (len > MAX_MESSAGE_SIZE - LWS_PRE) {
        return -1;
    }

    /* Allocate buffer with LWS_PRE padding */
    unsigned char *buf = malloc(LWS_PRE + len);
    if (!buf) {
        return -1;
    }

    memcpy(buf + LWS_PRE, json, len);

    int written = lws_write(g_connection->wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    free(buf);

    return (written >= 0) ? 0 : -1;
}

/* WebSocket callback */
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    (void)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            if (g_connection) {
                pthread_mutex_lock(&g_connection->mutex);
                g_connection->state = AIVORY_CONN_CONNECTED;
                g_connection->wsi = wsi;
                pthread_mutex_unlock(&g_connection->mutex);

                if (g_connection->agent && g_connection->agent->config.debug) {
                    fprintf(stderr, "[AIVory Monitor] WebSocket connected\n");
                }

                /* Send registration message */
                char *reg_msg = build_register_message(g_connection->agent);
                if (reg_msg) {
                    ws_send_message(reg_msg);
                    free(reg_msg);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (g_connection && in && len > 0) {
                /* Parse message type */
                char *msg = malloc(len + 1);
                if (msg) {
                    memcpy(msg, in, len);
                    msg[len] = '\0';

                    /* Simple JSON parsing for msg type */
                    if (strstr(msg, "\"registered\"") || strstr(msg, "\"type\":\"registered\"")) {
                        pthread_mutex_lock(&g_connection->mutex);
                        g_connection->authenticated = true;
                        g_connection->state = AIVORY_CONN_AUTHENTICATED;
                        pthread_mutex_unlock(&g_connection->mutex);

                        if (g_connection->agent && g_connection->agent->config.debug) {
                            fprintf(stderr, "[AIVory Monitor] Agent registered\n");
                        }

                        /* Send queued messages */
                        char *queued;
                        while ((queued = queue_pop(&g_connection->queue)) != NULL) {
                            ws_send_message(queued);
                            free(queued);
                        }
                    } else if (strstr(msg, "\"error\"")) {
                        if (strstr(msg, "auth_error") || strstr(msg, "invalid_api_key")) {
                            fprintf(stderr, "[AIVory Monitor] Authentication failed\n");
                            pthread_mutex_lock(&g_connection->mutex);
                            g_connection->should_stop = true;
                            pthread_mutex_unlock(&g_connection->mutex);
                        }
                    }

                    free(msg);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (g_connection && g_connection->authenticated) {
                /* Send any queued messages */
                char *json = queue_pop(&g_connection->queue);
                if (json) {
                    ws_send_message(json);
                    free(json);

                    /* Request another callback if more messages */
                    if (queue_has_messages(&g_connection->queue)) {
                        lws_callback_on_writable(wsi);
                    }
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (g_connection) {
                pthread_mutex_lock(&g_connection->mutex);
                g_connection->state = AIVORY_CONN_DISCONNECTED;
                g_connection->wsi = NULL;
                g_connection->authenticated = false;
                pthread_mutex_unlock(&g_connection->mutex);

                if (g_connection->agent && g_connection->agent->config.debug) {
                    fprintf(stderr, "[AIVory Monitor] Connection error: %s\n",
                            in ? (char *)in : "unknown");
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            if (g_connection) {
                pthread_mutex_lock(&g_connection->mutex);
                g_connection->state = AIVORY_CONN_DISCONNECTED;
                g_connection->wsi = NULL;
                g_connection->authenticated = false;
                pthread_mutex_unlock(&g_connection->mutex);

                if (g_connection->agent && g_connection->agent->config.debug) {
                    fprintf(stderr, "[AIVory Monitor] WebSocket closed\n");
                }
            }
            break;

        default:
            break;
    }

    return 0;
}

/* Parse URL into components */
static int parse_url(const char *url, char *host, int *port, char *path, bool *use_ssl) {
    *use_ssl = false;
    *port = 80;
    strcpy(path, "/");

    const char *p = url;

    if (strncmp(p, "wss://", 6) == 0) {
        *use_ssl = true;
        *port = 443;
        p += 6;
    } else if (strncmp(p, "ws://", 5) == 0) {
        p += 5;
    } else {
        return -1;
    }

    /* Find end of host (: or / or end) */
    const char *host_end = p;
    while (*host_end && *host_end != ':' && *host_end != '/') {
        host_end++;
    }

    size_t host_len = host_end - p;
    if (host_len >= 256) return -1;
    strncpy(host, p, host_len);
    host[host_len] = '\0';

    p = host_end;

    /* Parse port if present */
    if (*p == ':') {
        p++;
        *port = atoi(p);
        while (*p && *p != '/') p++;
    }

    /* Parse path if present */
    if (*p == '/') {
        strncpy(path, p, 512);
        path[511] = '\0';
    }

    return 0;
}

/* WebSocket thread */
static void *ws_thread_func(void *arg) {
    aivory_agent_t *agent = (aivory_agent_t *)arg;
    ws_connection_t *conn = g_connection;

    char host[256];
    char path[512];
    int port;
    bool use_ssl;

    if (parse_url(agent->config.backend_url, host, &port, path, &use_ssl) != 0) {
        fprintf(stderr, "[AIVory Monitor] Invalid backend URL\n");
        return NULL;
    }

    while (!conn->should_stop) {
        pthread_mutex_lock(&conn->mutex);
        aivory_connection_state_t state = conn->state;
        pthread_mutex_unlock(&conn->mutex);

        if (state == AIVORY_CONN_DISCONNECTED) {
            if (agent->config.debug) {
                fprintf(stderr, "[AIVory Monitor] Connecting to %s\n", agent->config.backend_url);
            }

            /* Create libwebsockets context */
            struct lws_context_creation_info ctx_info;
            memset(&ctx_info, 0, sizeof(ctx_info));
            ctx_info.port = CONTEXT_PORT_NO_LISTEN;
            ctx_info.protocols = protocols;
            ctx_info.gid = -1;
            ctx_info.uid = -1;

            if (use_ssl) {
                ctx_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
            }

            conn->lws_ctx = lws_create_context(&ctx_info);
            if (!conn->lws_ctx) {
                fprintf(stderr, "[AIVory Monitor] Failed to create lws context\n");
                goto reconnect;
            }

            /* Connect to server */
            struct lws_client_connect_info conn_info;
            memset(&conn_info, 0, sizeof(conn_info));
            conn_info.context = conn->lws_ctx;
            conn_info.address = host;
            conn_info.port = port;
            conn_info.path = path;
            conn_info.host = host;
            conn_info.origin = host;
            conn_info.protocol = "aivory-monitor";

            if (use_ssl) {
                conn_info.ssl_connection = LCCSCF_USE_SSL |
                                          LCCSCF_ALLOW_SELFSIGNED |
                                          LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
            }

            struct lws *wsi = lws_client_connect_via_info(&conn_info);
            if (!wsi) {
                fprintf(stderr, "[AIVory Monitor] Failed to connect\n");
                lws_context_destroy(conn->lws_ctx);
                conn->lws_ctx = NULL;
                goto reconnect;
            }

            /* Service loop */
            time_t last_heartbeat = time(NULL);
            while (!conn->should_stop && conn->state != AIVORY_CONN_DISCONNECTED) {
                lws_service(conn->lws_ctx, 100);

                /* Send heartbeat every 30 seconds */
                time_t now = time(NULL);
                if (conn->authenticated && (now - last_heartbeat) >= 30) {
                    char *heartbeat = build_heartbeat_message();
                    if (heartbeat) {
                        ws_send_message(heartbeat);
                        free(heartbeat);
                    }
                    last_heartbeat = now;
                }
            }

            /* Cleanup */
            if (conn->lws_ctx) {
                lws_context_destroy(conn->lws_ctx);
                conn->lws_ctx = NULL;
            }
        }

reconnect:
        if (conn->should_stop) break;

        conn->reconnect_attempts++;
        if (conn->reconnect_attempts > 10) {
            fprintf(stderr, "[AIVory Monitor] Max reconnect attempts reached\n");
            break;
        }

        /* Exponential backoff */
        int delay = 1 << (conn->reconnect_attempts > 6 ? 6 : conn->reconnect_attempts);
        if (agent->config.debug) {
            fprintf(stderr, "[AIVory Monitor] Reconnecting in %d seconds (attempt %d)\n",
                    delay, conn->reconnect_attempts);
        }
        sleep(delay);
    }

    return NULL;
}

int aivory_ws_connect(aivory_agent_t *agent) {
    if (g_connection) {
        return 0;
    }

    g_connection = calloc(1, sizeof(ws_connection_t));
    if (!g_connection) {
        return -1;
    }

    pthread_mutex_init(&g_connection->mutex, NULL);
    queue_init(&g_connection->queue);
    g_connection->state = AIVORY_CONN_DISCONNECTED;
    g_connection->should_stop = false;
    g_connection->authenticated = false;
    g_connection->agent = agent;

    if (pthread_create(&g_connection->thread, NULL, ws_thread_func, agent) != 0) {
        queue_destroy(&g_connection->queue);
        pthread_mutex_destroy(&g_connection->mutex);
        free(g_connection);
        g_connection = NULL;
        return -1;
    }

    agent->connection = g_connection;
    return 0;
}

void aivory_ws_disconnect(aivory_agent_t *agent) {
    (void)agent;

    if (!g_connection) {
        return;
    }

    g_connection->should_stop = true;
    pthread_join(g_connection->thread, NULL);

    queue_destroy(&g_connection->queue);
    pthread_mutex_destroy(&g_connection->mutex);
    free(g_connection);
    g_connection = NULL;
}

void aivory_ws_send_exception(aivory_agent_t *agent, const char *json) {
    if (!g_connection || !json) {
        return;
    }

    pthread_mutex_lock(&g_connection->mutex);
    aivory_connection_state_t state = g_connection->state;
    bool authenticated = g_connection->authenticated;
    pthread_mutex_unlock(&g_connection->mutex);

    if (state == AIVORY_CONN_AUTHENTICATED && authenticated) {
        /* Send directly */
        ws_send_message(json);

        /* Request writable callback for any additional queued messages */
        if (g_connection->wsi) {
            lws_callback_on_writable(g_connection->wsi);
        }
    } else {
        /* Queue for later */
        queue_push(&g_connection->queue, json);

        if (agent->config.debug) {
            fprintf(stderr, "[AIVory Monitor] Message queued (not authenticated)\n");
        }
    }
}
