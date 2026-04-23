#include "editor_ipc/shared_memory.h"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#endif

namespace kryga::editor_ipc
{

namespace
{
#if defined(_WIN32)
std::string
format_shm_name(std::string_view name)
{
    std::string out = "Local\\kryga_editor_";
    out.append(name);
    return out;
}
#else
std::string
format_shm_name(std::string_view name)
{
    std::string out = "/kryga_editor_";
    out.append(name);
    return out;
}

std::string
last_errno_message()
{
    return std::strerror(errno);
}
#endif
}  // namespace

shared_memory::shared_memory(shared_memory&& other) noexcept
    : m_base(other.m_base)
    , m_size(other.m_size)
    , m_handle(other.m_handle)
    , m_name(std::move(other.m_name))
    , m_last_error(std::move(other.m_last_error))
    , m_created(other.m_created)
{
    other.m_base = nullptr;
    other.m_size = 0;
    other.m_handle = -1;
    other.m_created = false;
}

shared_memory&
shared_memory::operator=(shared_memory&& other) noexcept
{
    if (this != &other)
    {
        close();
        m_base = other.m_base;
        m_size = other.m_size;
        m_handle = other.m_handle;
        m_name = std::move(other.m_name);
        m_last_error = std::move(other.m_last_error);
        m_created = other.m_created;
        other.m_base = nullptr;
        other.m_size = 0;
        other.m_handle = -1;
        other.m_created = false;
    }
    return *this;
}

shared_memory::~shared_memory()
{
    close();
}

bool
shared_memory::open(std::string_view name, mode m, size_t size_bytes)
{
    close();
    m_name.assign(name);
    const auto full = format_shm_name(name);

#if defined(_WIN32)
    if (m == mode::create)
    {
        HANDLE h = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>((uint64_t)size_bytes >> 32),
            static_cast<DWORD>(size_bytes & 0xFFFFFFFFu),
            full.c_str());
        if (!h)
        {
            m_last_error = "CreateFileMapping failed (code " + std::to_string(GetLastError()) + ")";
            return false;
        }
        // GetLastError() == ERROR_ALREADY_EXISTS is acceptable here — it
        // means a previous instance left the mapping around. We take
        // ownership anyway.
        m_handle = reinterpret_cast<intptr_t>(h);
        m_created = true;
    }
    else
    {
        HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, full.c_str());
        if (!h)
        {
            m_last_error = "OpenFileMapping failed (code " + std::to_string(GetLastError()) + ")";
            return false;
        }
        m_handle = reinterpret_cast<intptr_t>(h);
        m_created = false;
    }

    void* view = MapViewOfFile(
        reinterpret_cast<HANDLE>(m_handle), FILE_MAP_ALL_ACCESS, 0, 0, size_bytes);
    if (!view)
    {
        m_last_error = "MapViewOfFile failed (code " + std::to_string(GetLastError()) + ")";
        CloseHandle(reinterpret_cast<HANDLE>(m_handle));
        m_handle = -1;
        return false;
    }
    m_base = view;
    m_size = size_bytes;
    return true;

#else  // POSIX
    const int oflags = (m == mode::create) ? (O_CREAT | O_RDWR) : O_RDWR;
    if (m == mode::create)
    {
        // Clear any stale region with the same name. shm_unlink returns -1
        // with ENOENT when none exists — that's fine, ignore.
        shm_unlink(full.c_str());
    }

    int fd = shm_open(full.c_str(), oflags, 0600);
    if (fd < 0)
    {
        m_last_error = "shm_open failed: " + last_errno_message();
        return false;
    }

    if (m == mode::create)
    {
        if (ftruncate(fd, static_cast<off_t>(size_bytes)) != 0)
        {
            m_last_error = "ftruncate failed: " + last_errno_message();
            ::close(fd);
            shm_unlink(full.c_str());
            return false;
        }
    }

    void* view = mmap(nullptr, size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (view == MAP_FAILED)
    {
        m_last_error = "mmap failed: " + last_errno_message();
        ::close(fd);
        if (m == mode::create)
        {
            shm_unlink(full.c_str());
        }
        return false;
    }

    m_handle = fd;
    m_base = view;
    m_size = size_bytes;
    m_created = (m == mode::create);
    return true;
#endif
}

void
shared_memory::close()
{
    if (m_base)
    {
#if defined(_WIN32)
        UnmapViewOfFile(m_base);
#else
        munmap(m_base, m_size);
#endif
        m_base = nullptr;
    }

    if (m_handle != -1)
    {
#if defined(_WIN32)
        CloseHandle(reinterpret_cast<HANDLE>(m_handle));
#else
        ::close(static_cast<int>(m_handle));
        if (m_created && !m_name.empty())
        {
            auto full = format_shm_name(m_name);
            shm_unlink(full.c_str());
        }
#endif
        m_handle = -1;
    }

    m_size = 0;
    m_created = false;
    m_name.clear();
    m_last_error.clear();
}

void
shared_memory::unlink_stale(std::string_view name)
{
#if defined(_WIN32)
    // Windows file mappings are reference-counted; closing the last handle
    // removes them automatically. Nothing to do.
    (void)name;
#else
    auto full = format_shm_name(name);
    shm_unlink(full.c_str());
#endif
}

}  // namespace kryga::editor_ipc
