#include "component.h"

namespace agea
{
namespace model
{
bool
component::clone(this_class& src)
{
    AGEA_return_nok(base_class::clone(src));

    m_parent_idx = src.m_parent_idx;
    m_order_idx = src.m_order_idx;

    return true;
}

bool
component::deserialize(json_conteiner& c)
{
    base_class::deserialize(c);

    game_object_serialization_helper::read_if_exists<uint32_t>("order", c, m_order_idx);
    game_object_serialization_helper::read_if_exists<uint32_t>("parent", c, m_parent_idx);

    return true;
}

bool
component::deserialize_finalize(json_conteiner& c)
{
    base_class::deserialize_finalize(c);

    game_object_serialization_helper::read_if_exists<uint32_t>("order", c, m_order_idx);
    game_object_serialization_helper::read_if_exists<uint32_t>("parent", c, m_parent_idx);

    return true;
}

}  // namespace model
}  // namespace agea
