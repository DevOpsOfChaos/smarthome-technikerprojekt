#pragma once

// C-kompatible Makros, damit -include auch in Framework-C-Dateien robust bleibt.
// Werte entsprechen dem aktuellen HardwarePinStandard (SDA=0, SCL=1).
#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1

// Optional: separate Status-LED nicht bestueckt.
#define NET_SEN_PIN_STATUS_LED -1
