// ring.c
// based on https://github.com/willemt/cbuffer

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

#include "ring.h"

static void setup_memory_map(ring_t *cb)
{
	char path[] = "/tmp/ring@XXXXXX";
	void *address;
	int fd;
	int status;

	fd = mkstemp(path);
	assert(fd >= 0);

	status = unlink(path);
	assert(status == 0);

	status = ftruncate(fd, cb->size);
	assert(status == 0);

	cb->data = mmap(NULL, cb->size << 1, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE,
					-1, 0);
	assert(cb->data != MAP_FAILED);

	address = mmap(cb->data, cb->size, PROT_READ | PROT_WRITE,
				   MAP_FIXED | MAP_SHARED, fd, 0);
	assert(address == cb->data);

	address = mmap(cb->data + cb->size, cb->size, PROT_READ | PROT_WRITE,
				   MAP_FIXED | MAP_SHARED, fd, 0);
	assert(address == cb->data + cb->size);

	status = close(fd);
	assert(status == 0);
}

ring_t *ring_new(unsigned int order)
{
	ring_t *r = malloc(sizeof(ring_t));
	r->size = 1UL << order;
	r->mask = r->size - 1;
	r->head = r->tail = 0;
	setup_memory_map(r);
	return r;
}

void ring_free(ring_t *r)
{
	munmap(r->data, r->size << 1);
	free(r);
}

int ring_available(const ring_t *r)
{
	return (r->tail + r->size - r->head) & r->mask;
}

void *ring_head(ring_t *r)
{
	return r->data + r->head;
}

void ring_head_advance(ring_t *r, unsigned int bytes)
{
	// TODO: check if the head exceeds the tail
	r->head = (r->head + bytes) & r->mask;
}

void *ring_tail(ring_t *r)
{
	return r->data + r->tail;
}

void ring_tail_advance(ring_t *r, unsigned int bytes)
{
	// TODO: check if the tail exceeds the head
	r->tail = (r->tail + bytes) & r->mask;
}
