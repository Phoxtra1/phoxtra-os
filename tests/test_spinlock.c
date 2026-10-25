#include "../kernel/sync/spinlock.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define THREAD_COUNT 8
#define ITERATIONS_PER_THREAD 100000u

static spinlock_t test_lock = SPINLOCK_INITIALIZER;
static uint64_t protected_counter;

static void* worker(void* unused) {
    (void)unused;
    for (uint32_t i = 0; i < ITERATIONS_PER_THREAD; ++i) {
        spin_lock(&test_lock);
        ++protected_counter;
        spin_unlock(&test_lock);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        int rc = pthread_create(&threads[i], NULL, worker, NULL);
        if (rc != 0) {
            fprintf(stderr, "FAIL: pthread_create returned %d\n", rc);
            return EXIT_FAILURE;
        }
    }
    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "FAIL: pthread_join returned %d\n", rc);
            return EXIT_FAILURE;
        }
    }

    uint64_t expected = (uint64_t)THREAD_COUNT * ITERATIONS_PER_THREAD;
    if (protected_counter != expected) {
        fprintf(stderr, "FAIL: protected counter %llu, expected %llu\n",
                (unsigned long long)protected_counter,
                (unsigned long long)expected);
        return EXIT_FAILURE;
    }
    puts("PASS: spinlock serializes concurrent updates without lost increments");
    return EXIT_SUCCESS;
}
