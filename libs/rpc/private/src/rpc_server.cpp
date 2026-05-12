#include "rpc/rpc_server.h"

#include <utils/check.h>
#include <utils/kryga_log.h>

#include <json/json.h>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <optional>
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

std::string
frame_message(const std::string& payload)
{
    std::ostringstream os;
    os << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
    return os.str();
}

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

// ---------------------------------------------------------------------------
// impl — declared with nested session forward-ref, defined below
// ---------------------------------------------------------------------------

struct rpc_server::impl
{
    struct session;

    std::unordered_map<std::string, request_handler> handlers;

    asio::io_context io_ctx;
    using work_guard_t = asio::executor_work_guard<asio::io_context::executor_type>;
    std::optional<work_guard_t> work_guard;

    std::unique_ptr<tcp::acceptor> acceptor;

    std::vector<std::shared_ptr<session>> clients;

    std::atomic<bool> running{false};
    uint32_t next_client_id = 1;

    std::thread io_thread;

    uint16_t bound_port = 0;
    std::string discovery_path;

    void
    do_accept();
    void
    handle_request(const Json::Value& msg, session& sender);
    void
    remove_client(session* s);
    void
    broadcast(const Json::Value& msg);
};

// ---------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------

struct rpc_server::impl::session : public std::enable_shared_from_this<session>
{
    session(tcp::socket socket, uint32_t id, impl& server)
        : m_socket(std::move(socket))
        , m_id(id)
        , m_server(server)
    {
        m_read_buf.reserve(4096);
    }

    void
    start()
    {
        do_read();
    }

    void
    send(const Json::Value& msg)
    {
        std::string framed = frame_message(serialize(msg));
        bool was_idle = m_write_queue.empty();
        m_write_queue.push_back(std::move(framed));
        if (was_idle)
        {
            do_write();
        }
    }

    void
    close()
    {
        boost::system::error_code ignore;
        m_socket.shutdown(tcp::socket::shutdown_both, ignore);
        m_socket.close(ignore);
    }

    uint32_t
    session_id() const
    {
        return m_id;
    }

private:
    void
    do_read()
    {
        auto self = shared_from_this();
        m_socket.async_read_some(
            asio::buffer(m_tmp),
            [this, self](boost::system::error_code ec, std::size_t bytes)
            {
                if (ec)
                {
                    on_disconnect();
                    return;
                }

                m_read_buf.insert(m_read_buf.end(), m_tmp.data(), m_tmp.data() + bytes);

                std::string payload;
                while (try_consume_frame(m_read_buf, payload))
                {
                    Json::Value msg;
                    Json::CharReaderBuilder b;
                    std::string parse_err;
                    std::istringstream is(payload);
                    if (!Json::parseFromStream(b, is, &msg, &parse_err))
                    {
                        ALOG_WARN("rpc: client #{} bad JSON: {}", m_id, parse_err);
                        continue;
                    }
                    if (msg.isMember("method") && msg.isMember("id"))
                    {
                        m_server.handle_request(msg, *this);
                    }
                }

                do_read();
            });
    }

    void
    do_write()
    {
        auto self = shared_from_this();
        asio::async_write(m_socket,
                          asio::buffer(m_write_queue.front()),
                          [this, self](boost::system::error_code ec, std::size_t)
                          {
                              if (ec)
                              {
                                  on_disconnect();
                                  return;
                              }
                              m_write_queue.pop_front();
                              if (!m_write_queue.empty())
                              {
                                  do_write();
                              }
                          });
    }

    void
    on_disconnect()
    {
        ALOG_INFO("rpc: client #{} disconnected", m_id);
        close();
        m_server.remove_client(this);
    }

    tcp::socket m_socket;
    uint32_t m_id;
    impl& m_server;

    std::vector<char> m_read_buf;
    std::array<char, 4096> m_tmp{};

    std::deque<std::string> m_write_queue;
};

// ---------------------------------------------------------------------------
// impl method definitions
// ---------------------------------------------------------------------------

void
rpc_server::impl::do_accept()
{
    acceptor->async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (ec)
            {
                return;
            }

            auto s = std::make_shared<session>(std::move(socket), next_client_id++, *this);
            clients.push_back(s);

            if (clients.size() > 1)
            {
                ALOG_WARN(
                    "rpc: multiple clients connected ({} total) "
                    "— concurrent writes may conflict",
                    clients.size());
            }

            ALOG_INFO("rpc: client #{} connected", s->session_id());
            s->start();
            do_accept();
        });
}

void
rpc_server::impl::handle_request(const Json::Value& msg, session& sender)
{
    const auto& id = msg["id"];
    const std::string method = msg.get("method", "").asString();
    const Json::Value& params = msg["params"];

    auto it = handlers.find(method);
    if (it == handlers.end())
    {
        sender.send(make_response_err(id, -32601, "method not found: " + method));
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
        sender.send(make_response_err(id, -32000, err));
    }
    else
    {
        sender.send(make_response_ok(id, result));
    }
}

void
rpc_server::impl::remove_client(session* s)
{
    auto it =
        std::find_if(clients.begin(), clients.end(), [s](const auto& c) { return c.get() == s; });
    if (it != clients.end())
    {
        ALOG_INFO("rpc: client #{} removed ({} remaining)", s->session_id(), clients.size() - 1);
        clients.erase(it);
    }
}

void
rpc_server::impl::broadcast(const Json::Value& msg)
{
    for (auto& c : clients)
    {
        c->send(msg);
    }
}

// ---------------------------------------------------------------------------
// rpc_server
// ---------------------------------------------------------------------------

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
    m_impl->acceptor->listen(4, ec);
    if (ec)
    {
        ALOG_ERROR("rpc: listen() failed: {}", ec.message());
        return false;
    }

    m_impl->bound_port = m_impl->acceptor->local_endpoint().port();
    m_impl->discovery_path = discovery_abs_path;
    m_impl->running.store(true);

    // Discovery file
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

    m_impl->io_ctx.restart();
    m_impl->work_guard.emplace(m_impl->io_ctx.get_executor());
    m_impl->do_accept();
    m_impl->io_thread = std::thread([impl = m_impl.get()]() { impl->io_ctx.run(); });

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

    asio::post(m_impl->io_ctx,
               [impl = m_impl.get()]()
               {
                   boost::system::error_code ignore;
                   impl->acceptor->close(ignore);
                   for (auto& c : impl->clients)
                   {
                       c->close();
                   }
                   impl->clients.clear();
               });

    m_impl->work_guard.reset();

    if (m_impl->io_thread.joinable())
    {
        m_impl->io_thread.join();
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
    asio::post(m_impl->io_ctx,
               [impl = m_impl.get(), msg = make_notification(method, params)]()
               { impl->broadcast(msg); });
}

}  // namespace kryga::rpc
