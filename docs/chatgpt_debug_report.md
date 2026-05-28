# NET-ERL-020 Debug Report — LED Ring Animation Runs Only Once

## 1. Current State

| Field | Value |
|---|---|
| Device ID | NET-ERL-020 |
| Chip | ESP32-C3 |
| Firmware | ESPHome 2026.5.1 (Arduino framework) |
| I2C Bus | 10 kHz, shared 3-sensor bus (BME680, VEML7700, ENS160) |
| LED Driver | `fastled_clockless` (WS2812B, GRB, 17 LEDs, GPIO4) |
| Presence | GPIO7, active-high, `on_press` → `handle_motion_on` |
| Boot | WiFi OK, MQTT OK, sensor init OK, relay switches OK |
| **BUG** | LED ring comfort sequence runs exactly **one cycle** (45 s) after boot or first motion, then **never again** |

---

## 2. Complete Bug & Fix History (Chronological)

### Bug 1 — Boot Crash: `AddressableLightEffect` NULL Pointer
- **Date**: 2026-05-27
- **Symptom**: Load access fault (`MEPC 0x420101be`, `MTVAL 0x70`) immediately after I2C init. ESPHome 2026.5.1 RMT driver on ESP32-C3 calls `LightState::get_output()` before the output is allocated, yielding a NULL `Parent LightState*` inside `AddressableLightEffect::start_internal()`.
- **Fix**: Replaced `esp32_rmt_led_strip` with `fastled_clockless` (software bit-banging). Changed `chipset: WS2812 → WS2812B`, added `max_refresh_rate: 100ms`.
- **Status**: RESOLVED

### Bug 2 — LED Ring Phase Modulo Missing
- **Date**: 2026-05-27
- **Symptom**: LED animation hiccuped at phase boundaries; the phase/time calculation within `led_ring_interval` did not wrap correctly for elapsed time beyond the total sequence duration.
- **Fix**: Added modulo wrapping: `phase_ms = (elapsed_s * 1000UL) % (total_s * 1000UL)` so the three phases (AQI → Temperature → Humidity) loop indefinitely while `display_until_ms` has not expired.
- **Status**: RESOLVED

### Bug 3 — ENS160 Compensation Missing
- **Date**: 2026-05-27
- **Symptom**: ENS160 AQI/eCO2/TVOC values were uncalibrated because the `compensation` block (temperature + humidity from BME680) referenced stale or uninitialized BME680 sensor IDs.
- **Fix**: Added `compensation: { temperature: env_bme680_temp, humidity: env_bme680_hum }` to the ENS160 sensor config.
- **Status**: RESOLVED

### Bug 4 — Auto-On Without Lux (5-Second Timeout)
- **Date**: 2026-05-27
- **Symptom**: When VEML7700 fails or is slow, `pending_auto_on_decision` was stuck forever because `evaluate_auto_on_from_lux` returned immediately when `env_lux == 65535` (sentinel). The light would never auto-turn-on.
- **Fix**: Added `pending_auto_on_since_ms` timer. When `env_lux == 65535` and 5 s have elapsed since motion was first detected, the light turns on anyway (VEML7700 tolerance fallback).
- **Status**: RESOLVED

### Bug 5 — 20-Minute Fault (ENS160 Warmup Burst)
- **Date**: 2026-05-27
- **Symptom**: During the 20-minute ENS160 warmup window, `env_has_aqi`/`env_has_eco2`/`env_has_tvoc` were set to `true` with garbage raw values (the sensor reports data before burn-in completes). This polluted MQTT state and showed wrong AQI colors on the LED ring.
- **Fix**: Added `ens160_init_ms` warmup guard: all three ENS160 sensors gate on `millis() - ens160_init_ms >= 1200000UL` (20 min). Before that, they output sentinel values (`65535` = "no valid value").
- **Status**: RESOLVED

### Bug 6 — Post-OTA I2C Lockup
- **Date**: 2026-05-27
- **Symptom**: After OTA firmware updates, the shared I2C bus locked up. Stale transactions from the old firmware left SDA/SCl in a hung state that the ESP32-C3 Arduino I2C driver could not auto-recover from.
- **Fix**: Added staggered sensor polling intervals (53s VEML7700, 59s ENS160, 61s BME680 — three coprime numbers). LCM is ~53 hours, so collisions happen only after ~2 days. Each sensor gets exclusive bus access + 50ms margin. Added stale-timout monitors (30s interval checking `last_bme_update_ms`, `last_veml_update_ms`, `last_ens_update_ms`).
- **Status**: RESOLVED

### Bug 7 — Deep Sleep Recovery Loop
- **Date**: 2026-05-28
- **Symptom**: ESP32-C3 booted, crashed on I2C, rebooted, and looped without ever reaching a stable state.
- **Fix**: Combined fix — I2C frequency reduced to 10 kHz for the 2kΩ multisensor bus, plus the `AddressableLightEffect` crash fix (Bug 1).
- **Status**: RESOLVED

### Bug 8 — 50 kHz I2C Broke Bus
- **Date**: 2026-05-28
- **Symptom**: At 50 kHz and 100 kHz, the BME680 (0x77) + VEML7700 (0x10) + ENS160 (0x52) triple-sensor bus exhibited intermittent NACKs and data corruption on the 2 kΩ pullup line.
- **Fix**: Reduced I2C frequency to 10 kHz. At this speed, all three sensors operate reliably on the shared, weak-pullup bus. I2C scan disabled (`scan: false`) to reduce boot-time bus stress.
- **Status**: RESOLVED

### Bug 9 — `update_interval: never` Broke Sensors
- **Date**: 2026-05-28
- **Symptom**: Setting sensor `update_interval: never` and driving polls exclusively via staggered intervals (`component.update`) caused ESPHome to never publish initial `on_value` events. The `env_has_*` flags stayed `false` forever.
- **Fix**: Rolled back to `update_interval: 60s` on all three sensors. The staggered interval blocks still fire `component.update` for extra reads, but auto-poll provides the baseline. This creates duplicate reads but guarantees initial values are emitted.
- **Status**: RESOLVED (rollback)

### Bug 10 — `contract_online` Guard Broke `hardware_fault`
- **Date**: 2026-05-28
- **Symptom**: `hardware_fault` was only computed inside `publish_net_erl_led_state`, which gated on `contract_online`. If MQTT was disconnected or during startup, `hardware_fault` was never recomputed, causing the LED ring to show stale fault state.
- **Fix**: Added `hardware_fault` recomputation inside the `led_ring_interval` (every 120 ms):
  ```cpp
  id(hardware_fault) = !id(env_has_temp) || !id(env_has_hum) || !id(env_has_lux);
  ```
- **Status**: RESOLVED

### Bug 11 — Garbage Sensor Values Over MQTT
- **Date**: 2026-05-28
- **Symptom**: Sentinel values (`-32768`, `65535`, `4294967295UL`) were published as raw integers in MQTT JSON state. Downstream consumers misinterpreted these as real sensor readings (e.g., "temperature: -3276.8°C").
- **Fix**: Changed the `publish_net_erl_led_state` JSON builder to emit `nullptr` (JSON null) for sentinel values instead of the raw integer. Applied to temperature, humidity, pressure, lux, gas, AQI, eCO2, and TVOC fields.
- **Status**: RESOLVED

### Bug 12 — Auto-On Periodic Evaluation Missing
- **Date**: 2026-05-28
- **Symptom**: When `pending_auto_on_decision` was `true` but the initial `evaluate_auto_on_from_lux` returned early (waiting for VEML7700 within the 5s window), no subsequent evaluation ran. The 5s timeout was never checked again.
- **Fix**: In the 50 ms presence poll branch where `explicit_off_hold` is active and motion_state is already true, added a periodic re-evaluation:
  ```cpp
  if (id(pending_auto_on_decision)) {
    id(evaluate_auto_on_from_lux).execute();
  }
  ```
- **Status**: RESOLVED

### Bug 13 — CURRENT: LED Animation Runs Only Once
- **Date**: 2026-05-28 (UNRESOLVED)
- **Symptom**: The LED ring comfort sequence (AQI → Temp → Humidity phases, 45 s total) runs exactly once — either at boot or on the first motion event. When the sequence expires (`display_until_ms` passes), the LEDs clear correctly. Subsequent motion events are detected (relay switches, MQTT publishes motion=true), but the LED ring **never re-animates**.
- **Status**: **OPEN**

---

## 3. Key Files

| # | File Path | Role |
|---|---|---|
| 1 | `esphome/devices/net_erl_hall_module_led_ring.yaml` | Main device configuration (1506 lines) — all logic, globals, scripts, intervals, sensors |
| 2 | `esphome/packages/smarthome_contract_base.yaml` | MQTT contract: meta, availability, setup portal, `contract_online`/`contract_fault` globals |
| 3 | `esphome/packages/smarthome_command_ack.yaml` | ACK mechanism: `flush_command_ack` script, `ack_request_id`/`ack_status` globals |
| 4 | `esphome/packages/smarthome_device_event.yaml` | Event publishing: `flush_device_event` script, event type codes (motion=2, relay=6, auto-on=7, auto-off=8, etc.) |
| 5 | `esphome/packages/smarthome_cover_contract.yaml` | Cover contract (not used by NET-ERL-020, included for completeness) |
| 6 | `esphome/packages/setup_portal.h` | C++ header: captive portal HTML generator, MAC validation, `SmartHomeSetupPortal` namespace |
| 7 | `esphome/packages/smarthome_command_dispatch.h` | C++ header: `smarthome_extract_request_id()`, `smarthome_validate_master_mac()` |
| 8 | `firmware/src/devices/net_erl_hall_module_led_ring/main.cpp` | Reference firmware (C++), not in this repo — contains original `netErlDeviceReadPresence` and `updateRingComfortDisplay` |
| 9 | `firmware/src/devices/net_erl_hall_module_led_ring/DeviceConfig.h` | Reference firmware config header — contains `NET_ERL_LED_RING_AQI_PHASE_MS` etc. |

**Note**: The root-level `esphome/net_erl_hall_module_led_ring.yaml` and `esphome/devices/net_erl_hall_module_led_ring.yaml` are identical copies. All work should target the `devices/` copy as it has the most recent edits.

---

## 4. Current Issue — Detailed Analysis

### 4.1 Normal Flow (First Cycle — Works)

```
GPIO7 HIGH → on_press of presence_input
  → handle_motion_on
    → motion_state = true
    → start_led_ring_comfort_sequence
      → if display_until_ms expired: sequence_started_ms = now
      → display_until_ms = now + 45000  (15+15+15 seconds)
      → refresh_led_ring_comfort_display
        → led_ring_light.turn_on() at brightness=1.0

led_ring_interval (every 120ms)
  → reads display_until_ms, computes phase_ms = elapsed % 45000
  → renders AQI / Temperature / Humidity phase via direct LED writes
  → calls output->schedule_show()

After 45s:
  → (int32_t)(display_until_ms - now) <= 0
  → clearAll() → output->schedule_show() → LEDs off

refresh_led_ring_comfort_display (triggered on publish)
  → display_active = false
  → (!contract_online || hardware_fault || alert_active || display_active) = false
  → led_ring_light.turn_off()  ✓
```

### 4.2 Second Motion Event — Where It Breaks

```
GPIO7 stays HIGH (or goes LOW→HIGH):
  → on_press of presence_input fires
    → logger: "PRESENCE: motion detected (GPIO7 HIGH)"
    → handle_motion_on
      → start_led_ring_comfort_sequence
        → now = millis()
        → if ((int32_t)(display_until_ms - now) <= 0)
            // display_until_ms is from the FIRST cycle, long expired
            // This evaluates to TRUE → sequence_started_ms = now
        → display_until_ms = now + 45000
        → refresh_led_ring_comfort_display
          → display_active = (int32_t)(display_until_ms - now) > 0 → TRUE
          → led_ring_light.turn_on() at brightness=1.0
```

**This looks correct.** The sequence should restart. So why does it fail?

### 4.3 Root Cause Hypothesis — The Poll Interference

The 50 ms presence poll (`interval: ${presence_poll_interval}`) runs this logic **concurrently** with the `on_press` handler:

```cpp
if (presence_high) {
    if (!id(explicit_off_hold)) {
        if (!id(motion_state)) {
            id(handle_motion_on).execute();   // motion_state==false → calls handle_motion_on
        } else {
            id(last_motion_ms) = now;          // motion_state==true → ONLY updates timestamp
            // *** DOES NOT CALL start_led_ring_comfort_sequence ***
        }
    }
}
```

**The problem chain on second motion detection:**

1. GPIO7 transitions LOW→HIGH.
2. `on_press` fires `handle_motion_on`. This sets `motion_state = true` and calls `start_led_ring_comfort_sequence`. The sequence **is** started.
3. **Simultaneously (within the same 50ms window)**, the presence poll runs:
   - `presence_high = true`
   - `explicit_off_hold = false`
   - `motion_state = true` (set by `handle_motion_on` in step 2)
   - Takes the `else` branch: `id(last_motion_ms) = now;` — does **NOT** call `start_led_ring_comfort_sequence`.
4. BUT — the poll saw `motion_state == true` from Step 2. It does not interfere.

**Wait.** Re-examine `start_led_ring_comfort_sequence`:

```cpp
const uint32_t now = millis();
if ((int32_t)(id(led_ring_display_until_ms) - now) <= 0) {
    id(led_ring_sequence_started_ms) = now;
}
id(led_ring_display_until_ms) = now + total_ms;
```

`display_until_ms` should be expired (from the first cycle). The condition `(int32_t)(old_display_until_ms - now) <= 0` evaluates to **true** when the old timer has passed. `sequence_started_ms` is reset to `now`. `display_until_ms` is set to `now + 45000`. This seems correct.

**However**, consider the `refresh_led_ring_comfort_display` that follows:

```cpp
const bool alert_active = id(led_ring_alert_code) != 0 &&
    (int32_t)(id(led_ring_alert_until_ms) - now) > 0;
const bool display_active = (int32_t)(id(led_ring_display_until_ms) - now) > 0;

if (!id(led_ring_enabled)) { turn_off; return; }

if (!id(contract_online) || id(hardware_fault) || alert_active || display_active) {
    turn_on(brightness=1.0);
} else {
    turn_off;  // ← THIS
}
```

If `contract_online == true`, `hardware_fault == false`, `alert_active == false`, and `display_active == true` (just set!), this calls `turn_on(brightness=1.0)`. The `led_ring_interval` then renders the animation. This should work.

### 4.4 The REAL Problem — `handle_motion_on` Timing vs ESPHome Script Execution

The `on_press` of a GPIO `binary_sensor` fires **asynchronously** in ESPHome. The `handle_motion_on` script starts executing, but ESPHome scripts are **not atomic** — they yield between steps. Between `start_led_ring_comfort_sequence` and the subsequent `publish_net_erl_led_state`, the 50ms poll can interleave.

Worse: `handle_motion_on` calls `start_led_ring_comfort_sequence` via `id(start_led_ring_comfort_sequence).execute()`. This script has its own steps:
1. Lambda: set `display_until_ms`
2. `refresh_led_ring_comfort_display`

If the 50ms poll fires between step 1 (setting `display_until_ms`) and the `led_ring_interval` rendering the first frame, the poll does NOT interfere with `display_until_ms`.

**BUT** — what if the `on_press` handler does NOT actually fire on the second GPIO transition?

### 4.5 Most Likely Cause — GPIO `on_press` Not Firing on Re-Triggers

The PIR sensor on GPIO7 typically outputs a continuous HIGH while motion is detected. The transition behavior is:

- **First motion**: GPIO LOW→HIGH → `on_press` fires ✓
- **Motion sustained**: GPIO stays HIGH for N seconds (no edges, no events)
- **Motion ends**: GPIO HIGH→LOW → `on_release` fires
- **Second motion (later)**: GPIO LOW→HIGH → `on_press` fires ✓

If the PIR sensor retriggers while still HIGH (i.e., a second motion event before the first `on_release`), there is **no LOW→HIGH transition** and `on_press` does **NOT** fire. GPIO remains HIGH from the first motion through the second. No edge = no event.

In this scenario:
- The presence poll (50ms) sees `presence_high == true` continuously.
- `motion_state` was set to `true` on first motion and never reset (since PIR stayed HIGH throughout).
- The poll takes the `else` branch: `id(last_motion_ms) = now;` — it never re-triggers the sequence.
- `on_press` never fires because GPIO never went LOW.

**This is the likely root cause.** The LED ring depends on `on_press` → `handle_motion_on` → `start_led_ring_comfort_sequence` to restart. If the PIR stays HIGH across two motion detection windows, `on_press` only fires once.

---

## 5. Questions for ChatGPT

### Q1: Does `led_ring_interval` need to restart `display_until_ms` when motion is re-detected?

Currently, `led_ring_interval` (120ms) is purely a rendering loop. It reads `display_until_ms` and renders phases — it does NOT modify `display_until_ms`. The restart is done by `start_led_ring_comfort_sequence`, which is only called from `handle_motion_on`.

If the issue is that `handle_motion_on` is never re-called (because `on_press` doesn't fire), then adding restart logic to the interval would be a workaround, not a fix. However, adding a "motion re-detected → extend display_until_ms" check in the 50ms poll would be the correct architectural fix.

### Q2: Is `on_press` of `presence_input` actually firing on subsequent motion events?

This is the critical diagnostic question. If the PIR sensor outputs a continuous HIGH while motion is sustained, subsequent motion pulses within the same HIGH period produce **no edges** → `on_press` does not fire. The `on_release` will fire when the PIR finally goes LOW, but by then the sequence has expired and there's no re-trigger until the next distinct LOW→HIGH transition.

**Test to verify**: Add a counter global that increments on every `on_press`. Check via MQTT/logs whether the counter increments on the second detection.

### Q3: Should `start_led_ring_comfort_sequence` be called whenever motion is detected (not just on state transition)?

**Yes.** The reference firmware (`main.cpp`) calls `netErlDeviceReadPresence()` in its main loop and restarts the ring sequence on every detection, not just on state transition. The ESPHome port should do the same.

The fix is in the **50ms presence poll**:

```cpp
if (presence_high) {
    if (!id(explicit_off_hold)) {
        if (!id(motion_state)) {
            id(handle_motion_on).execute();
        } else {
            id(last_motion_ms) = now;
            // === MISSING: restart LED sequence on sustained motion ===
            // Call start_led_ring_comfort_sequence here (or every 30s via the
            // existing last_seq_reset_ms logic that is currently a no-op)
        }
    }
}
```

The existing code already has a `last_seq_reset_ms` variable (line 1273-1276) but it does nothing — the comment says "start_led_ring_comfort_sequence now only via handle_motion_on". This is exactly the code path that needs to be re-activated.

**Proposed fix** (two independent layers):

**Layer A — Restart sequence on every motion re-detection (in the 50ms poll):**
```cpp
// After: id(last_motion_ms) = now;
// Add:
id(start_led_ring_comfort_sequence).execute();
```

**Layer B — Keep-alive: extend `display_until_ms` while motion is active (in the 50ms poll):**
```cpp
// Alternative to Layer A, or combined with it:
// Instead of restarting the whole sequence, just push out the expiration
if (id(led_ring_display_until_ms) > 0 && (int32_t)(id(led_ring_display_until_ms) - now) < 5000) {
    // Less than 5s remaining → push it out by another 45s
    id(led_ring_display_until_ms) = now + 45000;
}
```

**Recommended approach**: Layer A (restart on every motion detection) matches the firmware reference behavior and is the simplest fix. The sequence restarts from AQI phase, which is the intended UX — showing air quality on every new presence event.

---

## 6. Proposed Fix (Code Change)

In `esphome/devices/net_erl_hall_module_led_ring.yaml`, in the 50ms presence poll `interval:` block, locate this section (~line 1272-1297):

```yaml
          if (presence_high) {
            static uint32_t last_seq_reset_ms = 0;
            if (now - last_seq_reset_ms >= 30000UL) {
              # start_led_ring_comfort_sequence now only via handle_motion_on
              last_seq_reset_ms = now;
            }
            if (!id(explicit_off_hold)) {
              if (!id(motion_state)) {
                id(handle_motion_on).execute();
              } else {
                id(last_motion_ms) = now;
                if (id(manual_follow_motion_active)) {
                  id(manual_follow_motion_seen) = true;
                  id(manual_follow_motion_probe_active) = false;
                  id(manual_follow_probe_started_ms) = 0;
                }
              }
            } else {
              id(motion_state) = true;
              id(last_motion_ms) = now;
              if (id(pending_auto_on_decision)) {
                id(evaluate_auto_on_from_lux).execute();
              }
            }
            return;
          }
```

Replace the no-op `last_seq_reset_ms` block with an actual sequence restart:

```yaml
          if (presence_high) {
            static uint32_t last_seq_reset_ms = 0;
            if (now - last_seq_reset_ms >= 30000UL) {
              # Restart LED ring sequence every 30s while motion is sustained
              # This handles the case where on_press fires only once because
              # the PIR sensor stays HIGH across multiple motion windows.
              id(start_led_ring_comfort_sequence).execute();
              last_seq_reset_ms = now;
            }
            if (!id(explicit_off_hold)) {
              if (!id(motion_state)) {
                id(handle_motion_on).execute();
              } else {
                id(last_motion_ms) = now;
                if (id(manual_follow_motion_active)) {
                  id(manual_follow_motion_seen) = true;
                  id(manual_follow_motion_probe_active) = false;
                  id(manual_follow_probe_started_ms) = 0;
                }
              }
            } else {
              id(motion_state) = true;
              id(last_motion_ms) = now;
              if (id(pending_auto_on_decision)) {
                id(evaluate_auto_on_from_lux).execute();
              }
            }
            return;
          }
```

This restarts the LED ring sequence every 30 seconds while the PIR signal is HIGH. Since the total sequence duration is 45 seconds, the sequence will **never expire** while motion is continuously detected — it restarts every 30 seconds from the AQI phase.

If you prefer the sequence to run exactly once per motion event (and only re-trigger on a *new* LOW→HIGH transition), then the real fix is to ensure the PIR actually produces a falling edge between motion events, which may not be possible with standard PIR sensor modules. The 30-second restart is the pragmatic fix that matches the firmware reference behavior.

---

## 7. Additional Diagnostic Commands

To verify `on_press` behavior on-device:

```yaml
# Add to binary_sensor presence_input on_press:
- lambda: |-
    static int press_count = 0;
    press_count++;
    ESP_LOGI("presence", "on_press #%d, GPIO=%d, motion_state=%d, display_until_ms=%d",
             press_count, (int)id(presence_input).state, (int)id(motion_state),
             (int)id(led_ring_display_until_ms));
    id(handle_motion_on).execute();
```

If `press_count` increments on the second motion event, then `on_press` IS firing and the bug is elsewhere. If it stays at 1, then the PIR GPIO edge issue is confirmed.

---

*Report generated 2026-05-28 for ChatGPT debugging session.*
