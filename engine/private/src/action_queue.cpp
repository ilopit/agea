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
action_queue::set_event_callback(action_event_callback cb)
{
    m_event_callback = std::move(cb);
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
    {
        std::lock_guard<std::mutex> lock(m_completed_mutex);
        for (auto& r : m_completed_staging)
        {
            m_finished.push_back(std::move(r));
        }
        m_completed_staging.clear();
    }

    if (m_event_callback && m_running.load())
    {
        float p = m_progress.progress.load();
        if (std::abs(p - m_last_reported_progress) > 0.01f)
        {
            m_last_reported_progress = p;
            action_event evt;
            evt.type = action_event_type::progress;
            evt.progress = p;
            {
                std::lock_guard<std::mutex> lock(m_progress.status_mutex);
                evt.name = m_current_name;
                evt.status = m_progress.status;
            }
            m_event_callback(evt);
        }
    }
}

std::string
action_queue::current_name()
{
    std::lock_guard<std::mutex> lock(m_progress.status_mutex);
    return m_current_name;
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
    {
        return;
    }

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
            {
                return;
            }

            if (m_pending.empty())
            {
                continue;
            }

            current = std::move(m_pending.front());
            m_pending.pop_front();
        }

        // Reset progress
        m_progress.progress.store(0.0f);
        {
            std::lock_guard<std::mutex> lock(m_progress.status_mutex);
            m_current_name = current.name;
            m_progress.status = current.name;
        }
        m_last_reported_progress = 0.0f;
        m_running.store(true);

        if (m_event_callback)
        {
            action_event evt;
            evt.type = action_event_type::started;
            evt.name = current.name;
            m_event_callback(evt);
        }

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
        {
            std::lock_guard<std::mutex> lock(m_progress.status_mutex);
            m_current_name.clear();
        }

        ALOG_INFO("action_queue: '{}' {} in {:.1f}ms",
                  result.name,
                  result.success ? "completed" : "failed",
                  result.duration_ms);

        if (m_event_callback)
        {
            action_event evt;
            evt.type = action_event_type::completed;
            evt.name = result.name;
            evt.success = result.success;
            evt.error = result.error;
            evt.duration_ms = result.duration_ms;
            m_event_callback(evt);
        }

        {
            std::lock_guard<std::mutex> lock(m_completed_mutex);
            m_completed_staging.push_back(std::move(result));
        }
    }
}

}  // namespace engine
}  // namespace kryga
