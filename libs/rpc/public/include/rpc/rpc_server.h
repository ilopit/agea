#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace Json
{
class Value;
}

namespace kryga::rpc
{

// Result of a request handler. If `error_message` is non-empty the response
// becomes a JSON-RPC error with code -32000 (server-defined) and the message;
// otherwise `result` is sent as the response payload.
struct handler_result
{
    Json::Value* result = nullptr;
    std::string error_message;
};

using request_handler =
    std::function<void(const Json::Value& params, Json::Value& result_out, std::string& error_out)>;

// JSON-RPC 2.0 server over a localhost TCP socket using LSP-style message
// framing (Content-Length: N\r\n\r\n{json}). Supports multiple concurrent
// clients — a warning is logged when more than one is connected since
// concurrent writes may conflict.
//
// Threading: handlers and notifications can be called from any thread.
// Each client gets its own reader/writer threads. notify() broadcasts
// to all connected clients. No assumption is made about the engine main
// thread.
class rpc_server
{
public:
    rpc_server();
    ~rpc_server();

    rpc_server(const rpc_server&) = delete;
    rpc_server&
    operator=(const rpc_server&) = delete;

    // Bind to 127.0.0.1:port (port=0 = OS picks a free port). Writes a JSON
    // discovery file to `discovery_abs_path` containing {pid, port}.
    // Returns false on bind/listen failure.
    bool
    start(uint16_t port, const std::string& discovery_abs_path);

    // Get the bound port (valid after a successful start()).
    uint16_t
    port() const;

    // Stop accepting connections, close any open client, join threads.
    // Removes the discovery file. Idempotent.
    void
    stop();

    // Register a request handler. Must be called before start() — registry
    // is read by the I/O thread and not protected after that point.
    void
    on_request(const std::string& method, request_handler h);

    // Push a notification to all connected clients. Drops silently if no
    // clients are attached. Thread-safe.
    void
    notify(const std::string& method, const Json::Value& params);

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

}  // namespace kryga::rpc
