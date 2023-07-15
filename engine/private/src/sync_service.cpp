#include "engine/private/sync_service.h"

#include "engine/agea_engine.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <stack>
#include <future>
#include <unordered_set>

#include <utils/agea_log.h>

namespace agea
{

namespace
{
const std::unordered_set<std::string> s_supported_ext = {"vert", "frag", "lua"};
}

struct send_lambda
{
    explicit send_lambda(tcp::socket& stream, bool& close, boost::beast::error_code& ec)
        : m_stream(stream)
        , m_close(close)
        , m_ec(ec)
    {
    }

    template <bool isRequest, class Body, class Fields>
    void
    operator()(boost::beast::http::message<isRequest, Body, Fields>&& msg) const
    {
        // Determine if we should close the connection after
        m_close = msg.need_eof();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(m_stream, sr, m_ec);
    }

    tcp::socket& m_stream;
    bool& m_close;
    boost::beast::error_code& m_ec;
};

static void
fail(boost::beast::error_code ec, char const* what)
{
    ALOG_ERROR("Sync error {0} {1}", what, ec.message());
}

void
sync_service::do_session(sync_service* self, tcp::socket socket)
{
    bool close = false;
    boost::beast::error_code ec;
    boost::beast::flat_buffer buffer;

    send_lambda lambda{socket, close, ec};

    for (;;)
    {
        http::request<http::string_body> req;

        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream)
        {
            break;
        }

        if (ec)
        {
            return fail(ec, "read");
        }

        // Send the response
        handle_request(self, std::move(req), lambda);

        if (ec)
        {
            return fail(ec, "write");
        }

        if (close)
        {
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

void
sync_service::handle_request(sync_service* self,
                             http::request<http::string_body>&& req,
                             send_lambda& send)
{
    // Returns a bad request response
    auto const bad_request = [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::get && req.method() != http::verb::head)
    {
        return send(bad_request("Unknown HTTP-method"));
    }

    // Request path must be absolute and not contain "..".
    if (req.target().empty() || req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
    {
        return send(bad_request("Illegal request-target"));
    }

    boost::url u = boost::urls::parse_origin_form(req.target()).value();

    auto request_params = u.params();

    for (auto c : request_params)
    {
        if (c.key == "ping")
        {
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.body() = std::string("We are still alive!");
            res.keep_alive(req.keep_alive());
            res.prepare_payload();

            return send(std::move(res));
        }
        else if (c.key == "file")
        {
            auto pos = c.value.rfind('.');

            if (pos == std::string::npos)
            {
                return send(bad_request("Unsupported file"));
            }

            auto extention = c.value.substr(pos + 1);

            if (s_supported_ext.find(extention) == s_supported_ext.end())
            {
                return send(bad_request("Unsupported file"));
            }

            std::promise<std::string> prom;

            auto ftr = prom.get_future();

            sync_action sa{APATH(c.value), std::move(prom)};

            self->add_sync_action(std::move(sa));

            ftr.wait();

            std::string output = "";
            if (ftr.valid())
            {
                output = ftr.get();
            }

            http::response<http::string_body> res{http::status::ok, req.version()};
            res.body() = output;
            res.keep_alive(req.keep_alive());
            res.prepare_payload();

            return send(std::move(res));
        }
    }

    return send(bad_request("Unknown"));
}

void
sync_service::start()
{
    bool e = false;
    auto result = m_is_running.compare_exchange_strong(e, true);

    if (result)
    {
        m_main_thread = std::thread(sync_service::server_main, this);
    }
    else
    {
        ALOG_ERROR("Server already running!");
    }
}

void
sync_service::stop()
{
    bool e = true;
    auto result = m_is_running.compare_exchange_strong(e, false);
    if (result)
    {
        m_main_thread.join();
    }
    else
    {
        ALOG_ERROR("Server already stopped!");
    }
}

void
sync_service::extract_data(std::vector<sync_action>& vsa)
{
    std::lock_guard<std::mutex> g(m_sync_mutex);
    vsa = std::move(m_actions);
    m_has_sync_actions = false;
}

void
sync_service::add_sync_action(sync_action&& sa)
{
    std::lock_guard<std::mutex> g(m_sync_mutex);

    m_has_sync_actions = true;
    m_actions.emplace_back(std::move(sa));
}

int
sync_service::server_main(sync_service* self)
{
    auto const address = boost::asio::ip::make_address("0.0.0.0");
    auto const port = unsigned short(10033);

    // The io_context is required for all I/O
    boost::asio::io_context ioc{1};

    // The acceptor receives incoming connections
    tcp::acceptor acceptor{ioc, {address, port}};
    while (self->m_is_running)
    {
        // This will receive the new connection
        tcp::socket socket{ioc};

        // Block until we get a connection
        acceptor.accept(socket);

        // Launch the session, transferring ownership of the socket
        std::thread{sync_service::do_session, self, std::move(socket)}.detach();
    }

    return 0;
}

}  // namespace agea