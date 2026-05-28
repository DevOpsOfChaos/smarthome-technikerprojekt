/*
 * Minimaler I2C-Hardware-Test für NET-ERL Hall Module LED Ring
 * Pins: SDA=GPIO0, SCL=GPIO1, 100kHz
 * Kein WiFi, kein ENS160, keine Sensorbibliotheken.
 * Nur: Wire.init, Scan, manuelles Sensor-Read.
 */

#include <Arduino.h>
#include <Wire.h>

constexpr int I2C_SDA = 0;
constexpr int I2C_SCL = 1;
constexpr uint32_t I2C_CLOCK = 100000;

void scanBus() {
    Serial.println("--- I2C Scan ---");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("  FOUND 0x%02X\n", addr);
            found++;
        } else if (err == 2) {
            // NACK on address - normal, no device
        } else {
            Serial.printf("  ERR 0x%02X -> %u\n", addr, err);
        }
        yield(); // feed WDT
    }
    Serial.printf("Scan done: %d device(s)\n", found);
}

void tryReadRegister(uint8_t addr, uint8_t reg, const char* name) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false); // repeated start
    if (err != 0) {
        Serial.printf("  %s (0x%02X) reg 0x%02X: endTransmission err=%u\n", name, addr, reg, err);
        return;
    }
    uint8_t count = Wire.requestFrom(addr, (uint8_t)2);
    if (count == 0) {
        Serial.printf("  %s (0x%02X) reg 0x%02X: no data\n", name, addr, reg);
        return;
    }
    Serial.printf("  %s (0x%02X) reg 0x%02X:", name, addr, reg);
    while (Wire.available()) {
        Serial.printf(" %02X", Wire.read());
    }
    Serial.println();
}

void testDevice(uint8_t addr, const char* name, uint8_t idReg) {
    Serial.printf("--- Test %s (0x%02X) ---\n", name, addr);
    tryReadRegister(addr, idReg, name);
}

void setup() {
    Serial.begin(115200);
    // Warte max 5s auf USB-CDC-Verbindung (nicht blockierend wenn kein Host)
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 5000) { delay(10); }
    delay(200);
    Serial.println("\n\n=== I2C Hardware Test ===");
    Serial.printf("SDA=GPIO%d SCL=GPIO%d CLK=%lu Hz\n", I2C_SDA, I2C_SCL, I2C_CLOCK);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(I2C_CLOCK);
    Wire.setTimeOut(50);
    Serial.println("Wire.begin() done.");

    scanBus();

    // BME680: ID register 0xD0 should return 0x61
    testDevice(0x77, "BME680", 0xD0);
    testDevice(0x76, "BME680_ALT", 0xD0);

    // VEML7700: ID register 0x00 should return 0x01 (half-word) + 0xC0
    testDevice(0x10, "VEML7700", 0x00);

    // ENS160: PART_ID register 0x00 should return 0x0160
    testDevice(0x52, "ENS160", 0x00);
    testDevice(0x53, "ENS160_ALT", 0x00);

    Serial.println("=== Test Complete ===");
    Serial.println("Rebooting in 10s...");
    delay(10000);
    ESP.restart();
}

void loop() {
    delay(1000);
}
