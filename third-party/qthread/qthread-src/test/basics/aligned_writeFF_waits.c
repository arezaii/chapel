#include "argparsing.h"
#include <qthread/qthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static aligned_t concurrent_t;

static aligned_t writeFF_wrapper(void *arg) {
  iprintf("2: writeFF wrapper started\n");
  qthread_writeFF_const(&concurrent_t, 55);
  iprintf("2: writeFF completed\n");
  return 0;
}

// Test that writeFF waits for empty var to be filled, writes, and leaves full.
// Requires that only one worker is running. Basically does:
//     1: empty var
//     1: fork(writeFF)
//     1: yields
//     2: starts runnning
//     2: hits writeFF, and yields since var is empty
//     1: writeEF
//     1: hits readFF on forked task and yield
//     2: running again, finishes writeFF, task returns
//     1: readFF competes, finishes
static void testWriteFFWaits(void) {
  aligned_t ret;
  concurrent_t = 45;
  qthread_empty(&concurrent_t);
  test_check(qthread_num_workers() == 1);

  iprintf("1: Forking writeFF wrapper\n");
  qthread_fork_to(writeFF_wrapper, NULL, &ret, qthread_shep());
  iprintf("1: Forked, now yielding to 2\n");
  qthread_yield();
  iprintf("1: Back from yield\n");

  // verify that writeFF has not completed
  test_check(qthread_feb_status(&concurrent_t) == 0);
  test_check(concurrent_t != 55);

  iprintf("1: Writing EF\n");
  qthread_writeEF_const(&concurrent_t, 35);

  // wait for writeFF wrapper to complete
  qthread_readFF(NULL, &ret);

  // veify that writeFF completed and that FEB is full
  iprintf("1: concurrent_t=%d\n", concurrent_t);
  test_check(qthread_feb_status(&concurrent_t) == 1);
  test_check(concurrent_t == 55);
}

int main(int argc, char *argv[]) {
  CHECK_VERBOSE();
  test_check(qthread_init(1) == 0);
  iprintf("%i shepherds...\n", qthread_num_shepherds());
  iprintf("  %i threads total\n", qthread_num_workers());

  testWriteFFWaits();

  return 0;
}

/* vim:set expandtab */
