#include "vfs/vfs.h"

#include <utils/kryga_log.h>

#include <algorithm>
#include <random>

namespace kryga
{
namespace vfs
{
void
virtual_file_system::mount(std::string mount_point, std::unique_ptr<backend> b, int priority)
{
    ALOG_INFO("VFS: mounting '{}' backend '{}' at priority {}", mount_point, b->name(), priority);
    m_mounts.push_back({std::move(mount_point), std::move(b), priority});

    std::stable_sort(m_mounts.begin(), m_mounts.end(),
                     [](const mount_entry& a, const mount_entry& b)
                     {
                         if(a.mount_point != b.mount_point)
                         {
                             return a.mount_point < b.mount_point;
                         }
                         return a.priority > b.priority;
                     });
}

void
virtual_file_system::unmount(std::string_view mount_point, backend* b)
{
    auto it = std::remove_if(m_mounts.begin(), m_mounts.end(), [&](const mount_entry& e)
                             { return e.mount_point == mount_point && e.be.get() == b; });

    m_mounts.erase(it, m_mounts.end());

    auto wt = m_write_targets.find(std::string(mount_point));
    if(wt != m_write_targets.end() && wt->second == b)
    {
        m_write_targets.erase(wt);
    }
}

void
virtual_file_system::set_write_target(std::string_view mount_point, backend* b)
{
    m_write_targets[std::string(mount_point)] = b;
}

backend*
virtual_file_system::find_read_backend(std::string_view mount_point,
                                       std::string_view relative) const
{
    for(auto& e : m_mounts)
    {
        if(e.mount_point != mount_point)
        {
            continue;
        }

        auto info = e.be->stat(relative);
        if(info.exists)
        {
            return e.be.get();
        }
    }

    return nullptr;
}

backend*
virtual_file_system::find_write_backend(std::string_view mount_point) const
{
    auto wt = m_write_targets.find(std::string(mount_point));
    if(wt != m_write_targets.end())
    {
        return wt->second;
    }

    for(auto& e : m_mounts)
    {
        if(e.mount_point != mount_point)
        {
            continue;
        }

        if(!e.be->is_read_only())
        {
            return e.be.get();
        }
    }

    return nullptr;
}

bool
virtual_file_system::read_bytes(const rid& id, std::vector<uint8_t>& out) const
{
    if(id.empty())
    {
        return false;
    }

    auto* be = find_read_backend(id.mount_point(), id.relative());
    if(!be)
    {
        ALOG_WARN("VFS: file not found: {}", id.str());
        return false;
    }

    return be->read_all(id.relative(), out);
}

bool
virtual_file_system::read_string(const rid& id, std::string& out) const
{
    std::vector<uint8_t> bytes;
    if(!read_bytes(id, bytes))
    {
        return false;
    }

    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool
virtual_file_system::write_bytes(const rid& id, std::span<const uint8_t> data)
{
    if(id.empty())
    {
        return false;
    }

    auto* be = find_write_backend(id.mount_point());
    if(!be)
    {
        ALOG_ERROR("VFS: no writable backend for mount '{}' (path: {})", id.mount_point(),
                   id.str());
        return false;
    }

    return be->write_all(id.relative(), data);
}

bool
virtual_file_system::write_string(const rid& id, std::string_view data)
{
    auto bytes =
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    return write_bytes(id, bytes);
}

file_info
virtual_file_system::stat(const rid& id) const
{
    if(id.empty())
    {
        return {};
    }

    for(auto& e : m_mounts)
    {
        if(e.mount_point != id.mount_point())
        {
            continue;
        }

        auto info = e.be->stat(id.relative());
        if(info.exists)
        {
            return info;
        }
    }

    return {};
}

bool
virtual_file_system::exists(const rid& id) const
{
    return stat(id).exists;
}

bool
virtual_file_system::create_directories(const rid& id)
{
    if(id.empty())
    {
        return false;
    }

    auto* be = find_write_backend(id.mount_point());
    if(!be)
    {
        return false;
    }

    return be->create_directories(id.relative());
}

bool
virtual_file_system::remove(const rid& id)
{
    if(id.empty())
    {
        return false;
    }

    auto* be = find_write_backend(id.mount_point());
    if(!be)
    {
        return false;
    }

    return be->remove(id.relative());
}

bool
virtual_file_system::enumerate(const rid& id,
                               const backend::enumerate_cb& visitor,
                               bool recursive,
                               std::string_view ext_filter) const
{
    if(id.empty())
    {
        return false;
    }

    for(auto& e : m_mounts)
    {
        if(e.mount_point != id.mount_point())
        {
            continue;
        }

        if(!e.be->enumerate(id.relative(), visitor, recursive, ext_filter))
        {
            return false;
        }
    }

    return true;
}

std::optional<std::filesystem::path>
virtual_file_system::real_path(const rid& id) const
{
    if(id.empty())
    {
        return std::nullopt;
    }

    for(auto& e : m_mounts)
    {
        if(e.mount_point != id.mount_point())
        {
            continue;
        }

        auto rp = e.be->real_path(id.relative());
        if(rp.has_value())
        {
            return rp;
        }
    }

    return std::nullopt;
}

temp_dir_context
virtual_file_system::create_temp_dir()
{
    auto rand_part = [](size_t length)
    {
        std::string result;
        constexpr char data[] = "0123456789ABCDEF";
        for(size_t i = 0; i < length; ++i)
        {
            result += data[rand() % 16];
        }
        return result;
    };

    rid tmp_id("tmp", rand_part(16));

    create_directories(tmp_id);

    auto rp = real_path(tmp_id);
    if(rp.has_value())
    {
        return temp_dir_context(APATH(rp.value()));
    }

    ALOG_ERROR("VFS: failed to create temp dir — no real path for tmp://");
    return {};
}

}  // namespace vfs
}  // namespace kryga
