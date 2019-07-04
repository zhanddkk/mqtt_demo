/*
 * ring_buffer.c
 *
 *  Created on: Jul 2, 2019
 *      Author: zhanlei
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ring_buffer.h"

#if 0
#define DEBUG_LOG(fmt, ...)		printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...)
#endif

struct stDataPage {
	int len;
	void *data;
};

/**
 * Init Ring Buffer, malloc memory for ring buffer
 * @param rb: ring buffer instance
 * @param page_size: size of per page (byte), must greater then 0
 * @param total_page_number: total pages number of this ring buffer, must greater then  0
 * @return 0 if succeed, else returns -1
 */
int ring_buffer_init(struct stRingBuffer *rb, int page_size, int total_page_number) {
	int ret = -1;
	const int bytes_0 = page_size * total_page_number;
	const int bytes_1 = sizeof(struct stDataPage) * total_page_number;
	memset(rb, 0, sizeof(struct stRingBuffer));
	rb->pages = malloc(bytes_0 + bytes_1);
	if (rb->pages) {
		memset(rb->pages, 0, bytes_0 + bytes_1);
		rb->buffer = (uint8_t *)rb->pages + bytes_1;
		rb->total_page_number = total_page_number;
		rb->page_size = page_size;
		ret = 0;
		printf("==== Ring Buffer Memory Usage ====\n");
		printf("Total    [%p - %p]: %d bytes\n",
				rb->pages, (uint8_t *)rb->pages + bytes_0 + bytes_1,
				bytes_0 + bytes_1);
		printf(" *Pages  [%p - %p]: %d bytes\n",
				rb->pages, (uint8_t *)rb->pages + bytes_1,
				bytes_1);
		printf(" *Buffer [%p - %p]: %d bytes\n",
				rb->buffer, (uint8_t*) rb->buffer + bytes_0,
				bytes_0);
	}
	return ret;
}

/**
 * Release ring buffer resources
 * @param rb: ring buffer instance
 */
void ring_buffer_free(struct stRingBuffer *rb) {
	if (rb->pages) {
		free(rb->pages);
	}
	memset(rb, 0, sizeof(struct stRingBuffer));
}

/**
 * Read and remove data from ring buffer
 * @param rb: ring buffer instance
 * @param pdata: pointer of read out data
 * @return data length that has read, else return -1
 */
int ring_buffer_read(struct stRingBuffer *rb, void **pdata) {
	int dln = -1;
	if (ring_buffer_used(rb) > 0) {
		struct stDataPage *rp;
		rb->r++;
		if (rb->r >= rb->total_page_number) {
			rb->r = 0;
		}
		rp = (struct stDataPage *)rb->pages + rb->r;
		*pdata = rp->data;
		dln = rp->len;
		rp->data = NULL;
		DEBUG_LOG("Read %d bytes at page(%p)[%d] @%p\n", dln, rp, rb->r, *pdata);
	} else {
		DEBUG_LOG("Ring buffer is empty\n");
	}
	return dln;
}

/**
 * Input data to ring buffer
 * @param rb: ring buffer instance
 * @param data: data need to input
 * @param dln: data length
 * @return data length that has written, else returns -1
 */
int ring_buffer_write(struct stRingBuffer *rb, const void *data, int dln) {
	int ret = -1;
	if (ring_buffer_used(rb) < rb->total_page_number) {
		struct stDataPage *wp;
		rb->w++;
		if (rb->w >= rb->total_page_number) {
			rb->w = 0;
		}
		wp = (struct stDataPage *)rb->pages + rb->w;
		wp->len = (dln > rb->page_size) ? rb->page_size : dln;
		wp->data = (uint8_t *)rb->buffer + rb->page_size * rb->w;
		memcpy(wp->data, data, wp->len);
		ret = wp->len;
		DEBUG_LOG("Write %d bytes at page(%p)[%d] @%p\n", wp->len, wp, rb->w, wp->data);
	} else {
		DEBUG_LOG("Ring buffer is full\n");
	}
	return ret;
}

/**
 * How many pages has been used in the ring buffer
 * @param rb: ring buffer instance
 * @return total pages number that has been used in the ring buffer
 */
int ring_buffer_used(struct stRingBuffer *rb) {
	int dln;
	if (rb->w > rb->r) {
		dln = rb->w - rb->r;
	} else if (rb->w < rb->r) {
		dln = rb->total_page_number + rb->w - rb->r;
	} else {
		struct stDataPage *page = (struct stDataPage *)rb->pages + rb->r;
		if (page->data) {
			dln = rb->total_page_number;
		} else {
			dln = 0;
		}
	}
	return dln;
}

// ==============================================================================
// Test code
// ------------------------------------------------------------------------------
#if 0
void ring_buffer_test() {
	struct stRingBuffer rb;
	const uint8_t *data = NULL;
	int val = 0;
	int dln = 0;
	ring_buffer_init(&rb, sizeof(val), 10);
	dln = ring_buffer_read(&rb, (void**) &data);
	printf("dln = %d\n", dln);
	for (int i = 0; i < 15; i++) {
		int ret = ring_buffer_write(&rb, &i, sizeof(i));
		printf("W %d: %d\n", i, ret);
	}

	for (int i = 0; i < 15; i++) {
		dln = ring_buffer_read(&rb, (void**) &data);
		if (dln > 0) {
			val = 0;
			memcpy(&val, data, dln);
			printf("R %d: %d bytes -> %d\n", i, dln, val);
		} else {
			printf("R %d: %d\n", i, dln);
		}
	}

	for (int i = 0; i < 7; i++) {
		int ret = ring_buffer_write(&rb, &i, sizeof(i));
		printf("W %d: %d\n", i, ret);
	}

	for (int i = 0; i < 5; i++) {
		dln = ring_buffer_read(&rb, (void**) &data);
		if (dln > 0) {
			val = 0;
			memcpy(&val, data, dln);
			printf("R %d: %d bytes -> %d\n", i, dln, val);
		} else {
			printf("R %d: %d\n", i, dln);
		}
	}

	for (int i = 0; i < 2; i++) {
		int ret = ring_buffer_write(&rb, &i, sizeof(i));
		printf("W %d: %d\n", i, ret);
	}

	for (int i = 0; i < 13; i++) {
		dln = ring_buffer_read(&rb, (void**) &data);
		if (dln > 0) {
			val = 0;
			memcpy(&val, data, dln);
			printf("R %d: %d bytes -> %d\n", i, dln, val);
		} else {
			printf("R %d: %d\n", i, dln);
		}
	}
	ring_buffer_free(&rb);
}
#endif
