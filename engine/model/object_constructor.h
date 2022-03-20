#pragma once

#include "serialization/json_serialization.h"

#include "fs_locator.h"
#include "agea_minimal.h"

#include <string>
#include <memory>

namespace agea
{
namespace model
{
class smart_object;
class level_object;
class component;

class smart_object_serializer
{
public:
    static bool object_serialize(json_conteiner& c, const std::string& object_id);

    static std::shared_ptr<smart_object> object_deserialize(const std::string& object_id,
                                                            category c = category::objects);

    template <typename T>
    static std::shared_ptr<T>
    object_deserialize_concrete(const std::string& object_id, category c = category::objects)
    {
        auto obj = object_deserialize(object_id, c);

        return cast_ref<T>(obj);
    }

    static bool object_deserialize_finalize(json_conteiner& c, smart_object& go);

    static bool components_deserialize(json_conteiner& c, level_object& go);

private:
    static bool read_container(const std::string& object_id,
                               json_conteiner& conteiner,
                               category c = category::objects);
};

}  // namespace model
}  // namespace agea
