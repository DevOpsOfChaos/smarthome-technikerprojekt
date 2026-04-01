#pragma once

namespace SmartHome {
namespace ShStorage {

constexpr unsigned int SH_STORED_REPORT_INTERVAL_MIN_S = 5U;
constexpr unsigned int SH_STORED_REPORT_INTERVAL_MAX_S = 3600U;
constexpr unsigned int SH_STORED_AUTO_OFF_DELAY_MIN_S = 1U;
constexpr unsigned int SH_STORED_AUTO_OFF_DELAY_MAX_S = 3600U;
constexpr unsigned int SH_STORED_LIGHT_THRESHOLD_ON_MIN = 0U;
constexpr unsigned int SH_STORED_LIGHT_THRESHOLD_ON_MAX = 65535U;

}  // namespace ShStorage
}  // namespace SmartHome
