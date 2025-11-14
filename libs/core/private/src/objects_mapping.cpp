#include "core/objects_mapping.h"

#include <serialization/serialization.h>

#include <utils/agea_log.h>

namespace agea
{
namespace core
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
            m.is_class = is_class;
            m.p = s;
        }
    }
    return true;
}

void
object_mapping::clear()
{
    m_items.clear();
}

object_mapping&
object_mapping::add(const utils::id& id, bool is_class, const utils::path& p)
{
    auto& i = m_items[id];
    i.is_class = is_class;
    i.p = p;

    return *this;
}

}  // namespace core
}  // namespace agea