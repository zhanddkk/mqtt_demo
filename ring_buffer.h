/*
 * ring_buffer.h
 *
 *  Created on: Jul 2, 2019
 *      Author: zhanlei
 */

#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>

struct stRingBuffer {
	int r;
	int w;
	int total_page_number;
	int page_size;
	void *pages;
	void *buffer;
};

#ifdef __cplusplus
extern "C" {
#endif
extern int ring_buffer_init(struct stRingBuffer *rb, int page_size, int total_page_number);
extern void ring_buffer_free(struct stRingBuffer *rb);
extern int ring_buffer_read(struct stRingBuffer *rb, void **pdata);
extern int ring_buffer_write(struct stRingBuffer *rb, const void *data, int dln);
extern int ring_buffer_used(struct stRingBuffer *rb);
#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H_ */
