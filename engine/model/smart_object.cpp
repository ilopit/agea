#include "smart_object.h"

namespace agea
{
namespace model
{
bool
smart_object::clone(this_class& src)
{
    m_class_id = src.m_class_id;
    m_id = src.m_id;

    return true;
}

bool
smart_object::serialize(json_conteiner& c)
{
    return true;
}

bool
smart_object::deserialize(json_conteiner& c)
{
    game_object_serialization_helper::read_if_exists<std::string>("id", c, m_id);

    return true;
}

bool
smart_object::deserialize_finalize(json_conteiner& c)
{
    game_object_serialization_helper::read_if_exists<std::string>("id", c, m_id);

    return true;
}

bool
smart_object::post_construct()
{
    return true;
}

}  // namespace model
}  // namespace agea
