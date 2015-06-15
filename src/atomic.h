#ifndef __BRUBECK_ATOMIC_H__
#define __BRUBECK_ATOMIC_H__

#include <stdint.h>

#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))

/* Compile read-write barrier */
#define barrier() __asm__ volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() __asm__ volatile("pause\n": : :"memory")

/* Atomic exchange (of various sizes) */
static inline uint64_t atomic_xchg64(void *ptr, uint64_t x)
{
	__asm__ __volatile__("xchgq %0,%1"
			:"=r" (x)
			:"m" (*(volatile uint64_t *)ptr), "0" (x)
			:"memory");

	return x;
}

static inline unsigned atomic_xchg32(void *ptr, uint32_t x)
{
	__asm__ __volatile__("xchgl %0,%1"
			:"=r" (x)
			:"m" (*(volatile uint32_t *)ptr), "0" (x)
			:"memory");

	return x;
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
