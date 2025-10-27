#include "utils/agea_log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>

namespace agea
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
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("agea_log.txt", false);

        auto logger = std::make_shared<spdlog::logger>(
            "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
        logger->set_level(lvl);

        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y:%m:%d %T] [%z] [%P] %v");
        ALOG_ERROR("Let's the show begin!\n");
        spdlog::set_pattern("[%T.%e][%^%L%$][%s:%#] %v");

        return;
    }

    AGEA_never("Should never happens!");
}

}  // namespace utils
}  // namespace agea
