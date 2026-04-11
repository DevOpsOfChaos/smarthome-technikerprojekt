#pragma once

// C-kompatible Makros, damit -include auch in Framework-C-Dateien robust bleibt.
// I2C entspricht dem aktuellen HardwarePinStandard (SDA=0, SCL=1).
#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1

// Device-spezifischer Vorab-Pin fuer den digitalen Regenausgang.
// Nicht als globaler net_sen-Standard behandeln; beim Realtest ggf. hier aendern.
#define NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN 3
#define NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW 1
#define NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP 1

// Optionale Status-LED nicht bestueckt.
#define NET_SEN_PIN_STATUS_LED -1
