/*
 * sys_monitor.cpp
 *
 *  Created on: May 17, 2019
 *      Author: zhanlei
 */
#include <stdio.h>
#include <stdint.h>
#include <mosquitto.h>

static void _message_callback(struct mosquitto *mosq, void *userdata,
		const struct mosquitto_message *message) {
	if (message->payloadlen) {
		printf("%s: %s\n", message->topic, (char *)message->payload);
	} else {
	}
}

static void _connect_callback(struct mosquitto *mosq, void *userdata, int result) {
	if (!result) {
//		mosquitto_subscribe(mosq, NULL,
//				"$SYS/#", 0);
		mosquitto_subscribe(mosq, NULL,
				"$SYS/broker/heap/current", 0);
		mosquitto_subscribe(mosq, NULL,
				"$SYS/broker/heap/maximum", 0);
	} else {
		fprintf(stderr, "Connect failed\n");
	}
}

static void _subscribe_callback(struct mosquitto *mosq, void *userdata, int mid,
		int qos_count, const int *granted_qos) {
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for (i = 1; i < qos_count; i++) {
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

int sys_monitor_main(int argc, const char *argv[]) {
	const char *host = "169.254.5.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;

	mosquitto_lib_init();
	mosq = mosquitto_new("slc_simulator", clean_session, nullptr);
	if (mosq) {
		mosquitto_connect_callback_set(mosq, _connect_callback);
		mosquitto_message_callback_set(mosq, _message_callback);
		mosquitto_subscribe_callback_set(mosq, _subscribe_callback);
		if (mosquitto_username_pw_set(mosq, "Simulator3", "98d7")
				== MOSQ_ERR_SUCCESS) {
			if (mosquitto_connect(mosq, host, port, keepalive)
					== MOSQ_ERR_SUCCESS) {
				mosquitto_loop_forever(mosq, -1, 1);
			} else {
				fprintf(stderr, "Unable to connect.\n");
			}
		} else {
			fprintf(stderr, "Can't set username and password.\n");
		}
		mosquitto_destroy(mosq);
	} else {
		fprintf(stderr, "Error: Out of memory.\n");
	}
	fflush(stderr);
	mosquitto_lib_cleanup();
	return 0;
}


