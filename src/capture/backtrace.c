/**
 * Backtrace capture using execinfo or libunwind.
 */

#include "aivory/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifndef HAVE_LIBUNWIND
#include <execinfo.h>
#endif

#define MAX_FRAMES 50
#define MAX_JSON_SIZE 32768

static char *escape_json_string(const char *str) {
    if (!str) return strdup("");

    size_t len = strlen(str);
    char *escaped = malloc(len * 2 + 1);
    if (!escaped) return strdup("");

    char *dst = escaped;
    for (const char *src = str; *src; src++) {
        switch (*src) {
            case '"':  *dst++ = '\\'; *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n';  break;
            case '\r': *dst++ = '\\'; *dst++ = 'r';  break;
            case '\t': *dst++ = '\\'; *dst++ = 't';  break;
            default:   *dst++ = *src; break;
        }
    }
    *dst = '\0';

    return escaped;
}

static void extract_function_info(const char *symbol, char **func_name, char **file_path, int *offset) {
    *func_name = NULL;
    *file_path = NULL;
    *offset = 0;

    if (!symbol) return;

    /* Parse format: "module(function+offset) [address]" or similar */
    const char *lparen = strchr(symbol, '(');
    const char *rparen = strchr(symbol, ')');

    if (lparen && rparen && lparen < rparen) {
        /* Extract module path */
        size_t path_len = lparen - symbol;
        *file_path = malloc(path_len + 1);
        if (*file_path) {
            strncpy(*file_path, symbol, path_len);
            (*file_path)[path_len] = '\0';
        }

        /* Extract function name */
        const char *plus = strchr(lparen + 1, '+');
        if (plus && plus < rparen) {
            size_t name_len = plus - lparen - 1;
            if (name_len > 0) {
                *func_name = malloc(name_len + 1);
                if (*func_name) {
                    strncpy(*func_name, lparen + 1, name_len);
                    (*func_name)[name_len] = '\0';
                }
            }
            *offset = (int)strtol(plus + 1, NULL, 0);
        } else {
            size_t name_len = rparen - lparen - 1;
            if (name_len > 0) {
                *func_name = malloc(name_len + 1);
                if (*func_name) {
                    strncpy(*func_name, lparen + 1, name_len);
                    (*func_name)[name_len] = '\0';
                }
            }
        }
    } else {
        /* Just use the whole symbol */
        *func_name = strdup(symbol);
    }
}

char *aivory_capture_backtrace(int skip) {
    char *json = malloc(MAX_JSON_SIZE);
    if (!json) return NULL;

    int json_len = 0;
    json[json_len++] = '[';

#ifdef HAVE_LIBUNWIND
    unw_cursor_t cursor;
    unw_context_t context;
    unw_word_t ip, offset;
    char func_name[256];
    int frame_count = 0;
    int skipped = 0;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    while (unw_step(&cursor) > 0 && frame_count < MAX_FRAMES) {
        if (skipped < skip) {
            skipped++;
            continue;
        }

        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        func_name[0] = '\0';
        offset = 0;

        if (unw_get_proc_name(&cursor, func_name, sizeof(func_name), &offset) != 0) {
            strcpy(func_name, "<unknown>");
        }

        char *escaped_name = escape_json_string(func_name);

        if (frame_count > 0) {
            json[json_len++] = ',';
        }

        int written = snprintf(json + json_len, MAX_JSON_SIZE - json_len,
            "{"
            "\"method_name\":\"%s\","
            "\"is_native\":true,"
            "\"source_available\":false"
            "}",
            escaped_name
        );

        free(escaped_name);

        if (written > 0) {
            json_len += written;
        }

        frame_count++;
    }
#else
    void *frames[MAX_FRAMES];
    int frame_count = backtrace(frames, MAX_FRAMES);
    char **symbols = backtrace_symbols(frames, frame_count);

    if (symbols) {
        int actual_frames = 0;
        for (int i = skip; i < frame_count && actual_frames < MAX_FRAMES; i++) {
            char *func_name = NULL;
            char *file_path = NULL;
            int offset = 0;

            extract_function_info(symbols[i], &func_name, &file_path, &offset);

            char *escaped_name = escape_json_string(func_name ? func_name : "<unknown>");
            char *escaped_path = escape_json_string(file_path ? file_path : "");

            if (actual_frames > 0) {
                json[json_len++] = ',';
            }

            int written = snprintf(json + json_len, MAX_JSON_SIZE - json_len,
                "{"
                "\"method_name\":\"%s\","
                "\"file_path\":\"%s\","
                "\"is_native\":%s,"
                "\"source_available\":false"
                "}",
                escaped_name,
                escaped_path,
                file_path ? "false" : "true"
            );

            free(escaped_name);
            free(escaped_path);
            free(func_name);
            free(file_path);

            if (written > 0) {
                json_len += written;
            }

            actual_frames++;
        }
        free(symbols);
    }
#endif

    json[json_len++] = ']';
    json[json_len] = '\0';

    return json;
}

char *aivory_calculate_fingerprint(const char *type, const char *backtrace) {
    /* Simple hash calculation */
    unsigned long hash = 5381;

    if (type) {
        for (const char *c = type; *c; c++) {
            hash = ((hash << 5) + hash) + *c;
        }
    }

    if (backtrace) {
        /* Hash first part of backtrace */
        int chars = 0;
        for (const char *c = backtrace; *c && chars < 500; c++, chars++) {
            hash = ((hash << 5) + hash) + *c;
        }
    }

    char *fingerprint = malloc(17);
    if (fingerprint) {
        snprintf(fingerprint, 17, "%016lx", hash);
    }

    return fingerprint;
}
