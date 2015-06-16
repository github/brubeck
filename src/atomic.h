#ifndef __BRUBECK_ATOMIC_H__
#define __BRUBECK_ATOMIC_H__

#include <stdint.h>

#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))

/* Compile read-write barrier */
#define barrier() __sync_synchronize ()

/* Pause instruction to prevent excess processor bus usage (when possible) */
#if defined(__i386) || defined(__x86_64__)
#define cpu_relax() __asm__ volatile("pause\n": : :"memory")
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) \
	|| defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#define cpu_relax() __asm__ volatile("or 27,27,27\n": : :"memory")
#elif defined(__ARM_ARCH_6T2__) || defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
	|| defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
	|| defined(__ARM_ARCH_7S__) || defined(__aarch64__)
#define cpu_relax() __asm__ volatile("yield\n": : :"memory")
#else
#define cpu_relax() break
#endif

/* Atomic exchange (of various sizes) */
static inline uint64_t atomic_xchg64(void *ptr, uint64_t x)
{
	// translates to an xchg instruction and returns oldval
	return __sync_lock_test_and_set ((uint64_t *) ptr, x);
}

static inline uint32_t atomic_xchg32(void *ptr, uint32_t x)
{
	// translates to an xchg instruction and returns oldval
	return __sync_lock_test_and_set ((uint32_t *) ptr, x);
}

static inline void atomic_add64d(double *src, double val)
{
	union {
		double fp;
		uint64_t raw;
	} _old, _new;

	do {
		_old.fp = *src;
		_new.fp = (_old.fp + val);
	} while (!__sync_bool_compare_and_swap((uint64_t *)src, _old.raw, _new.raw));
}

#define SPINLOCK_BUSY 1
typedef uint32_t spinlock;

static inline void spin_lock(spinlock *lock)
{
	while (1) {
		if (!atomic_xchg32(lock, SPINLOCK_BUSY)) return;
		while (*lock) cpu_relax();
	}
}

static inline void spin_unlock(spinlock *lock)
{
	barrier();
	*lock = 0;
}

static inline int spin_trylock(spinlock *lock)
{
	return atomic_xchg32(lock, SPINLOCK_BUSY);
}

#endif
