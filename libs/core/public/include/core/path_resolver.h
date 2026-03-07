#pragma once

#include "core/objects_mapping.h"

#include <utils/path.h>

#include <memory>

namespace kryga
{
namespace core
{
class path_resolver
{
public:
    bool
    make_full_path(const utils::path& relative_path, utils::path& p) const;

    bool
    make_full_path(const utils::id& id, utils::path& p) const;

    path_resolver&
    set_prefix_path(const utils::path& v)
    {
        m_path_prefix = v;
        return *this;
    }

    const utils::path&
    get_prefix_path() const
    {
        return m_path_prefix;
    }

    path_resolver&
    set_objects_mapping(const std::shared_ptr<object_mapping>& v)
    {
        m_object_mapping = v;
        return *this;
    }

    object_mapping&
    get_objects_mapping() const
    {
        return *m_object_mapping;
    }

private:
    utils::path m_path_prefix;
    std::shared_ptr<object_mapping> m_object_mapping = std::make_shared<object_mapping>();
};
}  // namespace core
}  // namespace kryga
