/*
 * event_log.h
 *
 *  Created on: Jul 10, 2019
 *      Author: zhanlei
 */

#ifndef EVENT_LOG_H_
#define EVENT_LOG_H_

#include <sqlite3.h>
#include <string>

#include "payload.h"

namespace EventLog {
class EventLogDb {
public:
	EventLogDb(const char *filename);
	virtual ~EventLogDb();
	bool init();
	bool process(const EventLogDataToNmcValue *val);
	bool start_input();
	bool end_input();
private:
	const char *m_filename;
	sqlite3 *m_pdb;
	int m_id;
	int m_max_id;
	int m_counts;
	char buffer[1024];
//	struct timespec m_update_ts;

	static int _test_table_callback(void *userdata, int column_num,
		char **column_text, char **column_name);
	static int _get_control_data_callback(void *userdata, int column_num,
		char **column_text, char **column_name);
	bool is_table_exist(std::string table_name);
	bool get_control_data(int &current, int &max);
	bool init_event_table();
	bool init_control_table();
};
}


#endif /* EVENT_LOG_H_ */
