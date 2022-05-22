#pragma once

#include "serialization/json_serialization.h"

#include "core/fs_locator.h"
#include "core/agea_minimal.h"
#include "model/caches/class_object_cache.h"

#include <string>
#include <memory>

namespace agea
{
namespace model
{
class smart_object;
class game_object;
class component;
struct object_constructor_context;

object_constructor_context&
default_occ();

class object_constructor
{
public:
    static bool
    object_save(serialization::json_conteiner& c, const std::string& object_id);

    static smart_object*
    class_object_load(const std::string& object_path,
                      object_constructor_context& occ = default_occ());

    static smart_object*
    class_object_save(const std::string& object_path);

    static smart_object*
    object_clone_create(const std::string& src_object_id,
                        const std::string& new_object_id,
                        object_constructor_context& occ);

    static smart_object*
    object_clone_create(smart_object& src,
                        const std::string& new_object_id,
                        object_constructor_context& occ);

    static bool
    update_object_properties(smart_object& g, serialization::json_conteiner& c);

    static bool
    clone_object_properties(smart_object& from, smart_object& to, object_constructor_context& occ);

private:
    static bool
    read_container(const std::string& object_id,
                   serialization::json_conteiner& conteiner,
                   category c);

    static bool
    read_container(const std::string& object_id, serialization::json_conteiner& conteiner);

    static void
    object_properties_load(smart_object& obj,
                           serialization::json_conteiner& jc,
                           object_constructor_context&);
};

}  // namespace model
}  // namespace agea
