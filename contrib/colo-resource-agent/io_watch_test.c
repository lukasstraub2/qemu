#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <glib-2.0/glib.h>

#include "util.h"

int callback2(GIOChannel *channel, GIOCondition condition, gpointer data) {
    gboolean *quit = data;

    (void) channel;
    (void) condition;

    *quit = TRUE;
    return G_SOURCE_REMOVE;
}

int callback(GIOChannel *channel, GIOCondition condition, gpointer data) {
    gboolean *quit = data;

    (void) channel;
    (void) condition;

    g_io_add_watch_full(channel, G_PRIORITY_LOW, G_IO_IN | G_IO_HUP,
                        callback2, quit, NULL);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    int ret;
    GError *errp = NULL;
    int fd[2];
    GIOChannel *channel;
    gboolean quit = FALSE;
    (void) argc;
    (void) argv;

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (ret < 0) {
        fprintf(stderr, "Failed to open qmp socketpair: %s", g_strerror(errno));
        return 1;
    }

    channel = colod_create_channel(fd[0], &errp);
    if (!channel) {
        fprintf(stderr, "Failed to create channel: %s", errp->message);
        close(fd[0]);
        close(fd[1]);
        return 1;
    }

    g_io_add_watch_full(channel, G_PRIORITY_LOW, G_IO_IN | G_IO_HUP,
                        callback, &quit, NULL);

    colod_shutdown_channel(channel);

    while (!quit) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }

    g_io_channel_unref(channel);

    return 0;
}
