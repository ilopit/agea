#pragma once

#include <spdlog/sinks/base_sink.h>

#include <memory>
#include <mutex>

namespace kryga::rpc
{

class rpc_server;

// spdlog sink that emits each formatted log line as a `log` JSON-RPC
// notification with shape: {level: "info"|"warn"|..., text: "..."}. Drops
// silently when no client is attached. Adds a sink, does not replace any —
// engine still logs to console + file as before.
class rpc_log_sink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit rpc_log_sink(rpc_server& server)
        : m_server(server)
    {
    }

protected:
    void
    sink_it_(const spdlog::details::log_msg& msg) override;

    void
    flush_() override
    {
    }

private:
    rpc_server& m_server;
};

}  // namespace kryga::rpc
