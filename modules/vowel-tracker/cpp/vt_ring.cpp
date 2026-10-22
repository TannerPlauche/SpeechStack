//
//  vt_ring.cpp
//  VowelViz
//
//  Copyright (c) 2026 Tanner Plauché. All rights reserved.
//

#include "vowel_tracker.h"

#include <atomic>
#include <vector>

struct vt_ring
{
	explicit vt_ring(long capacity) : buffer(capacity + 1) {}

	std::vector<float>  buffer;     // one slot is left empty to tell full from empty
	std::atomic<long>   head{0};    // next write position, owned by the producer
	std::atomic<long>   tail{0};    // next read position, owned by the consumer
};

vt_ring *vt_ring_create(long capacity)
{
	return new vt_ring(capacity);
}

void vt_ring_destroy(vt_ring *ring)
{
	delete ring;
}

long vt_ring_write(vt_ring *ring, const float *samples, long numFrames, long stride)
{
	long size = (long)ring->buffer.size();
	long head = ring->head.load(std::memory_order_relaxed);
	long tail = ring->tail.load(std::memory_order_acquire);
	long space = (tail - head - 1 + size) % size;
	long count = numFrames < space ? numFrames : space;

	for(long i=0; i<count; i++)
	{
		ring->buffer[head] = samples[i*stride];
		head = (head + 1) % size;
	}
	ring->head.store(head, std::memory_order_release);
	return count;
}

long vt_ring_read(vt_ring *ring, float *dest, long maxSamples)
{
	long size = (long)ring->buffer.size();
	long tail = ring->tail.load(std::memory_order_relaxed);
	long head = ring->head.load(std::memory_order_acquire);
	long available = (head - tail + size) % size;
	long count = maxSamples < available ? maxSamples : available;

	for(long i=0; i<count; i++)
	{
		dest[i] = ring->buffer[tail];
		tail = (tail + 1) % size;
	}
	ring->tail.store(tail, std::memory_order_release);
	return count;
}

void vt_ring_clear(vt_ring *ring)
{
	ring->tail.store(ring->head.load(std::memory_order_acquire), std::memory_order_release);
}
