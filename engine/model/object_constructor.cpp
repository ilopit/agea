#include "object_constructor.h"

#include "model/components/component.h"
#include "model/level_object.h"
#include "model/mesh_object.h"

#include "fs_locator.h"

#include <fstream>
#include <json/json.h>

namespace agea
{
namespace model
{
bool
smart_object_serializer::read_container(const std::string& object_id,
                                        json_conteiner& conteiner,
                                        category c /*= category::objects*/)
{
    auto path = glob::resource_locator::get()->resource(c, object_id);

    std::ifstream file(path);
    if (!file.is_open())
    {
        return nullptr;
    }

    Json::Value json;
    file >> conteiner;

    return true;
}

bool
smart_object_serializer::object_serialize(json_conteiner& c, const std::string& object_id)
{
    return true;
}

std::shared_ptr<smart_object>
smart_object_serializer::object_deserialize(const std::string& object_id, category c)
{
    json_conteiner jc;
    read_container(object_id, jc, c);

    auto id = jc["class_id"].asString();
    auto empty = smart_object_prototype_registry::get_empty(id);

    empty->META_deserialize(jc);

    return empty;
}

bool
smart_object_serializer::object_deserialize_finalize(json_conteiner& c, smart_object& go)
{
    return go.META_deserialize_finalize(c);
}

bool
smart_object_serializer::components_deserialize(json_conteiner& c, level_object& go)
{
    if (!c.isMember("components"))
    {
        return false;
    }

    auto& items = c["components"];
    auto items_size = items.size();

    auto size = go.m_components.size() + items_size;
    go.m_components.resize(size);

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto& item = items[i];

        auto id = item["id"].asString();

        auto obj = smart_object_serializer::object_deserialize_concrete<component>(id);
        obj->META_deserialize_finalize(item);
        obj->META_post_construct();

        go.m_components[obj->m_order_idx] = obj;
    }

    return true;
}

}  // namespace model
}  // namespace agea
