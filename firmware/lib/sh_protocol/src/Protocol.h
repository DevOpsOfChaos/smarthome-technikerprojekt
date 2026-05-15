#pragma once

/*
====================================================================
 Projekt   : SmartHome ESP32
 Datei     : Protocol.h
 Modul     : ShProtocol
 Version   : 0.3.0
 Stand     : 2026-04-08

 Funktion:
 Vollständige Protokolldefinition für die ESP-NOW-Kommunikation
 zwischen Nodes und Master.

 Dieses Protokoll ist eine Neuentwicklung.
 Es basiert NICHT auf dem alten Protokoll aus dem Altprojekt.

 Entscheidungen:
 - Kleiner gemeinsamer Header mit Magic, Protokollversion,
   Nachrichtentyp, Sequenznummer, Flags, Payload-Länge und CRC16.
 - Feste Payloads für stabile Verwaltungsnachrichten
   (HELLO, HELLO_ACK, CMD, CFG, ACK, TIME).
 - HELLO trägt ab diesem Stand den oberen Gerätevertrag:
   Basisgerät + Profil + Reporting-Muster + Sensor-/Input-Masken.
 - Keine JSON-Payloads über ESP-NOW.
====================================================================
*/

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
#define SH_MAX_DEVICES         32U

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
#define SH_FLAG_ENCRYPTED      0x04U

#define SH_ACK_OK              0x00U
#define SH_ACK_ERROR           0x01U
#define SH_ACK_REJECTED        0x02U
#define SH_ACK_REJECTED_FULL   0x03U  // Registry voll, keine Slots frei

#define SH_CMD_RELAY           0x01U
#define SH_CMD_COVER           0x02U
#define SH_CMD_STATE_REQUEST   0x03U
#define SH_CMD_REBOOT          0x04U

#define SH_CMD_SET_RELAY       SH_CMD_RELAY

#define SH_COVER_CMD_OPEN          0x01U
#define SH_COVER_CMD_CLOSE         0x02U
#define SH_COVER_CMD_STOP          0x03U
#define SH_COVER_CMD_SET_POSITION  0x04U

#define SH_CFG_DEVICE_NAME            0x01U
#define SH_CFG_REPORT_INTERVAL_S      0x02U
#define SH_CFG_ACK_TIMEOUT_MS         0x03U
#define SH_CFG_MAX_RETRIES            0x04U
#define SH_CFG_EVENT_DEBOUNCE_MS      0x05U
#define SH_CFG_LED_ENABLED            0x06U

#define SH_CFG_TEMP_DELTA_01C         0x10U
#define SH_CFG_HUM_DELTA_01PCT        0x11U
#define SH_CFG_LUX_DELTA              0x12U
#define SH_CFG_PRESENCE_HOLD_S        0x13U

#define SH_CFG_RELAY_MODE             0x20U
#define SH_CFG_AUTO_OFF_DELAY_S       0x21U
#define SH_CFG_LIGHT_THRESHOLD_ON     0x22U
#define SH_CFG_LIGHT_THRESHOLD_OFF    0x23U
#define SH_CFG_RELAY_DEFAULT_ON_BOOT  0x24U
#define SH_CFG_AUTOMATION_ENABLED     0x25U

#define SH_CFG_COVER_RUN_UP_MS        0x30U
#define SH_CFG_COVER_RUN_DOWN_MS      0x31U
#define SH_CFG_COVER_REVERSE_LOCK_MS  0x32U
#define SH_CFG_COVER_CALIBRATED       0x33U

#define SH_CFG_WAKE_INTERVAL_S        0x40U
#define SH_CFG_RX_WINDOW_MS           0x41U
#define SH_CFG_LOW_BATTERY_PCT        0x42U

#define SH_CFG_RING_ENABLED           0x50U
#define SH_CFG_RING_BRIGHTNESS        0x51U
#define SH_CFG_RING_MODE              0x52U

#define SH_TLV_STATE_TIMESTAMP        0x01U
#define SH_TLV_STATE_LAST_TRIGGER     0x02U
#define SH_TLV_STATE_ERROR            0x03U

#define SH_TLV_STATE_TEMP_01C         0x20U
#define SH_TLV_STATE_HUM_01PCT        0x21U
#define SH_TLV_STATE_LUX              0x22U
#define SH_TLV_STATE_AQI              0x23U
#define SH_TLV_STATE_TVOC_PPB         0x24U
#define SH_TLV_STATE_ECO2_PPM         0x25U
#define SH_TLV_STATE_MOTION           0x26U
#define SH_TLV_STATE_WINDOW           0x27U
#define SH_TLV_STATE_RAIN_RAW         0x28U
#define SH_TLV_STATE_BATTERY_PCT      0x29U
#define SH_TLV_STATE_BATTERY_MV       0x2AU
#define SH_TLV_STATE_GAS_OHM          0x2BU

#define SH_TLV_STATE_RELAY0           0x40U
#define SH_TLV_STATE_RELAY1           0x41U
#define SH_TLV_STATE_COVER_POSITION   0x42U
#define SH_TLV_STATE_COVER_STATE      0x43U
#define SH_TLV_STATE_COVER_CALIBRATED 0x44U

#define SH_TLV_STATE_RSSI_DBM         0x60U
#define SH_TLV_STATE_BOOT_COUNTER     0x61U

#define SH_TLV_EVENT_TYPE             0x01U
#define SH_TLV_EVENT_TIMESTAMP        0x02U
#define SH_TLV_EVENT_RELAY_INDEX      0x03U
#define SH_TLV_EVENT_RELAY_TRIGGER    0x04U
#define SH_TLV_EVENT_RELAY_STATE      0x05U
#define SH_TLV_EVENT_BUTTON_INDEX     0x06U
#define SH_TLV_EVENT_BUTTON_TYPE      0x07U
#define SH_TLV_EVENT_COVER_TARGET     0x08U
#define SH_TLV_EVENT_COVER_STATE      0x09U
#define SH_TLV_EVENT_SENSOR_ERROR     0x0AU

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
#define SH_EVENT_COVER_CALIB_START    0x0CU
#define SH_EVENT_COVER_CALIB_DONE     0x0DU
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

#define SH_ERROR_NONE                 0x00U
#define SH_ERROR_SENSOR_INIT          0x01U
#define SH_ERROR_SENSOR_READ          0x02U
#define SH_ERROR_ACK_TIMEOUT          0x03U
#define SH_ERROR_COVER_CALIB          0x04U

#define SH_RELAY_COMFORT_FLAG_AUTO_REQUEST_ON           0x01U
#define SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED          0x02U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_SERVER         0x04U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX            0x08U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX    0x10U
#define SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE 0x20U
#define SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE     0x40U
#define SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED       0x80U

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

static_assert(sizeof(MsgHeader) == SH_HEADER_SIZE,
    "MsgHeader muss exakt SH_HEADER_SIZE Bytes groß sein");

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

static_assert(sizeof(HelloPayload) == 79,
    "HelloPayload muss 79 Bytes groß sein");

typedef struct __attribute__((packed)) {
    uint8_t channel;
    uint8_t ack_status;
    uint8_t _pad[2];
} HelloAckPayload;

static_assert(sizeof(HelloAckPayload) == 4,
    "HelloAckPayload muss 4 Bytes groß sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint32_t uptime_s;
} HeartbeatPayload;

static_assert(sizeof(HeartbeatPayload) == 20,
    "HeartbeatPayload muss 20 Bytes groß sein");

typedef struct __attribute__((packed)) {
    uint8_t cmd_type;
    uint8_t param1;
    uint8_t param2;
    uint8_t _pad;
} CmdPayload;

static_assert(sizeof(CmdPayload) == 4,
    "CmdPayload muss 4 Bytes groß sein");

typedef struct __attribute__((packed)) {
    uint8_t  param_id;
    uint8_t  _pad;
    uint16_t value;
} CfgPayload;

static_assert(sizeof(CfgPayload) == 4,
    "CfgPayload muss 4 Bytes groß sein");

typedef struct __attribute__((packed)) {
    uint8_t ack_seq;
    uint8_t ack_msg_type;
    uint8_t status;
    uint8_t _pad;
} AckPayload;

static_assert(sizeof(AckPayload) == 4,
    "AckPayload muss 4 Bytes groß sein");

typedef struct __attribute__((packed)) {
    uint32_t unix_time;
    int16_t  tz_offset_min;
    uint8_t  is_dst;
    uint8_t  _pad;
} TimePayload;

static_assert(sizeof(TimePayload) == 8,
    "TimePayload muss 8 Bytes groß sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint8_t  _pad[2];
} StateReportPayload;

static_assert(sizeof(StateReportPayload) == 20,
    "StateReportPayload muss 20 Bytes groß sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  fault;
    uint16_t report_interval_s;
    uint16_t auto_on_lux_threshold;
} StateConfigReportPayload;

static_assert(sizeof(StateConfigReportPayload) == 22,
    "StateConfigReportPayload muss 22 Bytes gross sein");

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

static_assert(sizeof(RelayComfortStateReportPayload) == 27,
    "RelayComfortStateReportPayload muss 27 Bytes gross sein");

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

static_assert(sizeof(RelayComfortConfigStateReportPayload) == 31,
    "RelayComfortConfigStateReportPayload muss 31 Bytes gross sein");

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

static_assert(sizeof(ExtendedRelayComfortStateReportPayload) == 37,
    "ExtendedRelayComfortStateReportPayload muss 37 Bytes gross sein");

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

static_assert(sizeof(ExtendedRelayComfortConfigStateReportPayload) == 41,
    "ExtendedRelayComfortConfigStateReportPayload muss 41 Bytes gross sein");

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

static_assert(sizeof(ExtendedRelayComfortGasStateReportPayload) == 41,
    "ExtendedRelayComfortGasStateReportPayload muss 41 Bytes gross sein");

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

static_assert(sizeof(ExtendedRelayComfortGasConfigStateReportPayload) == 45,
    "ExtendedRelayComfortGasConfigStateReportPayload muss 45 Bytes gross sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  relay_2;
    uint8_t  cover_mode;
    uint8_t  cover_state;
    uint8_t  cover_position;
    uint8_t  cover_calibrated;
    uint8_t  fault;
} ZrlStateReportPayload;

static_assert(sizeof(ZrlStateReportPayload) == 23,
    "ZrlStateReportPayload muss 23 Bytes gross sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  relay_1;
    uint8_t  relay_2;
    uint8_t  cover_mode;
    uint8_t  cover_state;
    uint8_t  cover_position;
    uint8_t  cover_calibrated;
    uint8_t  fault;
    uint16_t report_interval_s;
} ZrlConfigStateReportPayload;

static_assert(sizeof(ZrlConfigStateReportPayload) == 25,
    "ZrlConfigStateReportPayload muss 25 Bytes gross sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
} SensorStateReportPayload;

static_assert(sizeof(SensorStateReportPayload) == 24,
    "SensorStateReportPayload muss 24 Bytes groß sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    int16_t  temp_01c;
    uint16_t hum_01pct;
    uint16_t lux;
    uint8_t  motion;
    uint8_t  fault;
    uint16_t report_interval_s;
} SensorConfigStateReportPayload;

static_assert(sizeof(SensorConfigStateReportPayload) == 26,
    "SensorConfigStateReportPayload muss 26 Bytes gross sein");

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

static_assert(sizeof(ExtendedSensorStateReportPayload) == 34,
    "ExtendedSensorStateReportPayload muss 34 Bytes gross sein");

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

static_assert(sizeof(ExtendedSensorConfigStateReportPayload) == 36,
    "ExtendedSensorConfigStateReportPayload muss 36 Bytes gross sein");

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

static_assert(sizeof(ExtendedSensorGasStateReportPayload) == 38,
    "ExtendedSensorGasStateReportPayload muss 38 Bytes gross sein");

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

static_assert(sizeof(ExtendedSensorGasConfigStateReportPayload) == 40,
    "ExtendedSensorGasConfigStateReportPayload muss 40 Bytes gross sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    uint8_t  window_open;
    uint16_t rain_raw;
    uint8_t  button_flags;
    uint8_t  fault;
} BatteryStateReportPayload;

static_assert(sizeof(BatteryStateReportPayload) == 24,
    "BatteryStateReportPayload muss 24 Bytes groß sein");

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

static_assert(sizeof(BatteryConfigStateReportPayload) == 26,
    "BatteryConfigStateReportPayload muss 26 Bytes gross sein");

typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN];
    uint8_t  event_type;
    uint8_t  trigger;
    uint8_t  param1;
    uint16_t param2;
    uint8_t  _pad;
} EventReportPayload;

static_assert(sizeof(EventReportPayload) == 22,
    "EventReportPayload muss 22 Bytes groß sein");

typedef struct {
    uint8_t  type;
    uint8_t  length;
    uint8_t  value[4];
} TlvEntry;

static inline void fillHeader(
    MsgHeader& h,
    uint8_t msg_type,
    uint8_t seq,
    uint8_t flags,
    uint16_t payload_len)
{
    h.magic       = SH_PROTO_MAGIC;
    h.proto_ver   = SH_PROTO_VERSION;
    h.msg_type    = msg_type;
    h.seq         = seq;
    h.flags       = flags;
    h._reserved   = 0;
    h.payload_len = payload_len;
    h.crc16       = 0;
}

static inline bool isValidHeader(const MsgHeader& h) {
    if (h.magic != SH_PROTO_MAGIC) return false;
    if (h.proto_ver != SH_PROTO_VERSION) return false;
    if (h.payload_len > SH_MAX_PAYLOAD_BYTES) return false;
    return true;
}

static inline void safeCopyDeviceId(const char* src, char* dst) {
    if (!src || !dst) return;
    strncpy(dst, src, SH_DEVICE_ID_LEN - 1);
    dst[SH_DEVICE_ID_LEN - 1] = '\0';
    for (int i = 0; dst[i] != '\0'; i++) {
        const char c = dst[i];
        if (!isupper((unsigned char)c) &&
            !isdigit((unsigned char)c) &&
            c != '-') {
            dst[i] = '\0';
            break;
        }
    }
}

static inline void safeCopyMask(const char* src, char* dst, size_t dst_len, char fill_char) {
    if (!dst || dst_len == 0U) return;
    const size_t max_chars = dst_len - 1U;
    size_t i = 0U;
    for (; i < max_chars; ++i) {
        char c = fill_char;
        if (src != nullptr && src[i] != '\0') {
            c = src[i];
        }
        if (!isupper((unsigned char)c) && !isdigit((unsigned char)c) && c != 'X' && c != '_') {
            c = fill_char;
        }
        dst[i] = c;
    }
    dst[max_chars] = '\0';
}

static inline bool isValidDeviceId(const char* id) {
    if (!id) return false;
    const size_t len = strlen(id);
    if (len != 10 && len != 11) return false;
    for (int i = (int)len - 3; i < (int)len; i++) {
        if (!isdigit((unsigned char)id[i])) return false;
    }
    if (id[len - 4] != '-') return false;
    for (size_t i = 0; i < len; i++) {
        const char c = id[i];
        if (c == '-') continue;
        if (!isdigit((unsigned char)c) && !isupper((unsigned char)c)) return false;
    }
    return true;
}

static inline bool isValidMac(const uint8_t* mac) {
    if (!mac) return false;
    bool allZero = true;
    bool allFf   = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) allZero = false;
        if (mac[i] != 0xFF) allFf = false;
    }
    return !(allZero || allFf);
}

static inline void macToString(const uint8_t* mac, char* buffer) {
    if (!mac || !buffer) return;
    snprintf(buffer, 18,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline uint16_t calcCrc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static inline uint16_t calcPacketCrc(
    const MsgHeader& header,
    const uint8_t* payload)
{
    MsgHeader temp = header;
    temp.crc16 = 0;

    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    memcpy(buffer, &temp, sizeof(MsgHeader));

    if (temp.payload_len > 0U && payload != nullptr) {
        memcpy(buffer + sizeof(MsgHeader), payload, temp.payload_len);
    }

    return calcCrc16(buffer, (uint16_t)(sizeof(MsgHeader) + temp.payload_len));
}

static inline void finalizePacketCrc(
    MsgHeader& header,
    const uint8_t* payload)
{
    header.crc16 = calcPacketCrc(header, payload);
}

static inline bool hasValidPacketCrc(const uint8_t* packet, size_t len) {
    if (!packet || len < sizeof(MsgHeader)) return false;

    MsgHeader header;
    memcpy(&header, packet, sizeof(MsgHeader));

    if (!isValidHeader(header)) return false;
    if (len != (sizeof(MsgHeader) + header.payload_len)) return false;

    const uint8_t* payload = packet + sizeof(MsgHeader);
    return calcPacketCrc(header, payload) == header.crc16;
}

} // namespace SmartHome
