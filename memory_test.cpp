/*
 * memory_test.cpp
 *
 *  Created on: Jun 26, 2019
 *      Author: zhanlei
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

static void test() {
	const int len = 1024 * 60;
	const int size = 1024;
	uint8_t *data_ptr[len];
	printf("malloc\n");
	memset(data_ptr, 0, sizeof(data_ptr));
	for (int i = 0; i < len; i++) {
		data_ptr[i] = (uint8_t *)malloc(size);
		if (data_ptr[i]) {
			memset(data_ptr[i], i & 0xff, size);
			data_ptr[i][0] = '0' + i % 9;
		}
	}
	malloc_stats();
	printf("free?");
	getchar();
	for (int i = 0; i < len; i++) {
		uint8_t *ptr = data_ptr[len - i - 1];
		if (ptr) {
			int size = malloc_usable_size(ptr);
			ptr[size- 1] = '\0';
			free(ptr);
		}
		if (i % (1024 * 10) == (1024 * 10 - 1)) {
			malloc_stats();
			printf("[%d]next 1MB * 10->", i / (1024 * 10));
			getchar();
		}
	}
	printf("next?Y/N");
	while (getchar() != 'Y');
	malloc_trim(128 * 1024);
}

int memory_test_main(int argc, char *argv[]) {
	printf("ret = %d\n", mallopt(M_MXFAST, 0));
	test();
	printf("exit?[Y/N]");
	while (getchar() != 'Y') {
		malloc_stats();
//		malloc_info();
	}
	return 0;
}

