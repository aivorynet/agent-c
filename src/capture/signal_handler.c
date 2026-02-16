/**
 * Signal handler for crash capture.
 */

#include "aivory/monitor.h"
#include "aivory/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

/* Original signal handlers */
static struct sigaction g_original_sigsegv;
static struct sigaction g_original_sigabrt;
static struct sigaction g_original_sigfpe;
static struct sigaction g_original_sigbus;
static struct sigaction g_original_sigill;

static aivory_agent_t *g_agent_ref = NULL;
static volatile sig_atomic_t g_handling_signal = 0;

/* Forward declarations */
extern void aivory_ws_send_exception(aivory_agent_t *agent, const char *json);
extern char *aivory_capture_backtrace(int skip);
extern char *aivory_calculate_fingerprint(const char *type, const char *backtrace);

static const char *signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE:  return "SIGFPE";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        default:      return "UNKNOWN";
    }
}

static const char *signal_description(int sig) {
    switch (sig) {
        case SIGSEGV: return "Segmentation fault";
        case SIGABRT: return "Abort signal";
        case SIGFPE:  return "Floating point exception";
        case SIGBUS:  return "Bus error";
        case SIGILL:  return "Illegal instruction";
        default:      return "Unknown signal";
    }
}

static void signal_handler(int sig, siginfo_t *info, void *context) {
    (void)context; /* Unused */

    /* Prevent recursive signal handling */
    if (g_handling_signal) {
        _exit(128 + sig);
    }
    g_handling_signal = 1;

    if (g_agent_ref && g_agent_ref->initialized) {
        /* Capture backtrace */
        char *backtrace = aivory_capture_backtrace(2);

        /* Calculate fingerprint */
        char *fingerprint = aivory_calculate_fingerprint(signal_name(sig), backtrace);

        /* Get timestamp */
        time_t now = time(NULL);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

        /* Build minimal JSON (signal-safe) */
        char json[8192];
        int len = snprintf(json, sizeof(json),
            "{"
            "\"type\":\"exception\","
            "\"payload\":{"
            "\"id\":\"%s-signal\","
            "\"exception_type\":\"%s\","
            "\"message\":\"%s (address: %p)\","
            "\"fingerprint\":\"%s\","
            "\"stack_trace\":%s,"
            "\"local_variables\":{},"
            "\"context\":{\"signal\":%d,\"fatal\":true},"
            "\"captured_at\":\"%s\","
            "\"agent_id\":\"%s\","
            "\"environment\":\"%s\""
            "},"
            "\"timestamp\":%ld"
            "}",
            g_agent_ref->agent_id,
            signal_name(sig),
            signal_description(sig),
            info ? info->si_addr : NULL,
            fingerprint ? fingerprint : "",
            backtrace ? backtrace : "[]",
            sig,
            timestamp,
            g_agent_ref->agent_id,
            g_agent_ref->config.environment,
            (long)now * 1000
        );

        if (len > 0 && len < (int)sizeof(json)) {
            /* Try to send (best effort) */
            aivory_ws_send_exception(g_agent_ref, json);

            /* Give time for send */
            usleep(100000); /* 100ms */
        }

        free(backtrace);
        free(fingerprint);
    }

    /* Restore original handler and re-raise */
    struct sigaction *original = NULL;
    switch (sig) {
        case SIGSEGV: original = &g_original_sigsegv; break;
        case SIGABRT: original = &g_original_sigabrt; break;
        case SIGFPE:  original = &g_original_sigfpe;  break;
        case SIGBUS:  original = &g_original_sigbus;  break;
        case SIGILL:  original = &g_original_sigill;  break;
    }

    if (original && original->sa_handler != SIG_DFL && original->sa_handler != SIG_IGN) {
        sigaction(sig, original, NULL);
        raise(sig);
    } else {
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

void aivory_install_signal_handlers(aivory_agent_t *agent) {
    g_agent_ref = agent;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    /* Install handlers for crash signals */
    sigaction(SIGSEGV, &sa, &g_original_sigsegv);
    sigaction(SIGABRT, &sa, &g_original_sigabrt);
    sigaction(SIGFPE, &sa, &g_original_sigfpe);
    sigaction(SIGBUS, &sa, &g_original_sigbus);
    sigaction(SIGILL, &sa, &g_original_sigill);

    if (agent->config.debug) {
        fprintf(stderr, "[AIVory Monitor] Signal handlers installed\n");
    }
}

void aivory_uninstall_signal_handlers(void) {
    /* Restore original handlers */
    sigaction(SIGSEGV, &g_original_sigsegv, NULL);
    sigaction(SIGABRT, &g_original_sigabrt, NULL);
    sigaction(SIGFPE, &g_original_sigfpe, NULL);
    sigaction(SIGBUS, &g_original_sigbus, NULL);
    sigaction(SIGILL, &g_original_sigill, NULL);

    g_agent_ref = NULL;
}
