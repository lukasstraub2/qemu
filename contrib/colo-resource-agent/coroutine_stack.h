/*
 * COLO background daemon coroutine stack management
 *
 * Copyright (c) Lukas Straub <lukasstraub2@web.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef COROUTINE_STACK_H
#define COROUTINE_STACK_H

#include <stddef.h>
#include <assert.h>

#include <glib-2.0/glib.h>

#define COROUTINE_FRAME_SIZE (7*8)
#define COROUTINE_STACK_SIZE 16

typedef struct CoroutineFrame {
    char data[COROUTINE_FRAME_SIZE];
    unsigned int line;
} CoroutineFrame;

typedef struct CoroutineCallback {
    GSourceFunc plain;
    GIOFunc iofunc;
} CoroutineCallback;

typedef struct Coroutine {
    int quit;
    int yield;
    void *yield_value;
    CoroutineFrame *frame;
    CoroutineFrame stack[COROUTINE_STACK_SIZE];
    CoroutineCallback cb;
} Coroutine;

#define co_frame(co, size) \
    do { \
        _Static_assert((size) <= COROUTINE_FRAME_SIZE, "size <= COROUTINE_FRAME_SIZE failed"); \
        (co) = (void *)coroutine->frame->data; \
    } while (0)

#define co_yield(value) \
    do { \
        coroutine->yield = 1; \
        coroutine->yield_value = (void *) (value); \
        _co_yield(coroutine_yield_ret); \
    } while (0)

#define co_yield_int(value) \
    co_yield(GINT_TO_POINTER(value))

#define co_enter(coroutine, expr) \
    do { \
        if (!(coroutine)->frame) { \
            (coroutine)->frame = (coroutine)->stack; \
        } \
        (coroutine)->yield = 0; \
        assert(!(coroutine)->quit); \
        assert((coroutine)->frame == (coroutine)->stack); \
        expr; \
        assert((coroutine)->frame == (coroutine)->stack); \
        if (!(coroutine)->yield) { \
            (coroutine)->frame->line = 0; \
            (coroutine)->quit = 1; \
        } \
    } while(0)

#define co_recurse(expr) \
    while(1) { \
        coroutine->frame++; \
        assert(coroutine->frame - coroutine->stack < COROUTINE_STACK_SIZE); \
        int __use_co_recurse = 1; \
        expr; \
        coroutine->frame--; \
        if (coroutine->yield) { \
            _co_yield(coroutine_yield_ret); \
        } else { \
            coroutine->frame[1].line = 0; \
            break; \
        } \
    }

#define co_wrap(expr) \
    ((void)__use_co_recurse, expr)

#endif // COROUTINE_STACK_H
