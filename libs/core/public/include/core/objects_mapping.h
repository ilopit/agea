#pragma once

#include <serialization/serialization_fwds.h>

#include <utils/id.h>
#include <utils/path.h>

#include <unordered_map>

namespace agea
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
    buiild_object_mapping(const utils::path& p);

    bool
    buiild_object_mapping(serialization::conteiner& c, bool is_class);

    void
    clear();

    object_mapping&
    add(const utils::id& id, bool is_class, const utils::path& p);

    std::unordered_map<utils::id, item> m_items;
};
}  // namespace core
}  // namespace agea