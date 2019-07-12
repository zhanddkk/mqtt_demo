/*
 * value.h
 *
 *  Created on: May 16, 2019
 *      Author: zhanlei
 */

#ifndef VALUE_H_
#define VALUE_H_

#include <vector>
#include <tinycbor/cbor.h>

class BasicValue {
public:
	BasicValue() {

	}

	virtual ~BasicValue() {

	}

	virtual bool encode(CborEncoder &encoder) {
		return true;
	}
	virtual bool decode(CborValue &cbor_val) {
		return true;
	}
	virtual void print(uint32_t deep = 0) {
	}
};

class Uint32Value: public BasicValue {
public:
	Uint32Value() {
		value = 0;
	}
	virtual ~Uint32Value() {

	}

	virtual bool decode(CborValue &cbor_val) {
		uint64_t val;
		bool ret = cbor_value_is_unsigned_integer(&cbor_val);
		ret = ret && (cbor_value_get_uint64(&cbor_val, &val) == CborNoError);
		if (ret) {
			value = (uint32_t) val;
			ret = cbor_value_advance_fixed(&cbor_val) == CborNoError;
		}
		return ret;
	}

	virtual void print(uint32_t deep = 0) {
		for (uint32_t i = 0; i < deep; i++) {
			printf(" ");
		}
		printf("%u\n", value);
	}
	uint32_t value;
};

class EventLogDataToNmcValue: public BasicValue {
public:
	EventLogDataToNmcValue() {
		event_id = 0;
		main_group_id = 0;
		sub_group_id = 0;
		index_sub_group = 0;
		hash_id = 0;
		source_device_type = 0;
		device_instance = 0;
		trg_tm_scd = 0;
		trg_tm_ms = 0;
		b_risky_log = 0;
		b_customer_log = 0;
		b_service_log = 0;
		b_nmc_log = 0;
		object_id_high_word = 0;
		object_id_low_word = 0;
		severity = 0;
		data_value = 0;
		log_tm_scd = 0;
		log_tm_ms = 0;
		r_d_information[0] = '\0';
	}
	virtual ~EventLogDataToNmcValue() {

	}

	virtual bool encode(CborEncoder &encoder) {
		CborEncoder array_encoder;
		CborError err = cbor_encoder_create_array(&encoder, &array_encoder, 20);
		bool ret = cbor_encode_uint(&array_encoder, event_id) == CborNoError;
		ret = ret && (cbor_encode_uint(&array_encoder, main_group_id) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, sub_group_id) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, index_sub_group) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, hash_id) == CborNoError);

		ret = ret && (cbor_encode_uint(&array_encoder, source_device_type) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, device_instance) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, trg_tm_scd) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, trg_tm_ms) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, b_risky_log) == CborNoError);

		ret = ret && (cbor_encode_uint(&array_encoder, b_customer_log) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, b_service_log) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, b_nmc_log) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, object_id_high_word) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, object_id_low_word) == CborNoError);

		ret = ret && (cbor_encode_uint(&array_encoder, severity) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, data_value) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, log_tm_scd) == CborNoError);
		ret = ret && (cbor_encode_uint(&array_encoder, log_tm_ms) == CborNoError);
		ret = ret && (cbor_encode_text_stringz(&array_encoder, r_d_information) == CborNoError);

		if (err == CborNoError) {
			err = cbor_encoder_close_container(&encoder, &array_encoder);
		}
		return err == CborNoError;
	}

	virtual bool decode(CborValue &cbor_val) {
		bool ret = false;
		if (cbor_value_is_array(&cbor_val)) {
			CborValue recursed;
			CborError err = cbor_value_enter_container(&cbor_val, &recursed);
			if (err == CborNoError) {
				Uint32Value ui32_val;

				ret = ui32_val.decode(recursed);
				if (ret) {
					event_id = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					main_group_id = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					sub_group_id = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					index_sub_group = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					hash_id = ui32_val.value;
				}

				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					source_device_type = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					device_instance = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					trg_tm_scd = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					trg_tm_ms = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					b_risky_log = ui32_val.value;
				}

				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					b_customer_log = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					b_service_log = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					b_nmc_log = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					object_id_high_word = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					object_id_low_word = ui32_val.value;
				}

				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					severity = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					data_value = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					log_tm_scd = ui32_val.value;
				}
				ret = ret && ui32_val.decode(recursed);
				if (ret) {
					log_tm_ms = ui32_val.value;
				}

				if (ret && cbor_value_is_text_string(&recursed)) {
					size_t buffer_len = sizeof(r_d_information);
					err = cbor_value_copy_text_string(&recursed, r_d_information, &buffer_len, &recursed);
					if (err == CborNoError) {
						if (buffer_len > 0 && (buffer_len == sizeof(r_d_information))) {
							r_d_information[buffer_len - 1] = '\0';
						}
					} else {
						ret = false;
					}
				}

				ret = ret && (cbor_value_leave_container(&cbor_val, &recursed) == CborNoError);
			}
		}
		return ret;
	}

    uint32_t event_id;
    uint32_t main_group_id;
    uint32_t sub_group_id;
    uint32_t index_sub_group;
    uint32_t hash_id;

    uint32_t source_device_type;
    uint32_t device_instance;
    uint32_t trg_tm_scd;
    uint32_t trg_tm_ms;
    uint32_t b_risky_log;

    uint32_t b_customer_log;
    uint32_t b_service_log;
    uint32_t b_nmc_log;
    uint32_t object_id_high_word;
    uint32_t object_id_low_word;

    uint32_t severity;
    uint32_t data_value;
    uint32_t log_tm_scd;
    uint32_t log_tm_ms;
    char r_d_information[256];
};

class EventLogDataToNmcArrayValue: public BasicValue {
public:
	EventLogDataToNmcArrayValue() {
		m_val_counts = 0;
	}

	virtual ~EventLogDataToNmcArrayValue() {

	}

	const EventLogDataToNmcValue &get_val(int index) {
		return m_val[index];
	}

	int val_counts() {
		return m_val_counts;
	}

	virtual bool decode(CborValue &cbor_val) {
		bool ret = false;
		if (cbor_value_is_array(&cbor_val)) {
			CborValue recursed;
			CborError err = cbor_value_enter_container(&cbor_val, &recursed);
			if (err == CborNoError) {
				ret = true;
				for (m_val_counts = 0;
					((uint32_t)m_val_counts < sizeof(m_val) / sizeof(EventLogDataToNmcValue)) && (!cbor_value_at_end(&recursed));
					m_val_counts++) {
					if (!m_val[m_val_counts].decode(recursed)) {
						m_val_counts = 0;
						ret = false;
						break;
					}
				}
				ret = ret && (cbor_value_leave_container(&cbor_val, &recursed) == CborNoError);
			}
		}
		return ret;
	}

private:
	EventLogDataToNmcValue m_val[256];
	int m_val_counts;
};
#endif /* VALUE_H_ */
