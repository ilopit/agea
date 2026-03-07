#include "core/path_resolver.h"

namespace kryga
{
namespace core
{

bool
path_resolver::make_full_path(const utils::path& relative_path, utils::path& p) const
{
    if (m_path_prefix.empty())
    {
        return false;
    }

    p = m_path_prefix;
    p.append(relative_path);

    return true;
}

bool
path_resolver::make_full_path(const utils::id& id, utils::path& p) const
{
    if (!m_object_mapping)
    {
        return false;
    }

    auto itr = m_object_mapping->m_items.find(id);
    if (itr == m_object_mapping->m_items.end())
    {
        return false;
    }

    return make_full_path(itr->second.p, p);
}

}  // namespace core
}  // namespace kryga
