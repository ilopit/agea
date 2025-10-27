#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/spdlog.h>
#include "utils/check.h"

namespace agea
{
namespace utils
{
void
setup_logger(spdlog::level::level_enum lvl = spdlog::level::info);

extern bool is_initialized;
}  // namespace utils

}  // namespace agea

// clang-format off
#define ALOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__); AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
#define ALOG_INFO(...)  SPDLOG_INFO(__VA_ARGS__);AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
#define ALOG_WARN(...)  SPDLOG_WARN(__VA_ARGS__);AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
#define ALOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__);AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
#define ALOG_LAZY_ERROR SPDLOG_ERROR("Your message can be here!");AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
#define ALOG_FATAL(...) SPDLOG_CRITICAL(__VA_ARGS__);AGEA_check(::agea::utils::is_initialized, "Logs should not be used before initialization!")
// clang-format on