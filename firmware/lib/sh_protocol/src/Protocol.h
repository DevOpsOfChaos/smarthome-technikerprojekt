#pragma once

#include <Arduino.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "DeviceTypes.h"

#define SH_PROTO_MAGIC         0xA5U
#define SH_PROTO_VERSION       1U

#define SH_ESPNOW_MAX_BYTES    250U
#define SH_HEADER_SIZE         10U
#define SH_MAX_PAYLOAD_BYTES   (SH_ESPNOW_MAX_BYTES - SH_HEADER_SIZE)

#define SH_DEVICE_ID_LEN       16U
#define SH_DEVICE_NAME_LEN     32U
#define SH_SENSOR_MASK_LEN     11U
#define SH_INPUT_MASK_LEN      6U

#define SH_MSG_HELLO           0x01U
#define SH_MSG_HELLO_ACK       0x02U
#define SH_MSG_STATE           0x03U
#define SH_MSG_EVENT           0x04U
#define SH_MSG_CMD             0x05U
#define SH_MSG_CFG             0x06U
#define SH_MSG_ACK             0x07U
#define SH_MSG_TIME            0x08U
#define SH_MSG_HEARTBEAT       0x09U

#define SH_FLAG_ACK_REQUEST    0x01U
#define SH_FLAG_RETRANSMIT     0x02U

#define SH_ACK_OK              0x00U
#define SH_ACK_ERROR           0x01U
#define SH_ACK_REJECTED        0x02U

#define SH_CMD_RELAY           0x01U
#define SH_CMD_COVER           0x02U
#define SH_CMD_STATE_REQUEST   0x03U
#define SH_CMD_REBOOT          0x04U
#define SH_CMD_SET_RELAY       SH_CMD_RELAY

#define SH_CFG_REPORT_INTERVAL_S      0x02U
#define SH_CFG_AUTO_OFF_DELAY_S       0x21U
#define SH_CFG_LIGHT_THRESHOLD_ON     0x22U
#define SH_CFG_AUTOMATION_ENABLED     0x25U

#define SH_EVENT_BUTTON_PRESS         0x01U
#define SH_EVENT_MOTION_DETECTED      0x02U
#define SH_EVENT_WINDOW_OPENED        0x03U
#define SH_EVENT_WINDOW_CLOSED        0x04U
#define SH_EVENT_RAIN_DETECTED        0x05U
#define SH_EVENT_RELAY_CHANGED        0x06U
#define SH_EVENT_LIGHT_AUTO_ON        0x07U
#define SH_EVENT_LIGHT_AUTO_OFF       0x08U
#define SH_EVENT_COVER_UP             0x09U
#define SH_EVENT_COVER_DOWN           0x0AU
#define SH_EVENT_COVER_STOP           0x0BU
#define SH_EVENT_NODE_BOOT            0x0EU
#define SH_EVENT_SENSOR_FAULT         0x0FU
#define SH_EVENT_COMM_FAULT           0x10U
#define SH_EVENT_BUTTON_RELEASE       0x11U
#define SH_EVENT_BUTTON_LONG_PRESS    0x12U

#define SH_TRIGGER_UNKNOWN            0x00U
#define SH_TRIGGER_MANUAL_BUTTON      0x01U
#define SH_TRIGGER_MASTER_CMD         0x02U
#define SH_TRIGGER_AUTO               0x03U
#define SH_TRIGGER_AUTO_OFF_TIMER     0x04U
#define SH_TRIGGER_CONFIG             0x05U

#define SH_COVER_STATE_STOPPED        0x00U
#define SH_COVER_STATE_MOVING_UP      0x01U
#define SH_COVER_STATE_MOVING_DOWN    0x02U

#define SH_ERROR_ACK_TIMEOUT          0x03U

namespace SmartHome {

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  proto_ver;
    uint8_t  msg_type;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  _reserved;
    uint16_t payload_len;
    uint16_t crc16;
} MsgHeader;

static_assert(sizeof(MsgHeader) == SH_HEADER_SIZE, "MsgHeader muss exakt 10 Byte groß sein");

typedef struct __attribute__((packed)) {
    char     device_id[SH_DEVICE_ID_LEN];
    char     device_name[SH_DEVICE_NAME_LEN];
    uint8_t  device_class;
    uint8_t  caps_hi;
    uint8_t  caps_lo;
    uint8_t  power_type;
    uint16_t fw_version;
    uint32_t boot_counter;
    uint8_t  meta_schema_version;
    uint8_t  control_mode;
    uint8_t  config_profile;
    uint8_t  reporting_mode;
    char     sensor_mask[SH_SENSOR_MASK_LEN];
    char     input_mask[SH_INPUT_MASK_LEN];
} HelloPayload;

typedef struct __attribute__((packed)) {
    uint8_t channel;
    uint8_t ack_status;
    uint8_t _pad[2];
} HelloAckPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint32_t uptime_s;
} HeartbeatPayload;

typedef struct __attribute__((packed)) {
    uint8_t cmd_type;
    uint8_t param1;
    uint8_t param2;
    uint8_t _pad;
} CmdPayload;

typedef struct __attribute__((packed)) {
    uint8_t  param_id;
    uint8_t  _pad;
    uint16_t value;
} CfgPayload;

typedef struct __attribute__((packed)) {
    uint8_t ack_seq;
    uint8_t ack_msg_type;
    uint8_t status;
    uint8_t _pad;
} AckPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  event_type;
    uint8_t  trigger;
    uint8_t  param1;
    uint16_t param2;
    uint8_t  _pad;
} EventReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint8_t  _pad[2];
} StateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} StateConfigReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
} RelayComfortStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} RelayComfortConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
} ExtendedRelayComfortStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} ExtendedRelayComfortConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
} ExtendedRelayComfortGasStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  auto_flags;
    uint8_t  fault;
    uint8_t  _pad;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} ExtendedRelayComfortGasConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  relay_2;
    uint8_t  cover_mode;
    uint8_t  cover_state;
    uint8_t  cover_position;
    uint8_t  fault;
} ZrlStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  relay_2;
    uint8_t  cover_mode;
    uint8_t  cover_state;
    uint8_t  cover_position;
    uint8_t  fault;
    uint16_t report_interval_s;
} ZrlConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
} SensorStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
    uint16_t report_interval_s;
} SensorConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  fault;
} ExtendedSensorStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  fault;
    uint16_t report_interval_s;
} ExtendedSensorConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  fault;
} ExtendedSensorGasStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
    uint8_t  motion;
    uint8_t  fault;
    uint16_t report_interval_s;
} ExtendedSensorGasConfigStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    uint8_t  window_open;
    uint16_t rain_raw;
    uint8_t  button_flags;
    uint8_t  fault;
} BatteryStateReportPayload;

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    uint8_t  window_open;
    uint16_t rain_raw;
    uint8_t  button_flags;
    uint8_t  fault;
    uint16_t report_interval_s;
} BatteryConfigStateReportPayload;

static inline void fillHeader(MsgHeader& h, uint8_t msg_type, uint8_t seq, uint8_t flags, uint16_t payload_len) {
    h.magic = SH_PROTO_MAGIC;
    h.proto_ver = SH_PROTO_VERSION;
    h.msg_type = msg_type;
    h.seq = seq;
    h.flags = flags;
    h._reserved = 0;
    h.payload_len = payload_len;
    h.crc16 = 0;
}

static inline bool isValidHeader(const MsgHeader& h) {
    return h.magic == SH_PROTO_MAGIC && h.proto_ver == SH_PROTO_VERSION && h.payload_len <= SH_MAX_PAYLOAD_BYTES;
}

static inline bool isValidMac(const uint8_t* mac) {
    if (!mac) return false;
    bool allZero = true;
    bool allFf = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) allZero = false;
        if (mac[i] != 0xFF) allFf = false;
    }
    return !(allZero || allFf);
}

static inline void macToString(const uint8_t* mac, char* buffer) {
    if (!mac || !buffer) return;
    snprintf(buffer, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline uint16_t calcCrc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static inline uint16_t calcPacketCrc(const MsgHeader& header, const uint8_t* payload) {
    MsgHeader temp = header;
    temp.crc16 = 0;
    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    memcpy(buffer, &temp, sizeof(MsgHeader));
    if (temp.payload_len > 0U && payload != nullptr) {
        memcpy(buffer + sizeof(MsgHeader), payload, temp.payload_len);
    }
    return calcCrc16(buffer, (uint16_t)(sizeof(MsgHeader) + temp.payload_len));
}

static inline void finalizePacketCrc(MsgHeader& header, const uint8_t* payload) {
    header.crc16 = calcPacketCrc(header, payload);
}

static inline bool hasValidPacketCrc(const uint8_t* packet, size_t len) {
    if (!packet || len < sizeof(MsgHeader)) return false;
    MsgHeader header;
    memcpy(&header, packet, sizeof(MsgHeader));
    if (!isValidHeader(header)) return false;
    if (len != (sizeof(MsgHeader) + header.payload_len)) return false;
    return calcPacketCrc(header, packet + sizeof(MsgHeader)) == header.crc16;
}

}  // namespace SmartHome
