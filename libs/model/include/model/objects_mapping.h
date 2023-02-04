#pragma once

#include <serialization/serialization_fwds.h>

#include <utils/id.h>
#include <utils/path.h>

#include <unordered_map>

namespace agea
{
namespace model
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

    std::unordered_map<utils::id, item> m_items;
};
}  // namespace model
}  // namespace agea