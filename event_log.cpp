/*
 * event_log.cpp
 *
 *  Created on: May 17, 2019
 *      Author: zhanlei
 */
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <mosquitto.h>
#include <semaphore.h>

#include "payload.h"

struct ThreadData {
	uint32_t count;
	struct mosquitto *mosq;
};

static sem_t send_log_sem;

static void _message_callback(struct mosquitto *mosq, void *userdata,
		const struct mosquitto_message *message) {
	if (message->topic[0] == '$') {
		printf("%s: %s\n", message->topic, (char *)message->payload);
	} else if (message->payloadlen) {
		Payload &payload = *(Payload *) userdata;
		if (decode_payload((const uint8_t *) message->payload,
				message->payloadlen, payload)) {
			sem_post(&send_log_sem);
		}
	} else {
	}
}

static void _connect_callback(struct mosquitto *mosq, void *userdata, int result) {
	if (!result) {
		mosquitto_subscribe(mosq, NULL,
				"Device/UC/Test/General/TestEventLogPerformanceData", 0);
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

static void *_recv_task(void *ptr) {
	struct ThreadData *pd = (struct ThreadData *) ptr;
	const char *host = "169.254.5.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;

	Uint32Value ui32_val;
	Payload payload;
	payload.p_value = &ui32_val;

	mosquitto_lib_init();
	mosq = mosquitto_new("slc_simulator", clean_session, &payload);
	if (mosq) {
		mosquitto_connect_callback_set(mosq, _connect_callback);
		mosquitto_message_callback_set(mosq, _message_callback);
		mosquitto_subscribe_callback_set(mosq, _subscribe_callback);
		/*
		_username_map = (
			("Simulator1", "3279"),
			("Simulator2", "be95"),
			("Simulator3", "98d7"),
			("Simulator4", "9f02"),
		)
		 */
		if (mosquitto_username_pw_set(mosq, "Simulator3", "98d7")
				== MOSQ_ERR_SUCCESS) {
			if (mosquitto_connect(mosq, host, port, keepalive)
					== MOSQ_ERR_SUCCESS) {
				pd->mosq = mosq;
				mosquitto_loop_forever(mosq, -1, 1);
			} else {
				fprintf(stderr, "Unable to connect.\n");
			}
		} else {
			fprintf(stderr, "Can't set username and password.\n");
		}
		pd->mosq = nullptr;
		mosquitto_destroy(mosq);
	} else {
		fprintf(stderr, "Error: Out of memory.\n");
	}
	fflush(stderr);
	mosquitto_lib_cleanup();
	return nullptr;
}

int event_log_demo_main(int argc, char *argv[]) {

	pthread_t thread;
	struct ThreadData data = { 0, nullptr };
	sem_init(&send_log_sem, 0, 0);
	int ret = pthread_create(&thread, nullptr, _recv_task, &data);

	if (ret == 0) {
		int mid = 0;
		uint8_t buffer[1024];
		uint32_t size = 0;
		// {3146049, 3, 20, 1, 1537017285, 0, 0, 610814367, 445, 0, 0, 1, 0, 0, 0, 2, 1, 610814129, 414, ''}
		EventLogDataToNmcValue event_log_val;
		event_log_val.event_id = 3146049;
		Payload payload;
		payload.p_value = &event_log_val;
		payload.id = 0x6CED9F98;
		payload.producer_mask = 0x02;
		payload.action = 0;
		while (data.mosq == nullptr) {
			usleep(100000);
		}
		while (true) {
			if (sem_wait(&send_log_sem) == 0) {
				if (encode_payload(buffer, sizeof(buffer), size, payload)) {
					if (mosquitto_publish(data.mosq, &mid,
							"UPSSystem/General/EventLogDataToNMC", size, buffer,
							0, false) != MOSQ_ERR_SUCCESS) {
						printf("publish event log failed\n");
					}
				}
			}
		}
	} else {
		fprintf(stderr, "Can't create pthread.\n");
	}

	return 0;
}

