/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>

#include "daemon.h"

extern FILE *trace;
extern gboolean do_syslog;

void colod_trace(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (trace) {
        vfprintf(trace, fmt, args);
        fflush(trace);
    }

    va_end(args);
}

void colod_syslog(int pri, const char *fmt, ...) {
    va_list args;

    if (trace) {
        va_start(args, fmt);
        vfprintf(trace, fmt, args);
        fwrite("\n", 1, 1, trace);
        fflush(trace);
        va_end(args);
    }

    va_start(args, fmt);
    if (do_syslog) {
        vsyslog(pri, fmt, args);
    } else {
        vfprintf(stderr, fmt, args);
        fwrite("\n", 1, 1, stderr);
    }
    va_end(args);
}

int main(int argc, char **argv) {
    return daemon_main(argc, argv);
}
