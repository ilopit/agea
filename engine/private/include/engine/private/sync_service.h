#pragma once

#include <utils/path.h>

#include <thread>
#include <atomic>
#include <memory>
#include <cstdint>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/url.hpp>

#include <future>
#include <mutex>

namespace kryga
{
namespace http = boost::beast::http;  // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;     // from <boost/asio/ip/tcp.hpp>

struct sync_action
{
    utils::path path_to_resources;
    std::promise<std::string> to_signal;
};

class sync_service
{
public:
    static constexpr uint16_t default_port = 10033;

    void
    start(uint16_t port = default_port);

    void
    stop();

    bool
    has_sync_actions()
    {
        return m_has_sync_actions.load();
    }

    void
    extract_data(std::vector<sync_action>& vsa);

    void
    add_sync_action(sync_action&& sa);

private:
    void
    write_session_file();

    void
    remove_session_file();

    static int
    server_main(sync_service* self);

    static void
    do_session(sync_service* self, boost::asio::ip::tcp::socket socket);

    static void
    handle_request(sync_service* self,
                   boost::beast::http::request<boost::beast::http::string_body>&& req,
                   struct send_lambda& send);

    std::atomic_bool m_is_running = {false};
    std::thread m_main_thread;
    uint16_t m_port = default_port;

    std::mutex m_sync_mutex;

    std::atomic_bool m_has_sync_actions = false;
    std::vector<sync_action> m_actions;
};

}  // namespace kryga