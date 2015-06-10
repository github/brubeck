#ifndef __BRUBECK_SLAB_H__
#define __BRUBECK_SLAB_H__

/* Each slab has 32 bytes; 128 slabs per node = 4096 bytes (one page) */
#define SLAB_SIZE 32
#define SLABS_PER_NODE 128
#define NODE_SIZE (SLAB_SIZE * (SLABS_PER_NODE - 1))

struct brubeck_slab_node {
	struct brubeck_slab_node *next;
	size_t alloc;
	char heap[];
};

struct brubeck_slab {
	struct brubeck_slab_node *current;
	size_t total_alloc;
	pthread_mutex_t lock;
};

void brubeck_slab_init(struct brubeck_slab *slab);
void *brubeck_slab_alloc(struct brubeck_slab *slab, size_t need);

#endif
