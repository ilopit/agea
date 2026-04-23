#include "editor_ipc/named_event.h"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <cstring>
#endif

namespace kryga::editor_ipc
{

namespace
{
#if defined(_WIN32)
std::wstring
format_event_name(std::string_view name)
{
    std::wstring out = L"Local\\kryga_editor_";
    out.reserve(out.size() + name.size());
    for (char c : name)
    {
        out.push_back(static_cast<wchar_t>(c));
    }
    return out;
}
#else
std::string
format_event_name(std::string_view name)
{
    std::string out = "/kryga_editor_";
    out.append(name);
    return out;
}
#endif
}  // namespace

named_event::named_event(named_event&& other) noexcept
    : m_handle(other.m_handle)
    , m_name(std::move(other.m_name))
    , m_last_error(std::move(other.m_last_error))
    , m_created(other.m_created)
{
    other.m_handle = nullptr;
    other.m_created = false;
}

named_event&
named_event::operator=(named_event&& other) noexcept
{
    if (this != &other)
    {
        close();
        m_handle = other.m_handle;
        m_name = std::move(other.m_name);
        m_last_error = std::move(other.m_last_error);
        m_created = other.m_created;
        other.m_handle = nullptr;
        other.m_created = false;
    }
    return *this;
}

named_event::~named_event()
{
    close();
}

bool
named_event::is_open() const
{
#if defined(_WIN32)
    return m_handle != nullptr;
#else
    return m_handle != nullptr && m_handle != SEM_FAILED;
#endif
}

bool
named_event::open(std::string_view name, mode m)
{
    close();
    m_name.assign(name);

#if defined(_WIN32)
    auto wname = format_event_name(name);
    HANDLE h = nullptr;
    if (m == mode::create)
    {
        h = CreateEventW(nullptr, /*bManualReset*/ FALSE, /*bInitialState*/ FALSE, wname.c_str());
        if (!h)
        {
            m_last_error = "CreateEvent failed (code " + std::to_string(GetLastError()) + ")";
            return false;
        }
    }
    else
    {
        h = OpenEventW(EVENT_ALL_ACCESS, FALSE, wname.c_str());
        if (!h)
        {
            m_last_error = "OpenEvent failed (code " + std::to_string(GetLastError()) + ")";
            return false;
        }
    }
    m_handle = h;
    m_created = (m == mode::create);
    return true;
#else
    auto full = format_event_name(name);
    sem_t* sem = nullptr;
    if (m == mode::create)
    {
        // Wipe any stale sempahore so a leftover from a crashed run does
        // not hand out pre-signaled waits.
        sem_unlink(full.c_str());
        sem = sem_open(full.c_str(), O_CREAT | O_EXCL, 0600, 0);
    }
    else
    {
        sem = sem_open(full.c_str(), 0);
    }

    if (sem == SEM_FAILED)
    {
        m_last_error = std::string("sem_open failed: ") + std::strerror(errno);
        m_handle = nullptr;
        return false;
    }

    m_handle = sem;
    m_created = (m == mode::create);
    return true;
#endif
}

void
named_event::close()
{
    if (!is_open())
    {
        m_handle = nullptr;
        m_created = false;
        m_name.clear();
        return;
    }

#if defined(_WIN32)
    CloseHandle(static_cast<HANDLE>(m_handle));
#else
    sem_close(static_cast<sem_t*>(m_handle));
    if (m_created)
    {
        auto full = format_event_name(m_name);
        sem_unlink(full.c_str());
    }
#endif

    m_handle = nullptr;
    m_created = false;
    m_name.clear();
}

void
named_event::signal()
{
    if (!is_open()) return;
#if defined(_WIN32)
    SetEvent(static_cast<HANDLE>(m_handle));
#else
    sem_post(static_cast<sem_t*>(m_handle));
#endif
}

bool
named_event::wait_for(std::chrono::milliseconds timeout)
{
    if (!is_open()) return false;
#if defined(_WIN32)
    DWORD ms = static_cast<DWORD>(timeout.count());
    return WaitForSingleObject(static_cast<HANDLE>(m_handle), ms) == WAIT_OBJECT_0;
#else
    // sem_timedwait uses CLOCK_REALTIME — compute absolute deadline from
    // now.  We intentionally do NOT use sem_clockwait / CLOCK_MONOTONIC
    // because sem_clockwait is a Linux glibc extension that macOS doesn't
    // ship. Small realtime-clock jumps produce at most one spurious extra
    // timeout cycle, which the caller's loop absorbs.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long long nsec = ts.tv_nsec + (timeout.count() % 1000) * 1'000'000LL;
    ts.tv_sec += timeout.count() / 1000 + nsec / 1'000'000'000LL;
    ts.tv_nsec = static_cast<long>(nsec % 1'000'000'000LL);

    while (true)
    {
        int r = sem_timedwait(static_cast<sem_t*>(m_handle), &ts);
        if (r == 0) return true;
        if (errno == EINTR) continue;
        return false;  // ETIMEDOUT or other.
    }
#endif
}

}  // namespace kryga::editor_ipc
