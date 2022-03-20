#pragma once

#include <unordered_map>
#include <memory>

namespace agea
{
namespace model
{
class smart_object;

using prototypes_list = std::unordered_map<std::string, std::shared_ptr<smart_object>>;

template <typename T>
struct smart_object_prototype_registrator
{
    smart_object_prototype_registrator()
    {
        smart_object_prototype_registry::register_game_object<T>();
    }
};

struct smart_object_prototype_registry
{
    static void get_registry(prototypes_list*& or);

    static std::shared_ptr<smart_object> get_empty(const std::string& id);

    template <typename T>
    static void
    register_game_object()
    {
        prototypes_list* or = nullptr;
        get_registry(or);

        (* or)[T::META_class_id()] = T::META_class_create_empty_obj();
    }
};

}  // namespace model
}  // namespace agea
