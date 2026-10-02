// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

/* Hosted unit tests for KmodRing index / wrap logic */

typedef struct {
    uint64_t head;
    uint64_t tail;
    uint32_t slot_count;
} ring_state_t;

static inline bool ring_is_empty(ring_state_t *r) {
    return r->head == r->tail;
}

static inline bool ring_is_full(ring_state_t *r) {
    return (r->head - r->tail) >= r->slot_count;
}

static inline uint64_t ring_space_available(ring_state_t *r) {
    uint64_t used = r->head - r->tail;
    if (used >= r->slot_count) return 0;
    return r->slot_count - used;
}

static void test_empty_ring(void) {
    ring_state_t r = { .head = 0, .tail = 0, .slot_count = 1024 };
    assert(ring_is_empty(&r));
    assert(!ring_is_full(&r));
    assert(ring_space_available(&r) == 1024);
    printf("PASS: test_empty_ring\n");
}

static void test_single_record(void) {
    ring_state_t r = { .head = 1, .tail = 0, .slot_count = 1024 };
    assert(!ring_is_empty(&r));
    assert(!ring_is_full(&r));
    assert(ring_space_available(&r) == 1023);
    printf("PASS: test_single_record\n");
}

static void test_exact_full(void) {
    ring_state_t r = { .head = 1024, .tail = 0, .slot_count = 1024 };
    assert(!ring_is_empty(&r));
    assert(ring_is_full(&r));
    assert(ring_space_available(&r) == 0);
    printf("PASS: test_exact_full\n");
}

static void test_wrap_around(void) {
    ring_state_t r = { .head = 5024, .tail = 4000, .slot_count = 1024 };
    assert(!ring_is_empty(&r));
    assert(ring_is_full(&r));
    assert(ring_space_available(&r) == 0);
    printf("PASS: test_wrap_around\n");
}

int main(void) {
    printf("Running KmodRing Hosted Ring Logic Tests (KUnit Shim)...\n");
    test_empty_ring();
    test_single_record();
    test_exact_full();
    test_wrap_around();
    printf("All KUnit shim ring tests PASSED.\n");
    return 0;
}
