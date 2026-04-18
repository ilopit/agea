#include "utils/kryga_log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(__ANDROID__)
#include <spdlog/sinks/android_sink.h>
#else
#include <spdlog/sinks/basic_file_sink.h>
#endif
#include <memory>

namespace kryga
{
namespace utils
{
bool is_initialized = false;
void
setup_logger(spdlog::level::level_enum lvl)
{
    if (!is_initialized)
    {
        is_initialized = true;

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#if defined(__ANDROID__)
        // Android has no writable CWD; route file logs to logcat instead.
        auto file_sink = std::make_shared<spdlog::sinks::android_sink_mt>("kryga");
#else
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("kryga_log.txt", false);
#endif

        auto logger = std::make_shared<spdlog::logger>(
            "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
        logger->set_level(lvl);

        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y:%m:%d %T] [%z] [%P] %v");
        ALOG_ERROR("Let's the show begin!\n");
        spdlog::set_pattern("[%T.%e][%^%L%$][%s:%#] %v");

        return;
    }

    KRG_never("Should never happens!");
}

}  // namespace utils
}  // namespace kryga
