#pragma once

#include <serialization/serialization_fwds.h>

#include <utils/id.h>
#include <utils/path.h>
#include <vfs/rid.h>

#include <unordered_map>

namespace kryga
{
namespace core
{
class object_mapping
{
    struct item
    {
        bool is_class = false;
        utils::path p;
    };

public:
    bool
    build_object_mapping(const vfs::rid& id);

    bool
    build_object_mapping(serialization::container& c, bool is_class);

    bool
    build_from_vfs(const vfs::rid& root, bool is_class);

    void
    clear();

    object_mapping&
    add(const utils::id& id, bool is_class, const utils::path& p);

    std::unordered_map<utils::id, item> m_items;
};
}  // namespace core
}  // namespace kryga