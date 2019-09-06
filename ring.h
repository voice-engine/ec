
#ifndef __RING_H__
#define __RING_H__

typedef struct
{
	unsigned long int size;
	unsigned long int mask;
	unsigned int head, tail;
	void *data;
} ring_t;


ring_t *ring_new(unsigned int order);

void ring_free(ring_t *r);

int ring_available(const ring_t *r);

void *ring_head(ring_t *r);

void ring_head_advance(ring_t *r, unsigned int bytes);

void *ring_tail(ring_t *r);

void ring_tail_advance(ring_t *r, unsigned int bytes);

#endif // __RING_H__
