#pragma once

// Named, cross-process, auto-reset event. One producer signals, one or more
// consumers `wait_for` with a timeout. Maps to:
//   - POSIX: sem_open / sem_post / sem_timedwait
//   - Windows: CreateEventW / SetEvent / WaitForSingleObject
//
// Auto-reset semantics: a successful wait consumes exactly one signal. On
// POSIX this matches sem_wait's natural behavior; on Windows we pass
// `bManualReset = FALSE`.
//
// All waits take a timeout. Per the plan: no infinite waits — a missed
// signal must not livelock either side.

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace kryga::editor_ipc
{

class named_event
{
public:
    enum class mode
    {
        create,  // Create or attach (idempotent on both platforms).
        attach,  // Fails if the primitive does not exist.
    };

    named_event() = default;
    ~named_event();

    named_event(const named_event&) = delete;
    named_event& operator=(const named_event&) = delete;

    named_event(named_event&& other) noexcept;
    named_event& operator=(named_event&& other) noexcept;

    // `name` is the bare suffix; wrapper prepends the OS-specific prefix
    // (see editor/README.md for the full scheme).
    bool
    open(std::string_view name, mode m);

    void
    close();

    bool
    is_open() const;

    // Signal the event. Non-blocking.
    void
    signal();

    // Returns true if a signal was consumed, false on timeout.
    bool
    wait_for(std::chrono::milliseconds timeout);

    const std::string&
    last_error() const
    {
        return m_last_error;
    }

private:
    void* m_handle = nullptr;  // sem_t* on POSIX, HANDLE on Windows.
    std::string m_name;
    std::string m_last_error;
    bool m_created = false;
};

}  // namespace kryga::editor_ipc
