/*
 * slc_node.cpp
 *
 *  Created on: Jul 4, 2019
 *      Author: zhanlei
 */
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <mosquitto.h>
#include <semaphore.h>

#include "payload.h"
#include "ring_buffer.h"
#include "client_info.h"

#define MAX_MSG_SIZE	64
#define MAX_BUFFER_SIZE	2000
namespace SLC {
struct stUserData {
	Payload payload;
	Uint32Value ui32val;
	struct stRingBuffer rb;
	struct mosquitto *mosq;
};
}
static pthread_mutex_t _rb_mutex;
static sem_t _rb_process_sem;
static uint32_t drop_counts = 0;

static void _message_callback(struct mosquitto *mosq, void *userdata,
		const struct mosquitto_message *message) {
	if (message->payloadlen) {
		SLC::stUserData &user_data = *(SLC::stUserData *)userdata;
		pthread_mutex_lock(&_rb_mutex);
		int ret = ring_buffer_write(&user_data.rb, message->payload, message->payloadlen);
		pthread_mutex_unlock(&_rb_mutex);
		if (ret > 0) {
			sem_post(&_rb_process_sem);
		} else {
			// ring buffer is full
			// drop this message
			drop_counts++;
		}
	} else {
	}
}

static void _connect_callback(struct mosquitto *mosq, void *userdata, int result) {
	if (!result) {
		mosquitto_subscribe(mosq, NULL,
				"Device/UC/Test/General/TestEventLogPerformanceData", 0);
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

static void _send_uc_communication_status(struct mosquitto *mosq, uint32_t val) {
	// Device/UC/Status/CommunicationStatus
	int mid = 0;
	uint8_t buffer[64];
	uint32_t size = 0;
	Uint32Value ui32val;
	Payload payload;
	payload.p_value = &ui32val;
	payload.id = 0x6CED9F98;
	payload.producer_mask = 0x02;
	payload.action = 0;
	ui32val.value = val;

	if (encode_payload(buffer, sizeof(buffer), size, payload)) {
		if (mosquitto_publish(mosq, &mid,
				"Device/UC/Status/CommunicationStatus", size, buffer,
				0, false) != MOSQ_ERR_SUCCESS) {
			printf("publish UC communication status failed\n");
		}
	}
}

static void _send_event_log_to_nmc(struct mosquitto *mosq, uint32_t val) {
	int mid = 0;
	uint8_t buffer[1024];
	uint32_t size = 0;
	// {3146049, 3, 20, 1, 1537017285, 0, 0, 610814367, 445, 0, 0, 1, 0, 0, 0, 2, 1, 610814129, 414, ''}
	EventLogDataToNmcValue event_log_val;
	event_log_val.event_id = 3146049;
	event_log_val.data_value = val;
	Payload payload;
	payload.p_value = &event_log_val;
	payload.id = 0x6CED9F98;
	payload.producer_mask = 0x02;
	payload.action = 0;

	if (encode_payload(buffer, sizeof(buffer), size, payload)) {
		if (mosquitto_publish(mosq, &mid,
				"UPSSystem/General/EventLogDataToNMC", size, buffer,
				0, false) != MOSQ_ERR_SUCCESS) {
			printf("publish event log failed\n");
		}
	}
}

static void *_process_msg_task(void *ptr) {
	int dln;
	void *data;
	uint8_t backup_data[MAX_MSG_SIZE];
	SLC::stUserData &user_data = *(SLC::stUserData *)ptr;
	_send_uc_communication_status(user_data.mosq, 2);
	while (true) {
		if (sem_wait(&_rb_process_sem) == 0) {
			pthread_mutex_lock(&_rb_mutex);
			dln = ring_buffer_read(&user_data.rb, &data);
			if (dln > 0) {
				memcpy(backup_data, data, dln);
			}
			pthread_mutex_unlock(&_rb_mutex);
			if (dln > 0) {
				Payload &payload = user_data.payload;
				if (decode_payload((const uint8_t *) backup_data,
						dln, payload)) {
					Uint32Value *val = dynamic_cast<Uint32Value *>(payload.p_value);
					_send_event_log_to_nmc(user_data.mosq, val->value);
				} else {
					fprintf(stderr, "parse event log data gram failed\n");
				}
			}
		}
	}
	return nullptr;
}

int slc_node_demo_main(int argc, const char *argv[]) {
	const char *host = "169.254.5.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;

	SLC::stUserData user_data;
	pthread_t thread;
	user_data.payload.p_value = &user_data.ui32val;

	sem_init(&_rb_process_sem, 0, 0);
	pthread_mutex_init(&_rb_mutex, NULL);
	mosquitto_lib_init();
	mosq = mosquitto_new("slc_node", clean_session, &user_data);
	if (mosq) {
		mosquitto_connect_callback_set(mosq, _connect_callback);
		mosquitto_message_callback_set(mosq, _message_callback);
		mosquitto_subscribe_callback_set(mosq, _subscribe_callback);
		user_data.mosq = mosq;
		if (mosquitto_username_pw_set(mosq, constant_clients[e_CID_SLC].username, constant_clients[e_CID_SLC].password)
				== MOSQ_ERR_SUCCESS) {
			if (mosquitto_connect(mosq, host, port, keepalive)
					== MOSQ_ERR_SUCCESS) {
				// start other threads at here
				if (ring_buffer_init(&user_data.rb, MAX_MSG_SIZE, MAX_BUFFER_SIZE) == 0) {
					if (pthread_create(&thread, nullptr, _process_msg_task, &user_data) == 0) {
						mosquitto_loop_forever(mosq, -1, 1);
						pthread_cancel(thread);
						pthread_join(thread, nullptr);
					}
					ring_buffer_free(&user_data.rb);
				}
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
	pthread_mutex_destroy(&_rb_mutex);
	sem_destroy(&_rb_process_sem);
	fflush(stderr);
	mosquitto_lib_cleanup();

	return 0;
}



