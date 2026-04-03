#include "vfs/vfs.h"
#include "vfs/physical_backend.h"

#include <utils/kryga_log.h>

#include <algorithm>
#include <random>

namespace kryga
{
namespace vfs
{

backend*
virtual_file_system::mount(const rid& target,
                           std::filesystem::path root,
                           const mount_config& cfg)
{
    KRG_check(!target.relative().empty(), "Scoped mount requires a subpath (e.g. data://packages/base.apkg)");

    auto be = std::make_unique<physical_backend>(std::move(root));

    if (!cfg.index_filter.empty())
    {
        if (!be->build_index(cfg.index_filter))
        {
            ALOG_ERROR("VFS: failed to build index for '{}' at '{}'", target.str(), be->name());
            return nullptr;
        }

        if (!cfg.load_order.empty())
        {
            be->m_index.set_load_order(cfg.load_order);
        }
    }

    auto* ptr = be.get();

    ALOG_INFO("VFS: mounting '{}' backend '{}' at priority {}", target.str(), be->name(), cfg.priority);
    m_mounts.push_back({std::string(target.mount_point()), std::string(target.relative()), std::move(be), cfg.priority});

    std::stable_sort(m_mounts.begin(), m_mounts.end(),
                     [](const mount_entry& a, const mount_entry& b)
                     {
                         if (a.mount_point != b.mount_point)
                         {
                             return a.mount_point < b.mount_point;
                         }
                         return a.priority > b.priority;
                     });

    return ptr;
}

void
virtual_file_system::mount(std::string mount_point, std::unique_ptr<backend> b, int priority)
{
    ALOG_INFO("VFS: mounting '{}' backend '{}' at priority {}", mount_point, b->name(), priority);
    m_mounts.push_back({std::move(mount_point), {}, std::move(b), priority});

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

virtual_file_system::resolved_backend
virtual_file_system::find_read_backend(std::string_view mount_point,
                                       std::string_view relative) const
{
    for (auto& e : m_mounts)
    {
        if (e.mount_point != mount_point)
        {
            continue;
        }

        // For scoped backends, strip the scope prefix
        auto lookup = std::string(relative);
        if (!e.scope.empty())
        {
            if (!relative.starts_with(e.scope))
            {
                continue;
            }
            lookup = relative.substr(e.scope.size());
            if (!lookup.empty() && lookup.front() == '/')
            {
                lookup = lookup.substr(1);
            }
        }

        auto info = e.be->stat(lookup);
        if (info.exists)
        {
            return {e.be.get(), lookup};
        }
    }

    return {};
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

    auto resolved = find_read_backend(id.mount_point(), id.relative());
    if (!resolved.be)
    {
        ALOG_WARN("VFS: file not found: {}", id.str());
        return false;
    }

    return resolved.be->read_all(resolved.relative, out);
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

    for (auto& e : m_mounts)
    {
        if (e.mount_point != id.mount_point())
        {
            continue;
        }

        auto lookup = std::string(id.relative());
        if (!e.scope.empty())
        {
            if (!id.relative().starts_with(e.scope))
            {
                continue;
            }
            lookup = id.relative().substr(e.scope.size());
            if (!lookup.empty() && lookup.front() == '/')
            {
                lookup = lookup.substr(1);
            }
        }

        auto info = e.be->stat(lookup);
        if (info.exists)
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

    for (auto& e : m_mounts)
    {
        if (e.mount_point != id.mount_point())
        {
            continue;
        }

        auto lookup = std::string(id.relative());
        if (!e.scope.empty())
        {
            if (!id.relative().starts_with(e.scope))
            {
                continue;
            }
            lookup = id.relative().substr(e.scope.size());
            if (!lookup.empty() && lookup.front() == '/')
            {
                lookup = lookup.substr(1);
            }
        }

        if (!e.be->enumerate(lookup, visitor, recursive, ext_filter))
        {
            return false;
        }
    }

    return true;
}

std::optional<rid>
virtual_file_system::find_object(const rid& scope, std::string_view name) const
{
    for (auto& e : m_mounts)
    {
        if (e.mount_point != scope.mount_point())
        {
            continue;
        }

        if (!scope.relative().empty() && e.scope != scope.relative())
        {
            continue;
        }

        std::string relative;
        if (e.be->m_index.resolve(name, relative))
        {
            return rid(scope.mount_point(), e.scope.empty() ? relative : e.scope + "/" + relative);
        }
    }

    return std::nullopt;
}

void
virtual_file_system::enumerate_objects(const rid& scope,
                                       const object_visitor& visitor,
                                       backend* filter_be) const
{
    auto make_rid = [&](const mount_entry& e, const std::string& path) -> rid
    {
        return rid(scope.mount_point(), e.scope.empty() ? path : e.scope + "/" + path);
    };

    if (filter_be)
    {
        // Find the mount entry for this backend to get its scope
        for (auto& e : m_mounts)
        {
            if (e.be.get() != filter_be)
            {
                continue;
            }
            for (auto& [name, path] : filter_be->m_index.ordered_entries())
            {
                if (!visitor(name, make_rid(e, path)))
                {
                    return;
                }
            }
            return;
        }
        return;
    }

    // Merge all matching backends — higher priority wins on duplicates
    std::unordered_map<std::string, rid> merged;

    for (auto it = m_mounts.rbegin(); it != m_mounts.rend(); ++it)
    {
        if (it->mount_point != scope.mount_point())
        {
            continue;
        }

        if (!scope.relative().empty() && it->scope != scope.relative())
        {
            continue;
        }

        for (auto& [name, path] : it->be->m_index.entries())
        {
            merged.try_emplace(name, make_rid(*it, path));
        }
    }

    for (auto& [name, r] : merged)
    {
        if (!visitor(name, r))
        {
            break;
        }
    }
}

std::optional<std::filesystem::path>
virtual_file_system::real_path(const rid& id) const
{
    if(id.empty())
    {
        return std::nullopt;
    }

    for (auto& e : m_mounts)
    {
        if (e.mount_point != id.mount_point())
        {
            continue;
        }

        auto lookup = std::string(id.relative());
        if (!e.scope.empty())
        {
            if (!id.relative().starts_with(e.scope))
            {
                continue;
            }
            lookup = id.relative().substr(e.scope.size());
            if (!lookup.empty() && lookup.front() == '/')
            {
                lookup = lookup.substr(1);
            }
        }

        auto rp = e.be->real_path(lookup);
        if (rp.has_value())
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
