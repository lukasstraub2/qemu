/*
 * COLO background daemon
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef RAISE_TIMEOUT_COROUTINE_H
#define RAISE_TIMEOUT_COROUTINE_H

#include "qmp.h"
#include "daemon.h"

typedef struct ColodRaiseCoroutine ColodRaiseCoroutine;

void colod_raise_timeout_coroutine(ColodRaiseCoroutine **ptr,
                                   ColodQmpState *qmp,
                                   const ColodContext *ctx);
void colod_raise_timeout_coroutine_free(ColodRaiseCoroutine **ptr);

#endif // RAISE_TIMEOUT_COROUTINE_H
