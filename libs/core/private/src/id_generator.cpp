#include "core/id_generator.h"

#include <core/caches/caches_map.h>
#include <core/caches/objects_cache.h>
#include <root/smart_object.h>

#include <utils/agea_log.h>

#include <format>

namespace agea
{

glob::id_generator::type glob::id_generator::type::s_instance;

namespace core
{

namespace
{
void
clear_numbered_part(std::string& id)
{
    auto pos = id.find('#');
    if (pos == std::string::npos)
    {
        return;
    }

    AGEA_check(pos, "Shoud not be first");

    id.resize(pos);
}
}  // namespace

agea::utils::id
id_generator::generate(const utils::id& obj_id)
{
    auto obj_id_raw = obj_id.str();
    clear_numbered_part(obj_id_raw);

    utils::id new_id;

    for (;;)
    {
        auto ctr = m_mapping[AID(obj_id_raw)].ctr++;
        std::string s = std::format("{}#{}", obj_id_raw.c_str(), ctr);

        if (glob::proto_objects_cache::getr().has_item(AID(s)) ||
            glob::objects_cache::getr().has_item(AID(s)))
        {
            continue;
        }
        new_id = AID(s);
        break;
    }

    return new_id;
}

utils::id
id_generator::generate(const utils::id& old_obj_id, const utils::id& old_component_id)
{
    auto old_component_id_raw = old_component_id.str();
    clear_numbered_part(old_component_id_raw);

    utils::id new_id;

    for (;;)
    {
        auto ctr = m_mapping[AID(old_component_id_raw)].ctr++;
        std::string s =
            std::format("{}/{}#{}", old_obj_id.cstr(), old_component_id_raw.c_str(), ctr);

        if (glob::proto_objects_cache::getr().has_item(AID(s)) ||
            glob::objects_cache::getr().has_item(AID(s)))
        {
            continue;
        }
        new_id = AID(s);
        break;
    }

    return new_id;
}

}  // namespace core
}  // namespace agea