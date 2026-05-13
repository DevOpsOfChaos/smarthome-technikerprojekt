#pragma once

// Pinlinie aus dem erprobten Kitchen-Aufbau.
#define PIN_SENSOR_SDA 1
#define PIN_SENSOR_SCL 0
#define PIN_BUTTON_1 6
#define PIN_LD2410_OUT 7
#define PIN_LED_RING 8
#define PIN_RELAY_1 10
#define PIN_LD2410_UART_RX 20
#define PIN_LD2410_UART_TX 21

#define LED_RING_COUNT 17

#define BUTTON_1_ACTIVE_LOW 1
#define RELAY_1_ACTIVE_HIGH 1
#define PIN_STATUS_LED -1

// Setup nutzt denselben Button nur bei langem Halten.
#define SETUP_BUTTON_PIN PIN_BUTTON_1
#define SETUP_BUTTON_ACTIVE_LOW BUTTON_1_ACTIVE_LOW
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN -1
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
