CC=gcc

CFLAGS=-g -O2 -Wall -Wextra -fsanitize=address `pkg-config --cflags libnl-3.0 glib-2.0 json-glib-1.0`
CPG_LDFLAGS=-lcorosync_common -lcpg
LDFLAGS=`pkg-config --libs libnl-3.0 glib-2.0 json-glib-1.0`
common_objects=util.o qemu_util.o json_util.o coutil.o qmp.o client.o netlink.o watchdog.o qmpcommands.o raise_timeout_coroutine.o yellow_coroutine.o eventqueue.o main_coroutine.o daemon.o

%.o: %.c *.h
	$(CC) -c -o $@ $< $(CFLAGS)

all: colod check

colod: $(common_objects) cpg.o colod.o
	$(CC) -o $@ $^ $(CFLAGS) $(CPG_LDFLAGS) $(LDFLAGS)

smoketest_quit_early: $(common_objects) stub_cpg.o smoke_util.o smoketest_quit_early.o smoketest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

smoketest_client_quit: $(common_objects) stub_cpg.o smoke_util.o smoketest_client_quit.o smoketest.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

test_eventqueue: eventqueue.o test_eventqueue.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

test_yellow_coroutine: util.o stub_cpg.o stub_netlink.o yellow_coroutine.o test_yellow_coroutine.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

io_watch_test: util.o io_watch_test.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

netlink_test: util.o netlink.o netlink_test.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean check

check: smoketest_quit_early smoketest_client_quit test_eventqueue test_yellow_coroutine netlink_test
	$(foreach EXEC,$^, echo "./${EXEC}"; ./${EXEC} || exit 1;)

clean:
	rm -f *.o colod smoketest_quit_early smoketest_client_quit test_eventqueue io_watch_test netlink_test
