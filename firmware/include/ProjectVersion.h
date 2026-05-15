/**
 * @file ProjectVersion.h
 * @brief Zentrale Projektversion (SemVer) und Name
 *
 * @details Wird von allen Firmware-Targets eingebunden (via build_flags -include).
 *          Versionierung: MAJOR.MINOR.PATCH (Semantic Versioning).
 *          Bei Release das Datum auf den Tag der Freigabe setzen.
 *
 * @author DevOpsOfChaos
 * @date   2026-04-08
 */

#pragma once

#define PROJECT_NAME           "SmartHome ESP32"
#define PROJECT_VERSION        "0.4.0"
#define PROJECT_VERSION_DATE   "2026-04-08"
