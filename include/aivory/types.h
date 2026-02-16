/**
 * Internal type definitions.
 */

#ifndef AIVORY_TYPES_H
#define AIVORY_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message types */
typedef enum {
    AIVORY_MSG_REGISTER,
    AIVORY_MSG_EXCEPTION,
    AIVORY_MSG_HEARTBEAT,
    AIVORY_MSG_BREAKPOINT_HIT
} aivory_message_type_t;

/* Connection state */
typedef enum {
    AIVORY_CONN_DISCONNECTED,
    AIVORY_CONN_CONNECTING,
    AIVORY_CONN_CONNECTED,
    AIVORY_CONN_AUTHENTICATED
} aivory_connection_state_t;

/* Runtime info */
typedef struct {
    const char *runtime;
    const char *runtime_version;
    const char *platform;
    const char *arch;
} aivory_runtime_info_t;

/* Captured variable */
typedef struct aivory_variable {
    char *name;
    char *type;
    char *value;
    bool is_null;
    bool is_truncated;
    struct aivory_variable *children;
    int children_count;
} aivory_variable_t;

/* Message queue entry */
typedef struct aivory_queue_entry {
    char *json;
    struct aivory_queue_entry *next;
} aivory_queue_entry_t;

/* Message queue */
typedef struct {
    aivory_queue_entry_t *head;
    aivory_queue_entry_t *tail;
    int count;
    int max_size;
    void *mutex; /* pthread_mutex_t */
} aivory_queue_t;

#ifdef __cplusplus
}
#endif

#endif /* AIVORY_TYPES_H */
