#ifndef PHOXTRA_SPINLOCK_H
#define PHOXTRA_SPINLOCK_H

#include <stdint.h>

/*
 * Small x86-64 spinlock for kernel-owned shared state.
 *
 * Kernel call sites should normally use spin_lock_irqsave() /
 * spin_unlock_irqrestore() so a local interrupt cannot preempt a lock holder
 * and deadlock while trying to acquire the same lock. The non-IRQ variants
 * are also exposed for host-side stress testing and carefully controlled
 * low-level use.
 */
typedef struct {
    volatile uint32_t value;
} spinlock_t;

#define SPINLOCK_INITIALIZER { 0u }

typedef uint64_t spinlock_irq_flags_t;

static inline void spinlock_init(spinlock_t* lock) {
    if (lock) __atomic_store_n(&lock->value, 0u, __ATOMIC_RELAXED);
}

static inline void spin_lock(spinlock_t* lock) {
    if (!lock) return;
    while (__atomic_exchange_n(&lock->value, 1u, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->value, __ATOMIC_RELAXED)) {
            __asm__ volatile("pause" ::: "memory");
        }
    }
}

static inline int spin_try_lock(spinlock_t* lock) {
    if (!lock) return 0;
    return __atomic_exchange_n(&lock->value, 1u, __ATOMIC_ACQUIRE) == 0u;
}

static inline void spin_unlock(spinlock_t* lock) {
    if (lock) __atomic_store_n(&lock->value, 0u, __ATOMIC_RELEASE);
}

static inline spinlock_irq_flags_t spin_lock_irqsave(spinlock_t* lock) {
    spinlock_irq_flags_t flags;
    __asm__ volatile("pushfq\n\tpopq %0\n\tcli"
                     : "=r"(flags) : : "memory");
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t* lock,
                                          spinlock_irq_flags_t flags) {
    spin_unlock(lock);
    if (flags & (1ULL << 9))
        __asm__ volatile("sti" ::: "memory");
}

#endif
