#include "rpc/rpc_log_sink.h"

#include "rpc/rpc_server.h"

#include <json/json.h>

#include <spdlog/pattern_formatter.h>

namespace kryga::rpc
{

namespace
{
const char*
level_to_str(spdlog::level::level_enum lvl)
{
    switch (lvl)
    {
    case spdlog::level::trace:
        return "trace";
    case spdlog::level::debug:
        return "debug";
    case spdlog::level::info:
        return "info";
    case spdlog::level::warn:
        return "warn";
    case spdlog::level::err:
        return "error";
    case spdlog::level::critical:
        return "fatal";
    default:
        return "info";
    }
}
}  // namespace

void
rpc_log_sink::sink_it_(const spdlog::details::log_msg& msg)
{
    Json::Value params(Json::objectValue);
    params["level"] = level_to_str(msg.level);
    params["text"] = std::string(msg.payload.data(), msg.payload.size());
    m_server.notify("log", params);
}

}  // namespace kryga::rpc
