#include "rpc/rpc_server.h"

#include <utils/check.h>
#include <utils/kryga_log.h>

#include <json/json.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <process.h>  // _getpid
#define KRG_GETPID() static_cast<unsigned long>(::_getpid())
#else
#include <unistd.h>
#define KRG_GETPID() static_cast<unsigned long>(::getpid())
#endif

namespace kryga::rpc
{

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

namespace
{

// LSP-style message framing: "Content-Length: N\r\n\r\n<N bytes of UTF-8 JSON>"
std::string
frame_message(const std::string& payload)
{
    std::ostringstream os;
    os << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
    return os.str();
}

// Try to extract one framed message from `buffer`. On success consumes the
// framed bytes from the front and writes payload into `payload_out`.
bool
try_consume_frame(std::vector<char>& buffer, std::string& payload_out)
{
    constexpr std::string_view header_terminator = "\r\n\r\n";
    auto it = std::search(
        buffer.begin(), buffer.end(), header_terminator.begin(), header_terminator.end());
    if (it == buffer.end())
    {
        return false;
    }

    std::string headers(buffer.begin(), it);
    size_t content_length = 0;
    {
        std::istringstream is(headers);
        std::string line;
        while (std::getline(is, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            constexpr std::string_view k = "Content-Length:";
            if (line.rfind(k, 0) == 0)
            {
                content_length = std::stoul(line.substr(k.size()));
            }
        }
    }

    auto payload_begin = it + header_terminator.size();
    size_t available = static_cast<size_t>(buffer.end() - payload_begin);
    if (available < content_length)
    {
        return false;
    }

    payload_out.assign(payload_begin, payload_begin + content_length);
    buffer.erase(buffer.begin(), payload_begin + content_length);
    return true;
}

std::string
serialize(const Json::Value& v)
{
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, v);
}

Json::Value
make_response_ok(const Json::Value& id, const Json::Value& result)
{
    Json::Value r(Json::objectValue);
    r["jsonrpc"] = "2.0";
    r["id"] = id;
    r["result"] = result;
    return r;
}

Json::Value
make_response_err(const Json::Value& id, int code, const std::string& msg)
{
    Json::Value r(Json::objectValue);
    r["jsonrpc"] = "2.0";
    r["id"] = id;
    Json::Value err(Json::objectValue);
    err["code"] = code;
    err["message"] = msg;
    r["error"] = err;
    return r;
}

Json::Value
make_notification(const std::string& method, const Json::Value& params)
{
    Json::Value n(Json::objectValue);
    n["jsonrpc"] = "2.0";
    n["method"] = method;
    n["params"] = params;
    return n;
}

}  // namespace

struct rpc_server::impl
{
    std::unordered_map<std::string, request_handler> handlers;

    asio::io_context io_ctx;
    std::unique_ptr<tcp::acceptor> acceptor;

    // Active client. Guarded by client_mutex so the writer and accept threads
    // don't race on socket destruction. asio::ip::tcp::socket is not
    // thread-safe but writes are serialized through a single writer thread.
    std::mutex client_mutex;
    std::shared_ptr<tcp::socket> client;

    std::atomic<bool> running{false};

    std::mutex out_mutex;
    std::condition_variable out_cv;
    std::deque<std::string> out_queue;  // pre-framed messages

    std::thread io_thread;
    std::thread writer_thread;

    uint16_t bound_port = 0;
    std::string discovery_path;

    void
    handle_request(const Json::Value& msg)
    {
        const auto& id = msg["id"];
        const std::string method = msg.get("method", "").asString();
        const Json::Value& params = msg["params"];

        auto it = handlers.find(method);
        if (it == handlers.end())
        {
            // -32601 = method not found per JSON-RPC 2.0
            push_outbound(make_response_err(id, -32601, "method not found: " + method));
            return;
        }

        Json::Value result(Json::nullValue);
        std::string err;
        try
        {
            it->second(params, result, err);
        }
        catch (const std::exception& e)
        {
            err = std::string("handler exception: ") + e.what();
        }

        if (!err.empty())
        {
            push_outbound(make_response_err(id, -32000, err));
        }
        else
        {
            push_outbound(make_response_ok(id, result));
        }
    }

    void
    push_outbound(const Json::Value& msg)
    {
        std::string payload = serialize(msg);
        std::string framed = frame_message(payload);
        {
            std::lock_guard lk(out_mutex);
            out_queue.push_back(std::move(framed));
        }
        out_cv.notify_one();
    }

    // Take the current client (if any), under the lock, swap to nullptr.
    // Returned shared_ptr keeps the socket alive while the caller closes it.
    std::shared_ptr<tcp::socket>
    detach_client()
    {
        std::lock_guard lk(client_mutex);
        std::shared_ptr<tcp::socket> taken;
        client.swap(taken);
        return taken;
    }

    void
    drop_client()
    {
        auto taken = detach_client();
        if (taken)
        {
            boost::system::error_code ignore;
            taken->shutdown(tcp::socket::shutdown_both, ignore);
            taken->close(ignore);
        }
        out_cv.notify_all();
    }

    void
    accept_and_serve()
    {
        while (running.load())
        {
            auto sock = std::make_shared<tcp::socket>(io_ctx);

            boost::system::error_code ec;
            acceptor->accept(*sock, ec);
            if (ec)
            {
                if (!running.load())
                {
                    break;
                }
                continue;
            }

            // Kick previous client if any. Assigning the new socket under the
            // lock; the previous socket is closed after we release the lock so
            // we don't block writers any longer than necessary.
            std::shared_ptr<tcp::socket> prev;
            {
                std::lock_guard lk(client_mutex);
                prev = std::move(client);
                client = sock;
            }
            if (prev)
            {
                boost::system::error_code ignore;
                prev->shutdown(tcp::socket::shutdown_both, ignore);
                prev->close(ignore);
            }
            ALOG_INFO("rpc: client connected");

            std::vector<char> rx;
            rx.reserve(4096);
            while (running.load())
            {
                char tmp[4096];
                std::size_t n = sock->read_some(asio::buffer(tmp, sizeof(tmp)), ec);
                if (ec || n == 0)
                {
                    break;
                }
                rx.insert(rx.end(), tmp, tmp + n);

                std::string payload;
                while (try_consume_frame(rx, payload))
                {
                    Json::Value msg;
                    Json::CharReaderBuilder b;
                    std::string parse_err;
                    std::istringstream is(payload);
                    if (!Json::parseFromStream(b, is, &msg, &parse_err))
                    {
                        ALOG_WARN("rpc: bad JSON: {}", parse_err);
                        continue;
                    }
                    if (msg.isMember("method") && msg.isMember("id"))
                    {
                        handle_request(msg);
                    }
                    else
                    {
                        // Notifications from client are not handled today.
                        ALOG_WARN("rpc: ignoring non-request message");
                    }
                }
            }

            ALOG_INFO("rpc: client disconnected");
            // Only close if it's still our client (drop_client / new accept
            // may have swapped it already).
            std::shared_ptr<tcp::socket> mine_if_still_current;
            {
                std::lock_guard lk(client_mutex);
                if (client == sock)
                {
                    mine_if_still_current = std::move(client);
                }
            }
            if (mine_if_still_current)
            {
                boost::system::error_code ignore;
                mine_if_still_current->shutdown(tcp::socket::shutdown_both, ignore);
                mine_if_still_current->close(ignore);
            }
            out_cv.notify_all();
        }
    }

    void
    writer_loop()
    {
        while (running.load())
        {
            std::string framed;
            {
                std::unique_lock lk(out_mutex);
                out_cv.wait(lk, [&] { return !running.load() || !out_queue.empty(); });
                if (!running.load())
                {
                    return;
                }
                framed = std::move(out_queue.front());
                out_queue.pop_front();
            }

            std::shared_ptr<tcp::socket> sock;
            {
                std::lock_guard lk(client_mutex);
                sock = client;
            }
            if (!sock)
            {
                // No client — drop. (Notifications fired with no consumer.)
                continue;
            }
            boost::system::error_code ec;
            asio::write(*sock, asio::buffer(framed), ec);
            if (ec)
            {
                // Write failure → drop. The read loop will notice the socket
                // is gone on next read_some and exit.
                std::shared_ptr<tcp::socket> taken;
                {
                    std::lock_guard lk(client_mutex);
                    if (client == sock)
                    {
                        taken = std::move(client);
                    }
                }
                if (taken)
                {
                    boost::system::error_code ignore;
                    taken->shutdown(tcp::socket::shutdown_both, ignore);
                    taken->close(ignore);
                }
            }
        }
    }
};

rpc_server::rpc_server()
    : m_impl(std::make_unique<impl>())
{
}

rpc_server::~rpc_server()
{
    stop();
}

bool
rpc_server::start(uint16_t port, const std::string& discovery_abs_path)
{
    if (m_impl->running.load())
    {
        return false;
    }

    boost::system::error_code ec;
    auto endpoint = tcp::endpoint(asio::ip::address_v4::loopback(), port);
    m_impl->acceptor = std::make_unique<tcp::acceptor>(m_impl->io_ctx);
    m_impl->acceptor->open(endpoint.protocol(), ec);
    if (ec)
    {
        ALOG_ERROR("rpc: open() failed: {}", ec.message());
        return false;
    }
    m_impl->acceptor->set_option(asio::socket_base::reuse_address(true), ec);
    m_impl->acceptor->bind(endpoint, ec);
    if (ec)
    {
        ALOG_ERROR("rpc: bind() to 127.0.0.1:{} failed: {}", port, ec.message());
        return false;
    }
    m_impl->acceptor->listen(1, ec);
    if (ec)
    {
        ALOG_ERROR("rpc: listen() failed: {}", ec.message());
        return false;
    }

    m_impl->bound_port = m_impl->acceptor->local_endpoint().port();
    m_impl->discovery_path = discovery_abs_path;
    m_impl->running.store(true);

    // Discovery file: write {pid, port} as JSON.
    {
        Json::Value d(Json::objectValue);
        d["pid"] = static_cast<Json::UInt64>(KRG_GETPID());
        d["port"] = static_cast<Json::UInt>(m_impl->bound_port);
        d["version"] = 1;
        std::ofstream f(discovery_abs_path, std::ios::binary | std::ios::trunc);
        if (f)
        {
            f << serialize(d);
        }
        else
        {
            ALOG_WARN("rpc: could not write discovery file '{}'", discovery_abs_path);
        }
    }

    m_impl->writer_thread = std::thread(&impl::writer_loop, m_impl.get());
    m_impl->io_thread = std::thread(&impl::accept_and_serve, m_impl.get());

    ALOG_INFO("rpc: listening on 127.0.0.1:{} (pid {})", m_impl->bound_port, KRG_GETPID());
    return true;
}

uint16_t
rpc_server::port() const
{
    return m_impl->bound_port;
}

void
rpc_server::stop()
{
    if (!m_impl->running.exchange(false))
    {
        return;
    }

    if (m_impl->acceptor)
    {
        boost::system::error_code ignore;
        m_impl->acceptor->close(ignore);  // unblocks accept()
    }
    m_impl->drop_client();
    m_impl->out_cv.notify_all();

    if (m_impl->io_thread.joinable())
    {
        m_impl->io_thread.join();
    }
    if (m_impl->writer_thread.joinable())
    {
        m_impl->writer_thread.join();
    }

    m_impl->acceptor.reset();

    if (!m_impl->discovery_path.empty())
    {
        std::remove(m_impl->discovery_path.c_str());
    }
}

void
rpc_server::on_request(const std::string& method, request_handler h)
{
    KRG_check(!m_impl->running.load(), "on_request() called after start()");
    m_impl->handlers[method] = std::move(h);
}

void
rpc_server::notify(const std::string& method, const Json::Value& params)
{
    if (!m_impl->running.load())
    {
        return;
    }
    m_impl->push_outbound(make_notification(method, params));
}

}  // namespace kryga::rpc
