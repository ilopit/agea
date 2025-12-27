#include "core/id_generator.h"

#include "core/caches/caches_map.h"
#include "global_state/global_state.h"

#include <packages/root/model/smart_object.h>

#include <utils/agea_log.h>

#include <format>

namespace agea
{

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

    AGEA_check(pos, "Should not be first");

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
        auto& node = m_mapping[AID(obj_id_raw)];

        std::string s = std::format("{}#{}", obj_id_raw.c_str(), node.ctr);
        ++node.ctr;

        if (glob::glob_state().get_instance_objects_cache()->has_item(AID(s)) ||
            glob::glob_state().get_class_objects_cache()->has_item(AID(s)))
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