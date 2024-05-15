#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "daemon.h"
#include "coroutine_stack.h"

void colod_watchdog_refresh(ColodWatchdog *state);
void colo_watchdog_free(ColodWatchdog *state);
ColodWatchdog *colod_watchdog_new(const ColodContext *ctx);

#endif // WATCHDOG_H
