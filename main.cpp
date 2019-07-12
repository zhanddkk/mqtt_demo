/*
 * main.cpp
 *
 *  Created on: May 7, 2019
 *      Author: zhanlei
 */
#include <stdio.h>
#include <string.h>
#include "demo.h"

struct stProcess {
	const char *name;
	int (*pf)(int argc, const char *argv[]);
};

static struct stProcess process_list[] = {
	{"slc", slc_node_demo_main},
	{"nmc", nmc_node_demo_main},
	{"monitor", sys_monitor_main},
	{"event-log", event_log_demo_main}
};

const int process_number = sizeof(process_list) / sizeof(struct stProcess);

int main(int argc, const char *argv[]) {
	int ret = 0;
	int index = 3;
	if (argc > 1) {
		const char *name = argv[1];
		for (int i = 0; i < process_number; i++) {
			if (strcmp(name, process_list[i].name) == 0) {
				index = i;
				break;
			}
		}
		// sub process args
		argc -= 2;
		argv += 2;
	} else if (argc == 1) {
		argc--;
		argv += 1;
	} else {
		return -1;
	}
	printf("Run As: %s\n", process_list[index].name);
	for (int i = 0; i < argc; i++) {
		printf(" *argv[%d]: %s\n", i, argv[i]);
	}
	ret = process_list[index].pf(argc, argv);
	return ret;
}

