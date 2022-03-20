#include "level_constructor.h"

#include "model/level.h"
#include "model/mesh_object.h"
#include "model/components/mesh_component.h"
#include "fs_locator.h"

#include <fstream>
#include <json/json.h>

namespace agea
{
namespace model
{
namespace level_constructor
{
bool
load_level(level& l, const std::string& id)
{
    auto path = glob::resource_locator::get()->resource(category::levels, id);

    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    Json::Value json;
    file >> json;

    auto groups = json["groups"].size();

    for (unsigned idx = 0; idx < groups; ++idx)
    {
        auto& json_group = json["groups"][idx];
        std::cout << json_group["name"].asString() << std::endl;

        auto object_id = json_group["object"].asString();

        auto obj = smart_object_serializer::object_deserialize(object_id);

        auto& items = json_group["instances"];
        auto items_size = items.size();

        for (unsigned i = 0; i < items_size; ++i)
        {
            auto& item = items[i];

            auto clone = obj->META_clone_obj();
            smart_object_serializer::object_deserialize_finalize(item, *clone);
            clone->META_post_construct();

            auto to_push = cast_ref<level_object>(clone);

            l.m_objects.push_back(to_push);
        }
    }

    for (auto ll : l.m_objects)
    {
        ll->prepare_for_rendering();
    }

    return true;
}

}  // namespace level_constructor
}  // namespace model
}  // namespace agea
