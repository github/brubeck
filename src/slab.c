#include "brubeck.h"

static struct brubeck_slab_node *push_node(struct brubeck_slab *slab)
{
	struct brubeck_slab_node *node = xmalloc(SLAB_SIZE * SLABS_PER_NODE);

	node->alloc = 0;
	node->next = slab->current;
	slab->current = node;

	return node;
}

void *brubeck_slab_alloc(struct brubeck_slab *slab, size_t need)
{
	struct brubeck_slab_node *node;
	void *ptr;

	need = ((need + SLAB_SIZE - 1) & ~(SLAB_SIZE - 1));

	pthread_mutex_lock(&slab->lock);

	node = slab->current;

	if (node->alloc + need > NODE_SIZE) {
		node = push_node(slab);
	}

	slab->total_alloc += need;

	ptr = node->heap + node->alloc;
	node->alloc += need;

	pthread_mutex_unlock(&slab->lock);

	return ptr;
}

void brubeck_slab_init(struct brubeck_slab *slab)
{
	push_node(slab);
	pthread_mutex_init(&slab->lock, NULL);
}
