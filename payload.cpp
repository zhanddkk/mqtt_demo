/*
 * payload.cpp
 *
 *  Created on: May 7, 2019
 *      Author: zhanlei
 */

#include "payload.h"

static bool encode_payload_header(CborEncoder &encoder, const Payload &payload)
{
	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_TYPE);
	cbor_encode_uint(&encoder, payload.type);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_VERSION);
	cbor_encode_uint(&encoder, payload.version);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_ID);
	cbor_encode_uint(&encoder, payload.id);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_PRODUCER_MASK);
	cbor_encode_uint(&encoder, payload.producer_mask);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_ACTION);
	cbor_encode_uint(&encoder, payload.action);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_TIME_SEC);
	cbor_encode_uint(&encoder, payload.time_sec);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_TIME_MS);
	cbor_encode_uint(&encoder, payload.time_ms);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_OBJ_ID_H);
	cbor_encode_uint(&encoder, payload.obj_id_h);

	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_OBJ_ID_L);
	cbor_encode_uint(&encoder, payload.obj_id_l);

	// !important: device index start from 1 outside SLC
	cbor_encode_uint(&encoder, Payload::E_MAP_KEY_DEVICE_INDEX);
	cbor_encode_uint(&encoder, payload.device_index + 1);
	return true;
}

static bool decode_payload_item(uint32_t key, CborValue &it, Payload &payload)
{
	bool ret_val = false;

	if (key == Payload::E_MAP_KEY_VALUE) {
		ret_val = payload.p_value->decode(it);
	} else {
		Uint32Value ui32_val;
		switch (key) {
		case Payload::E_MAP_KEY_TYPE:
			ret_val = ui32_val.decode(it);
			payload.type = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_VERSION:
			ret_val = ui32_val.decode(it);
			payload.version = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_ID:
			ret_val = ui32_val.decode(it);
			payload.id = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_PRODUCER_MASK:
			ret_val = ui32_val.decode(it);
			payload.producer_mask = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_ACTION:
			ret_val = ui32_val.decode(it);
			payload.action = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_TIME_SEC:
			ret_val = ui32_val.decode(it);
			payload.time_sec = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_TIME_MS:
			ret_val = ui32_val.decode(it);
			payload.time_ms = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_DEVICE_INDEX:
			ret_val = ui32_val.decode(it);
			payload.device_index = ui32_val.value - 1;
			break;
		case Payload::E_MAP_KEY_OBJ_ID_H:
			ret_val = ui32_val.decode(it);
			payload.obj_id_h = ui32_val.value;
			break;
		case Payload::E_MAP_KEY_OBJ_ID_L:
			ret_val = ui32_val.decode(it);
			payload.obj_id_l = ui32_val.value;
			break;
		default:
			ret_val = cbor_value_advance(&it) == CborNoError;
			break;
		}
	}
	return ret_val;
}

bool encode_payload(uint8_t *buf, uint32_t size, uint32_t &used_size, const Payload &payload)
{
	bool ret_val = false;
	CborEncoder root_encoder;
	CborEncoder payload_encoder;

	cbor_encoder_init(&root_encoder, buf, size, 0);
	cbor_encoder_create_map(&root_encoder, &payload_encoder, 11);
	encode_payload_header(payload_encoder, payload);

	if (payload.p_value) {
		cbor_encode_uint(&payload_encoder, Payload::E_MAP_KEY_VALUE);
		ret_val = payload.p_value->encode(payload_encoder);
	}
	ret_val = ret_val & (cbor_encoder_close_container(&root_encoder, &payload_encoder) == CborNoError);
	if (ret_val) {
		used_size = cbor_encoder_get_buffer_size(&root_encoder, buf);
	}
	return ret_val;
}

bool decode_payload(const uint8_t *buf, uint32_t size, Payload &payload)
{
	CborParser parser;
	CborValue payload_map;
	CborValue items;

	bool ret_val = cbor_parser_init(buf, size, 0, &parser, &payload_map) == CborNoError;
	ret_val = ret_val && (cbor_value_get_type(&payload_map) == CborMapType);
	ret_val = ret_val && (cbor_value_enter_container(&payload_map, &items) == CborNoError);
	if (ret_val) {
		bool is_key = true;
		uint32_t key = 0;
		Uint32Value ui32_val;
		while (!cbor_value_at_end(&items)) {
			if (is_key) {
				if (ui32_val.decode(items)) {
					key = ui32_val.value;
					is_key = false;
				} else {
					ret_val = cbor_value_advance(&items) == CborNoError;
					if (!ret_val) {
						break;
					}
				}
			} else {
				ret_val = decode_payload_item(key, items, payload);
				if (ret_val) {
					is_key = true;
				} else {
					break;
				}
			}
		}
	}
	return ret_val;
}

Payload::Payload()
{
	type = 0;
    version = 0;
    id = 0;
    producer_mask = 0;
    action = 0;
    device_index = 0;
    time_sec = 0;
    time_ms = 0;
    obj_id_h = 0;
    obj_id_l = 0;
    p_value = nullptr;
}

Payload::~Payload()
{

}

void Payload::ptint()
{
	 printf("type          %u\n", type);
	 printf("version       %u\n", version);
	 printf("id            0x%08X\n", id);
	 printf("producer_mask 0x%02X\n", producer_mask);
	 printf("action        %u\n", action);
	 printf("device_index  %u\n", device_index + 1);
	 printf("time_sec      %u\n", time_sec);
	 printf("time_ms       %u\n", time_ms);
	 printf("obj_id_h      0x%08X\n", obj_id_h);
	 printf("obj_id_l      0x%08X\n", obj_id_l);
	 if (p_value) {
		 printf("value\n");
		 p_value->print(1);
	 }
}


