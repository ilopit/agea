#include "model/objects_mapping.h"

#include <serialization/serialization.h>

#include <utils/agea_log.h>

namespace agea
{
namespace model
{

bool
object_mapping::buiild_object_mapping(const utils::path& p)
{
    serialization::conteiner c;
    if (!serialization::read_container(p, c))
    {
        return false;
    }

    if (!buiild_object_mapping(c, true))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!buiild_object_mapping(c, false))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
object_mapping::buiild_object_mapping(serialization::conteiner& c, bool is_class)
{
    auto mapping = c[is_class ? "class_obj_mapping" : "instance_obj_mapping"];
    auto mapping_size = mapping.size();

    for (unsigned i = 0; i < mapping_size; ++i)
    {
        auto item = mapping[i];
        for (auto ss : item)
        {
            auto f = AID(ss.first.as<std::string>());
            auto s = APATH(ss.second.as<std::string>());
            auto& m = m_items[f];
            m.first = is_class;
            m.second = s;
        }
    }
    return true;
}

}  // namespace model
}  // namespace agea