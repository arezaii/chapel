#include "argparsing.h"
#include <qthread/qthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_OPTION(option, opr, val)                                          \
  do {                                                                         \
    size_t v = qthread_readstate(option);                                      \
    iprintf("%s: " #option " = %zu, expect " #opr " %zu\n",                    \
            (v opr val) ? "GOOD" : " BAD",                                     \
            v,                                                                 \
            val);                                                              \
    test_check(v opr val);                                                     \
  } while (0)

static aligned_t spinner(void *arg) {
  while (atomic_load_explicit((_Atomic aligned_t *)arg, memory_order_relaxed) ==
         0);
  return 1;
}

int main(int argc, char *argv[]) {
  int status;

  CHECK_VERBOSE(); // part of the testing harness; toggles iprintf() output

  status = qthread_initialize();
  test_check(status == QTHREAD_SUCCESS);

  iprintf("%i shepherds...\n", qthread_num_shepherds());
  iprintf("  %i threads total\n", qthread_num_workers());

  TEST_OPTION(STACK_SIZE, >=, 2048);
  {
    _Atomic aligned_t r;
    atomic_store_explicit(&r, 0u, memory_order_relaxed);
    TEST_OPTION(BUSYNESS, ==, 1); // Just this thread
    TEST_OPTION(NODE_BUSYNESS, ==, 1);
    TEST_OPTION(WORKER_OCCUPATION, ==, 1);
    qthread_fork(spinner, (void *)&r, (aligned_t *)&r);
    qthread_flushsc();
    TEST_OPTION(BUSYNESS, >=, 1);
    TEST_OPTION(BUSYNESS, <=, 2);
    /* This is <= instead of == because spinner might be dequeued but not
     * yet executing */
    TEST_OPTION(NODE_BUSYNESS, <=, 2);
    TEST_OPTION(WORKER_OCCUPATION, >=, 1);
    TEST_OPTION(WORKER_OCCUPATION, <=, 2);
    atomic_store_explicit(&r, 1u, memory_order_relaxed);
    qthread_readFF(NULL, (aligned_t *)&r);
  }
  {
    size_t sheps;
    TEST_OPTION(TOTAL_SHEPHERDS, >=, 1);
    sheps = qthread_readstate(TOTAL_SHEPHERDS);
    test_check(sheps >= 1);
    TEST_OPTION(ACTIVE_SHEPHERDS, >=, 1);
    TEST_OPTION(ACTIVE_SHEPHERDS, <=, sheps);
  }
  {
    size_t wkrs;
    TEST_OPTION(TOTAL_WORKERS, >=, 1);
    wkrs = qthread_readstate(TOTAL_WORKERS);
    test_check(wkrs >= 1);
    TEST_OPTION(ACTIVE_WORKERS, >=, 1);
    TEST_OPTION(ACTIVE_WORKERS, <=, wkrs);
  }
  TEST_OPTION(CURRENT_SHEPHERD, ==, 0);      // maybe this will change someday
  TEST_OPTION(CURRENT_WORKER, ==, 0);        // maybe this will change someday
  TEST_OPTION(CURRENT_UNIQUE_WORKER, ==, 0); // maybe this will change someday
  TEST_OPTION(CURRENT_TEAM, ==, QTHREAD_DEFAULT_TEAM_ID);
  TEST_OPTION(PARENT_TEAM, ==, QTHREAD_NON_TEAM_ID);

  return EXIT_SUCCESS;
}

/* vim:set expandtab */
