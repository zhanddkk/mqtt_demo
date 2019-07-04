/*
 * nmc_node.cpp
 *
 *  Created on: Jul 1, 2019
 *      Author: zhanlei
 */

#include <stdint.h>
#include <stdio.h>
#include <sqlite3.h>
#include <unistd.h>
#include <time.h>
#include <mosquitto.h>
#include <pthread.h>
#include <semaphore.h>

#include <string>

#include "payload.h"
#include "ring_buffer.h"
#include "client_info.h"

#define MAX_EVENT_ID		200000
#define _TMP_TO_TXT(num)	#num
#define TO_TXT(VAL)			_TMP_TO_TXT(VAL)
namespace NMC {
class EventLogDb {
public:
	EventLogDb(const char *filename) {
		m_filename = filename;
		m_pdb = nullptr;
		m_id = -1;
		m_max_id = MAX_EVENT_ID;
		m_update_ts = {0, 0};
		m_counts = 0;
	}

	virtual ~EventLogDb() {
		if (m_pdb) {
			sqlite3_close(m_pdb);
		}
	}

	bool init() {
		bool ret = false;
		if (sqlite3_open_v2(m_filename, &m_pdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_WAL, nullptr) == SQLITE_OK) {
			if (init_event_table() && init_control_table()) {
				if (get_control_data(m_id, m_max_id)) {
				} else {
					m_id = -1;
					m_max_id = MAX_EVENT_ID;
				}
				ret = true;
			}
		}

		if (ret && start_input()) {
			printf("init db OK(id = %d, max = %d)\n", m_id, m_max_id);
			m_counts = m_id;
		} else {
			fprintf(stderr, "init db failed\n");
		}
		return ret;
	}

	bool process(EventLogDataToNmcValue *val) {
		bool ret = false;
		const char code[] =
			"INSERT INTO events VALUES ("
				"%d,"
				"%d,"
				"%u,"
				"%u,"
				"%u,"
				"%d,"
				"%d,"
				"%d,"
				"%d,"
				"%d,"
				"%ld,"
				"%ld,"
				"%d,"
				"%d,"
				"%d,"
				"%d,"
				"%d,"
				"%d,"
				"%llu,"
				"%d,"
				"%d,"
				"%d,"
				"\'%s\'"
			");UPDATE control SET current = %d WHERE id = 0;";
		struct timespec ts = {0, 0};
		int next_id = m_id + 1;

        clock_gettime(CLOCK_REALTIME, &ts);
		snprintf(buffer, sizeof(buffer), code,
			next_id,
			next_id, /* val->event_number, */
			val->event_id,
			val->main_group_id,
			val->sub_group_id,
			val->index_sub_group,
			0, /* val->text_reference_id, */
			val->hash_id,
			val->source_device_type,
			val->device_instance,
			ts.tv_sec, /* val->log_tm_scd, */
			(ts.tv_nsec / 100000 + 5) / 10, /* val->log_tm_ms, */
			val->trg_tm_scd,
			val->trg_tm_ms,
			val->b_risky_log,
			val->b_customer_log,
			val->b_service_log,
			val->b_nmc_log,
			(unsigned long long)(((uint64_t)val->object_id_high_word << 32) | val->object_id_low_word),
			val->severity,
			0, /* val->bookmark_id, */
			val->data_value,
			val->r_d_information,
			next_id);
		if (ts.tv_sec >= (m_update_ts.tv_sec + 1)) {
			static uint32_t drop_counts;
			printf("[%ld.%ld]: %03d -> %d drop = %d\n", ts.tv_sec, ts.tv_nsec, m_id - m_counts, next_id, drop_counts);
			m_update_ts.tv_sec = ts.tv_sec;
			m_counts = m_id;
			end_input();
			start_input();
		}
//		printf("%s\n", buffer);

		char *errmsg = nullptr;
		int exe_ret = sqlite3_exec(
			m_pdb,
			buffer,
			nullptr,
			nullptr,
			&errmsg
			);
		if (errmsg) {
			fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
			sqlite3_free(errmsg);
		} else {
			ret = true;
		}
		m_id = next_id;
		return ret;
	}

private:
	const char *m_filename;
	sqlite3 *m_pdb;
	int m_id;
	int m_max_id;
	int m_counts;
	char buffer[1024];
	struct timespec m_update_ts;

	static int _test_table_callback(void *userdata, int column_num, char **column_text, char **column_name) {
		bool *ret = (bool *)userdata;
		if (ret && (column_num >= 1) && column_text) {
			if (column_text[0]) {
				*ret = atoi(column_text[0]) > 0;
			}
		}
		return 0;
	}

	static int _get_control_data_callback(void *userdata, int column_num, char **column_text, char **column_name) {
		int *data = (int *)userdata;
		if (data && (column_num == 2) && column_text) {
			for (int i = 0; i < column_num; i++) {
				if (column_text[i]) {
					data[i] = atoi(column_text[i]);
				}
			}
		}
		return 0;
	}

	bool is_table_exist(std::string table_name) {
		std::string code = "SELECT count(*) FROM sqlite_master WHERE type=\'table\' AND name=\'" + table_name + "\';";
		char *errmsg = nullptr;
		bool ret = false;
		int exe_ret = sqlite3_exec(
			m_pdb,
			code.c_str(),
			_test_table_callback,
			&ret,
			&errmsg
			);
		if (errmsg) {
			fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
			sqlite3_free(errmsg);
		}
		return ret;
	}

	bool get_control_data(int &current, int &max) {
		bool ret;
		int data[2] = {-1, -1};
		const char *code = "SELECT current, max FROM control WHERE id = 0;";
		char *errmsg = nullptr;
		int exe_ret = sqlite3_exec(
			m_pdb,
			code,
			_get_control_data_callback,
			data,
			&errmsg
			);
		if (errmsg) {
			fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
			sqlite3_free(errmsg);
		}
		if ((data[0] > -1) && (data[1]) > -1) {
			current = data[0];
			max = data[1];
			ret = true;
		} else {
			ret = false;
		}
		return ret;
	}

	bool init_event_table() {
		bool ret = false;
		if (is_table_exist("events")) {
			ret = true;
		} else {
			const char *code = "BEGIN;"
				"CREATE TABLE events ("
					"slot_id                 INTEGER PRIMARY KEY NOT NULL,"
					"event_number            INTEGER,"
					"event_code              INTEGER,"
					"main_group_id           INTEGER,"
					"sub_group_id            INTEGER,"
					"index_sub_group_id      INTEGER,"
					"text_reference_id       INTEGER,"
					"hash_id                 INTEGER,"
					"device_instance_type_id INTEGER,"
					"device_instance_id      INTEGER,"
					"log_seconds             INTEGER,"
					"log_milliseconds        INTEGER,"
					"trigger_seconds         INTEGER,"
					"trigger_milliseconds    INTEGER,"
					"risky_log               BOOLEAN,"
					"customer_log            BOOLEAN,"
					"service_log             BOOLEAN,"
					"nmc_log                 BOOLEAN,"
					"object_id               INTEGER,"
					"severity                INTEGER,"
					"bookmark_id             INTEGER,"
					"data_value              INTEGER,"
					"rd_information          TEXT"
				"); COMMIT;";
			char *errmsg = nullptr;
			int exe_ret = sqlite3_exec(
				m_pdb,
				code,
				nullptr,
				nullptr,
				&errmsg
				);
			if (errmsg) {
				fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
				sqlite3_free(errmsg);
			} else {
				printf("create events table OK\n");
				ret = true;
			}
		}
		return ret;
	}

	bool init_control_table() {
		bool ret = false;
		if (is_table_exist("control")) {
			ret = true;
		} else {
			const char *code = "BEGIN;"
				"CREATE TABLE control ("
					"id      INTEGER PRIMARY KEY,"
					"current INTEGER,"
					"max     INTEGER"
				"); INSERT INTO control VALUES(0, 0, " TO_TXT(MAX_EVENT_ID) ");COMMIT;";
			char *errmsg = nullptr;
			int exe_ret = sqlite3_exec(
				m_pdb,
				code,
				nullptr,
				nullptr,
				&errmsg
				);
			if (errmsg) {
				fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
				sqlite3_free(errmsg);
			} else {
				printf("create control table OK\n");
				ret = true;
			}
		}
		return ret;
	}

	bool start_input() {
		bool ret = false;
		char *errmsg = nullptr;
		int exe_ret = sqlite3_exec(
			m_pdb,
			"BEGIN;",
			nullptr,
			nullptr,
			&errmsg
			);
		if (errmsg) {
			fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
			sqlite3_free(errmsg);
		} else {
			ret = true;
		}
		return ret;
	}

	bool end_input() {
		bool ret = false;
		char *errmsg = nullptr;
		int exe_ret = sqlite3_exec(
			m_pdb,
			"COMMIT;",
			nullptr,
			nullptr,
			&errmsg
			);
		if (errmsg) {
			fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
			sqlite3_free(errmsg);
		} else {
			ret = true;
		}
		return ret;
	}
};

struct stUserData {
	Payload payload;
	EventLogDataToNmcValue event_log_val;
	bool write_db;
	struct stRingBuffer rb;
	EventLogDb *db;
};
}

// =================================================================================================//
// Main code
// -------------------------------------------------------------------------------------------------//
#define MAX_MSG_SIZE	1024
#define MAX_BUFFER_SIZE	2000

static pthread_mutex_t _rb_mutex;
static sem_t _rb_process_sem;
static uint32_t drop_counts = 0;

static void _message_callback(struct mosquitto *mosq, void *userdata,
		const struct mosquitto_message *message) {
	if (message->payloadlen) {
		NMC::stUserData &user_data = *(NMC::stUserData *)userdata;
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
				"UPSSystem/General/EventLogDataToNMC", 0);
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

static void *_event_log_process_task(void *ptr) {
	int dln;
	void *data;
	uint8_t backup_data[MAX_MSG_SIZE];
	NMC::stUserData &user_data = *(NMC::stUserData *)ptr;
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
				NMC::EventLogDb &db = *user_data.db;
				if (decode_payload((const uint8_t *) backup_data,
						dln, payload)) {
					EventLogDataToNmcValue *val = dynamic_cast<EventLogDataToNmcValue *>(payload.p_value);
					if (user_data.write_db) {
						db.process(val);
					}
				} else {
					fprintf(stderr, "parse event log data gram failed\n");
				}
			}
		}
	}
	return nullptr;
}

int nmc_node_demo_main(int argc, const char *argv[]) {
	const char *host = "169.254.5.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	NMC::stUserData user_data;
	const char *db_file = "test.db";
	bool ret = false;

	user_data.write_db = true;
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--db") == 0) {
			i++;
			if (i < argc) {
				db_file = argv[i];
			} else {
				break;
			}
		} else if (strcmp(argv[i], "--no-write") == 0) {
			user_data.write_db = false;
		} else {

		}
	}

	if (user_data.write_db) {
		user_data.db = new NMC::EventLogDb(db_file);
		if (user_data.db) {
			ret = user_data.db->init();
		} else {
			fprintf(stderr, "New EventLog DB failed\n");
		}
	} else {
		ret = true;
	}

	if (ret) {
		pthread_t thread;

		user_data.payload.p_value = dynamic_cast<BasicValue *>(&user_data.event_log_val);
		sem_init(&_rb_process_sem, 0, 0);
		pthread_mutex_init(&_rb_mutex, NULL);
		mosquitto_lib_init();
		mosq = mosquitto_new("nmc_node", clean_session, &user_data);
		if (mosq) {
			mosquitto_connect_callback_set(mosq, _connect_callback);
			mosquitto_message_callback_set(mosq, _message_callback);
			mosquitto_subscribe_callback_set(mosq, _subscribe_callback);
			if (mosquitto_username_pw_set(mosq, constant_clients[e_CID_NMC1].username, constant_clients[e_CID_NMC1].password)
					== MOSQ_ERR_SUCCESS) {
				if (mosquitto_connect(mosq, host, port, keepalive)
						== MOSQ_ERR_SUCCESS) {
					// start other threads at here
					if (ring_buffer_init(&user_data.rb, MAX_MSG_SIZE, MAX_BUFFER_SIZE) == 0) {
						if (pthread_create(&thread, nullptr, _event_log_process_task, &user_data) == 0) {
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
	} else {

	}
	if (user_data.db) {
		delete user_data.db;
		user_data.db = nullptr;
	}
	return 0;
}



