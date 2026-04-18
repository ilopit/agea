#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <deque>
#include <atomic>

namespace kryga
{
namespace engine
{

// Progress state for a running action, updated from the worker thread
struct action_progress
{
    std::atomic<float> progress{0.0f};  // 0.0 to 1.0
    std::string status;                 // current step description
    std::mutex status_mutex;

    void
    set_status(const std::string& s)
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        status = s;
    }

    std::string
    get_status()
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        return status;
    }
};

// A named async action submitted to the queue
struct action
{
    std::string name;
    std::function<void(action_progress& progress)> work;
};

// Result of a completed action
struct action_result
{
    std::string name;
    bool success = false;
    std::string error;
    float duration_ms = 0.0f;
};

// Simple single-worker action queue. Actions run one at a time on a background thread.
// The main thread polls for completion and can read progress.
class action_queue
{
public:
    action_queue() = default;
    ~action_queue();

    // Submit an action. It will run after any currently queued actions complete.
    void
    submit(action a);

    // Call from main thread each frame. Moves completed results to the finished list.
    void
    tick();

    // True if an action is currently executing
    bool
    is_busy() const
    {
        return m_running.load();
    }

    // Current action name (empty if idle)
    std::string
    current_action_name();

    // Progress of the current action (thread-safe)
    action_progress*
    current_progress()
    {
        return &m_progress;
    }

    // Completed actions since last clear
    const std::vector<action_result>&
    finished() const
    {
        return m_finished;
    }

    void
    clear_finished()
    {
        m_finished.clear();
    }

    // Number of actions waiting (not including the currently running one)
    size_t
    queued_count();

private:
    void
    worker_loop();
    void
    start_worker();

    std::deque<action> m_pending;
    std::mutex m_pending_mutex;

    action_progress m_progress;
    std::atomic<bool> m_running{false};

    std::vector<action_result> m_finished;

    // Completed results from worker thread, moved to m_finished in tick()
    std::vector<action_result> m_completed_staging;
    std::mutex m_completed_mutex;

    std::thread m_worker;
    std::atomic<bool> m_shutdown{false};
    std::condition_variable m_cv;
};

}  // namespace engine
}  // namespace kryga
