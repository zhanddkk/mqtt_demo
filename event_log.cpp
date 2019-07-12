/*
 * event_log.cpp
 *
 *  Created on: May 17, 2019
 *      Author: zhanlei
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <mosquitto.h>
#include <pthread.h>
#include <semaphore.h>
#include <list>
#include <memory>
#include <sys/timerfd.h>
#include "event_log.h"
//#include "ring_buffer.h"
#include "client_info.h"

#define MAX_EVENT_ID		200000
#define _TMP_TO_TXT(num)	#num
#define TO_TXT(VAL)			_TMP_TO_TXT(VAL)

namespace EventLog {

struct stMemPage {
	int size;
	std::shared_ptr<uint8_t> data;
	stMemPage() {
		size = 0;
	}

	stMemPage(const stMemPage &d) {
		*this = d;
	}

	const stMemPage &operator= (const stMemPage &d) {
		size = d.size;
		data = d.data;
		return *this;
	}
};

struct stUserData {
	Payload payload;
	EventLogDataToNmcArrayValue event_log_val;
	std::list<struct stMemPage> buffer;
	int current_counts;
	int max_buffer_size;
	int max_write_counts;
	EventLogDb *db;
};

EventLogDb::EventLogDb(const char *filename) {
	m_filename = filename;
	m_pdb = nullptr;
	m_id = -1;
	m_max_id = MAX_EVENT_ID;
	m_counts = 0;
}

EventLogDb::~EventLogDb() {
	if (m_pdb) {
		sqlite3_close(m_pdb);
	}
}

bool EventLogDb::init() {
	bool ret = false;
	if (sqlite3_open_v2(m_filename, &m_pdb,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_WAL,
		nullptr) == SQLITE_OK) {
		if (init_event_table() && init_control_table()) {
			if (get_control_data(m_id, m_max_id)) {
			} else {
				m_id = -1;
				m_max_id = MAX_EVENT_ID;
			}
			ret = true;
		}
	}

	if (ret) {
		printf("init db OK(id = %d, max = %d)\n", m_id, m_max_id);
		m_counts = m_id;
	} else {
		fprintf(stderr, "init db failed\n");
	}
	return ret;
}

bool EventLogDb::process(const EventLogDataToNmcValue *val) {
	bool ret = false;
#if __WORDSIZE == 64
	const char code[] = "INSERT INTO events VALUES ("
		"%d,"	"%d,"	"%u,"	"%u,"	"%u,"
		"%d,"	"%d,"	"%d,"	"%d,"	"%d,"
		"%ld,"	"%ld,"	"%d,"	"%d,"	"%d,"
		"%d,"	"%d,"	"%d,"	"%lu,"	"%d,"
		"%d,"	"%d,"	"\'%s\'"
		");UPDATE control SET current = %d WHERE id = 0;";
#else
	const char code[] = "INSERT INTO events VALUES ("
		"%d,"	"%d,"	"%u,"	"%u,"	"%u,"
		"%d,"	"%d,"	"%d,"	"%d,"	"%d,"
		"%ld,"	"%ld,"	"%d,"	"%d,"	"%d,"
		"%d,"	"%d,"	"%d,"	"%llu,"	"%d,"
		"%d,"	"%d,"	"\'%s\'"
		");UPDATE control SET current = %d WHERE id = 0;";
#endif
	struct timespec ts = { 0, 0 };
	int next_id = m_id + 1;

	clock_gettime(CLOCK_REALTIME, &ts);
	snprintf(buffer, sizeof(buffer), code,
		next_id, next_id, /* val->event_number, */
		val->event_id, val->main_group_id,
		val->sub_group_id, val->index_sub_group,
		0, /* val->text_reference_id, */ val->hash_id,
		val->source_device_type, val->device_instance,
		ts.tv_sec, /* val->log_tm_scd, */(ts.tv_nsec / 100000 + 5) / 10, /* val->log_tm_ms, */
		val->trg_tm_scd, val->trg_tm_ms,
		val->b_risky_log, val->b_customer_log,
		val->b_service_log, val->b_nmc_log,
		(((uint64_t) val->object_id_high_word << 32)
			| val->object_id_low_word), val->severity, 0, /* val->bookmark_id, */
		val->data_value, val->r_d_information, next_id);

	char *errmsg = nullptr;
	int exe_ret = sqlite3_exec(m_pdb, buffer, nullptr, nullptr, &errmsg);
	if (errmsg) {
		fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
		sqlite3_free(errmsg);
	} else {
		ret = true;
	}
	m_id = next_id;
	return ret;
}

int EventLogDb::_test_table_callback(void *userdata, int column_num,
	char **column_text, char **column_name) {
	bool *ret = (bool *) userdata;
	if (ret && (column_num >= 1) && column_text) {
		if (column_text[0]) {
			*ret = atoi(column_text[0]) > 0;
		}
	}
	return 0;
}

int EventLogDb::_get_control_data_callback(void *userdata, int column_num,
	char **column_text, char **column_name) {
	int *data = (int *) userdata;
	if (data && (column_num == 2) && column_text) {
		for (int i = 0; i < column_num; i++) {
			if (column_text[i]) {
				data[i] = atoi(column_text[i]);
			}
		}
	}
	return 0;
}

bool EventLogDb::is_table_exist(std::string table_name) {
	std::string code =
		"SELECT count(*) FROM sqlite_master WHERE type=\'table\' AND name=\'"
			+ table_name + "\';";
	char *errmsg = nullptr;
	bool ret = false;
	int exe_ret = sqlite3_exec(m_pdb, code.c_str(), _test_table_callback,
		&ret, &errmsg);
	if (errmsg) {
		fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
		sqlite3_free(errmsg);
	}
	return ret;
}

bool EventLogDb::get_control_data(int &current, int &max) {
	bool ret;
	int data[2] = { -1, -1 };
	const char *code = "SELECT current, max FROM control WHERE id = 0;";
	char *errmsg = nullptr;
	int exe_ret = sqlite3_exec(m_pdb, code, _get_control_data_callback,
		data, &errmsg);
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

bool EventLogDb::init_event_table() {
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
		int exe_ret = sqlite3_exec(m_pdb, code, nullptr, nullptr, &errmsg);
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

bool EventLogDb::init_control_table() {
	bool ret = false;
	if (is_table_exist("control")) {
		ret = true;
	} else {
		const char *code =
			"BEGIN;"
				"CREATE TABLE control ("
				"id      INTEGER PRIMARY KEY,"
				"current INTEGER,"
				"max     INTEGER"
				"); INSERT INTO control VALUES(0, 0, " TO_TXT(MAX_EVENT_ID) ");COMMIT;";
		char *errmsg = nullptr;
		int exe_ret = sqlite3_exec(m_pdb, code, nullptr, nullptr, &errmsg);
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

bool EventLogDb::start_input() {
	bool ret = false;
	char *errmsg = nullptr;
	int exe_ret = sqlite3_exec(m_pdb, "BEGIN;", nullptr, nullptr, &errmsg);
	if (errmsg) {
		fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
		sqlite3_free(errmsg);
	} else {
		ret = true;
	}
	return ret;
}

bool EventLogDb::end_input() {
	bool ret = false;
	char *errmsg = nullptr;
	int exe_ret = sqlite3_exec(m_pdb, "COMMIT;", nullptr, nullptr, &errmsg);
	if (errmsg) {
		fprintf(stderr, "%d: %s\n", exe_ret, errmsg);
		sqlite3_free(errmsg);
	} else {
		ret = true;
	}
	return ret;
}
}

// =================================================================================================//
// Main code
// -------------------------------------------------------------------------------------------------//
#define MAX_BUFFER_SIZE	5000

namespace EventLog{
struct stPeriodicInfo{
	int timer_fd;
	uint64_t wakeups_missed;
};
}

static pthread_mutex_t _rb_mutex;
static uint32_t drop_counts = 0;

static void _message_callback(struct mosquitto *mosq, void *userdata,
	const struct mosquitto_message *message) {
	bool ret = false;
	if (message->payloadlen) {
		EventLog::stUserData &user_data = *(EventLog::stUserData *) userdata;
		pthread_mutex_lock(&_rb_mutex);
		if (user_data.buffer.size() < (uint32_t)user_data.max_buffer_size) {
			EventLog::stMemPage mp;
			mp.size = message->payloadlen;
			uint8_t *pd = new uint8_t[mp.size];
			if (pd) {
				memcpy(pd, message->payload, mp.size);
				mp.data = std::shared_ptr<uint8_t>(pd);
				user_data.buffer.push_back(mp);
				ret = true;
			}
		}
		pthread_mutex_unlock(&_rb_mutex);
		if (ret > 0) {

		} else {
			// ring buffer is full
			// drop this message
			drop_counts++;
		}
	} else {
	}
}

static void _connect_callback(struct mosquitto *mosq, void *userdata,
	int result) {
	if (!result) {
		mosquitto_subscribe(mosq, NULL, "UPSSystem/General/EventlogArray",
			0);
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

static int _make_periodic(uint32_t ms_period, EventLog::stPeriodicInfo *info)
{
    int ret = -1;
    uint32_t ns;
    uint32_t sec;
    int fd;
    struct itimerspec itval;

    // Create the timer
    fd = timerfd_create (CLOCK_MONOTONIC, 0);
    info->wakeups_missed = 0;
    info->timer_fd = fd;

    if (fd >= 0) {
		// Make the timer periodic
		sec = ms_period / 1000u;
		ns = (ms_period % 1000u) * 1000000u;
		itval.it_interval.tv_sec = sec;
		itval.it_interval.tv_nsec = ns;
		itval.it_value.tv_sec = sec;
		itval.it_value.tv_nsec = ns;
		ret = timerfd_settime (fd, 0, &itval, NULL);
    }
    return ret;
}

static void *_event_log_process_task(void *ptr) {
	EventLog::stUserData &user_data = *(EventLog::stUserData *) ptr;
	Payload &payload = user_data.payload;
	EventLog::EventLogDb &db = *user_data.db;
	int start_index = 0;
	EventLogDataToNmcArrayValue *val = nullptr;
	EventLog::stPeriodicInfo info;
	bool is_timer_started = false;
	uint64_t missed;
	while (true) {
		// Start timer
		if (!is_timer_started) {
			if (_make_periodic(1000, &info) == 0) {
				is_timer_started = true;
			} else {
				// Set time failed, but create the timer succeed
				close(info.timer_fd);
			}
		}

		// Process timer event
		if (is_timer_started) {
			// Wait the alarm of the timer
			int ret = read(info.timer_fd, &missed, sizeof(missed));
			if (ret == -1) {
				// Read failed
				// Release resource
				close(info.timer_fd);
				info.wakeups_missed = 0;
				is_timer_started = false;
			} else {
				int write_counts = 0;
				bool has_write = false;
				struct timespec ts = { 0, 0 };
				clock_gettime(CLOCK_MONOTONIC, &ts);
				while (true) {
					if (val == nullptr) {
						pthread_mutex_lock(&_rb_mutex);
						uint32_t size = user_data.buffer.size();
						pthread_mutex_unlock(&_rb_mutex);

						if (size) {
							pthread_mutex_lock(&_rb_mutex);
							EventLog::stMemPage mp = user_data.buffer.front();
							user_data.buffer.pop_front();
							pthread_mutex_unlock(&_rb_mutex);
							if (mp.size > 0) {
								if (decode_payload(
									(const uint8_t *) mp.data.get(), mp.size,
									payload)) {
									val = dynamic_cast<EventLogDataToNmcArrayValue *>(payload.p_value);
									start_index = 0;
								} else {
									fprintf(stderr,
										"parse event log data gram failed\n");
								}
							}
						} else {
							break;
						}
					}

					if (val) {
						while (true) {
							if (start_index >= val->val_counts()) {
								val = nullptr;
								start_index = 0;
								break;
							}

							if (write_counts >= user_data.max_write_counts) {
								break;
							}
							if (!has_write) {
								db.start_input();
								has_write = true;
							}
							db.process(&val->get_val(start_index++));
							write_counts++;
						}
						if (val) {
							break;
						}
					}
				}

				if (has_write) {
					pthread_mutex_lock(&_rb_mutex);
					uint32_t size = user_data.buffer.size();
					pthread_mutex_unlock(&_rb_mutex);
					printf("[%ld.%09ld] W: %4d -> %4d (drop = %4d, queued = %4d)\n", ts.tv_sec,
						ts.tv_nsec, start_index, write_counts, drop_counts, size);
					db.end_input();
				}
			}
		}
	}
	return nullptr;
}

int event_log_demo_main(int argc, const char *argv[]) {
	const char *host = "169.254.5.1";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	EventLog::stUserData user_data;
	const char *db_file = "test.db";
	bool ret = false;
	user_data.max_buffer_size = 500;
	user_data.max_write_counts = 1000;
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--db") == 0) {
			i++;
			if (i < argc) {
				db_file = argv[i];
			} else {
				break;
			}
		} else if (strcmp(argv[i], "--max-write-counts") == 0) {
			i++;
			if (i < argc) {
				int n = atoi(argv[i]);
				if (n >= 0) {
					user_data.max_write_counts = n;
				}
			} else {
				break;
			}
		} else if (strcmp(argv[i], "--max-buffer-size") == 0) {
			i++;
			if (i < argc) {
				int n = atoi(argv[i]);
				if (n > 0) {
					user_data.max_buffer_size = n;
				}
			} else {
				break;
			}
		} else {

		}
	}

	user_data.db = new EventLog::EventLogDb(db_file);
	if (user_data.db) {
		ret = user_data.db->init();
	} else {
		fprintf(stderr, "New EventLog DB failed\n");
	}

	if (ret) {
		pthread_t thread;

		user_data.payload.p_value =
			dynamic_cast<BasicValue *>(&user_data.event_log_val);
		pthread_mutex_init(&_rb_mutex, NULL);
		mosquitto_lib_init();
		mosq = mosquitto_new("event_log", clean_session, &user_data);
		if (mosq) {
			mosquitto_connect_callback_set(mosq, _connect_callback);
			mosquitto_message_callback_set(mosq, _message_callback);
			mosquitto_subscribe_callback_set(mosq, _subscribe_callback);
			if (mosquitto_username_pw_set(mosq,
				constant_clients[e_CID_NMC1].username,
				constant_clients[e_CID_NMC1].password) == MOSQ_ERR_SUCCESS) {
				if (mosquitto_connect(mosq, host, port, keepalive)
					== MOSQ_ERR_SUCCESS) {
					// start other threads at here
					if (pthread_create(&thread, nullptr,
						_event_log_process_task, &user_data) == 0) {
						mosquitto_loop_forever(mosq, -1, 1);
						pthread_cancel(thread);
						pthread_join(thread, nullptr);
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



