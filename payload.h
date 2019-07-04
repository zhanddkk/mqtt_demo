/*
 * payload.h
 *
 *  Created on: May 7, 2019
 *      Author: zhanlei
 */

#ifndef PAYLOAD_H_
#define PAYLOAD_H_

#include <stdint.h>
#include <string>

#include "value.h"

class Payload {
public:
	enum MapKey
	{
	    E_MAP_KEY_TYPE = 0,
	    E_MAP_KEY_VERSION = 1,
	    E_MAP_KEY_ID = 2,
	    E_MAP_KEY_PRODUCER_MASK = 3,
	    E_MAP_KEY_ACTION = 4,
	    E_MAP_KEY_TIME_SEC = 5,
	    E_MAP_KEY_TIME_MS = 6,
	    E_MAP_KEY_DEVICE_INDEX = 7,
	    E_MAP_KEY_OBJ_ID_H = 8,
	    E_MAP_KEY_OBJ_ID_L = 9,
	    E_MAP_KEY_VALUE = 10,
	};

	Payload();
	virtual ~Payload();

	void ptint();

    uint32_t type;
    uint32_t version;
    uint32_t id;
    uint32_t producer_mask;
    uint32_t action;
    uint32_t device_index;
    uint32_t time_sec;
    uint32_t time_ms;
    uint32_t obj_id_h;
    uint32_t obj_id_l;

    BasicValue *p_value;
};

extern bool encode_payload(uint8_t *buf, uint32_t size, uint32_t &used_size, const Payload &payload);
extern bool decode_payload(const uint8_t *buf, uint32_t size, Payload &payload);
#endif /* PAYLOAD_H_ */
