/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdarg.h>
#include <stdio.h>

/* Keep this definition in sync with dav1d.h. C owns the va_list ABI. */
typedef struct Dav1dLogger {
    void *cookie;
    void (*callback)(void *cookie, const char *format, va_list ap);
} Dav1dLogger;

static void call_logger(const Dav1dLogger *logger, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    logger->callback(logger->cookie, format, ap);
    va_end(ap);
}

void rav1d_log_byte(const Dav1dLogger *logger, int byte) {
    call_logger(logger, "%c", byte);
}

static void log_stdout(void *cookie, const char *format, va_list ap) {
    (void)cookie;
    vfprintf(stdout, format, ap);
}

static void log_stderr(void *cookie, const char *format, va_list ap) {
    (void)cookie;
    vfprintf(stderr, format, ap);
}

const Dav1dLogger rav1d_stdout_logger = {NULL, log_stdout};
const Dav1dLogger rav1d_stderr_logger = {NULL, log_stderr};
