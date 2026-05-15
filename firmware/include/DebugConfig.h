/**
 * @file DebugConfig.h
 * @brief Globale Debug-Flags zur Laufzeit-Fehlersuche
 *
 * @details DEBUG_AKTIV ist der Master-Schalter. Wenn false, sind alle
 *          Subsystem-Debugs automatisch deaktiviert. Debug-Ausgaben
 *          erfolgen via Serial (115200 Baud).
 *
 * @warning Fuer Produktion ALLE Flags auf false setzen!
 *          Debug-Logs beeintraechtigen die Echtzeitfaehigkeit.
 *
 * @author DevOpsOfChaos
 */

#pragma once

// Master-Schalter: true = Debug-Ausgaben aktiv
constexpr bool DEBUG_AKTIV           = true;

// Subsystem-Debugs (nur wirksam wenn DEBUG_AKTIV == true)
constexpr bool DEBUG_SENSORIK        = true;   ///< Sensormesswerte und I2C-Diagnose
constexpr bool DEBUG_KOMMUNIKATION   = true;   ///< ESP-NOW- und MQTT-Pakete
constexpr bool DEBUG_AKTOREN         = true;   ///< Relais-Schaltvorgaenge
