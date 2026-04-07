#include "engine/action_queue.h"

#include <utils/kryga_log.h>

#include <chrono>

namespace kryga
{
namespace engine
{

action_queue::~action_queue()
{
    m_shutdown.store(true);
    m_cv.notify_all();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void
action_queue::submit(action a)
{
    ALOG_INFO("action_queue: submitted '{}'", a.name);

    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        m_pending.push_back(std::move(a));
    }

    start_worker();
    m_cv.notify_one();
}

void
action_queue::tick()
{
    std::lock_guard<std::mutex> lock(m_completed_mutex);
    for (auto& r : m_completed_staging)
    {
        m_finished.push_back(std::move(r));
    }
    m_completed_staging.clear();
}

std::string
action_queue::current_action_name()
{
    if (!m_running.load())
        return {};
    return m_progress.get_status().empty() ? "Working..." : m_progress.get_status();
}

size_t
action_queue::queued_count()
{
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    return m_pending.size();
}

void
action_queue::start_worker()
{
    if (m_worker.joinable())
        return;

    m_worker = std::thread([this]() { worker_loop(); });
}

void
action_queue::worker_loop()
{
    while (!m_shutdown.load())
    {
        action current;

        {
            std::unique_lock<std::mutex> lock(m_pending_mutex);
            m_cv.wait(lock, [&] { return !m_pending.empty() || m_shutdown.load(); });

            if (m_shutdown.load())
                return;

            if (m_pending.empty())
                continue;

            current = std::move(m_pending.front());
            m_pending.pop_front();
        }

        // Reset progress
        m_progress.progress.store(0.0f);
        m_progress.set_status(current.name);
        m_running.store(true);

        action_result result;
        result.name = current.name;

        auto start = std::chrono::high_resolution_clock::now();

        try
        {
            current.work(m_progress);
            result.success = true;
        }
        catch (const std::exception& e)
        {
            result.error = e.what();
            result.success = false;
            ALOG_ERROR("action_queue: '{}' failed: {}", current.name, e.what());
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration<float, std::milli>(end - start).count();

        m_running.store(false);
        m_progress.progress.store(1.0f);

        ALOG_INFO("action_queue: '{}' {} in {:.1f}ms",
                  result.name,
                  result.success ? "completed" : "failed",
                  result.duration_ms);

        {
            std::lock_guard<std::mutex> lock(m_completed_mutex);
            m_completed_staging.push_back(std::move(result));
        }
    }
}

}  // namespace engine
}  // namespace kryga
