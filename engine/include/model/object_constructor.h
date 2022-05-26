#pragma once

#include "serialization/serialization_fwds.h"

#include "core/fs_locator.h"
#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "model/caches/class_object_cache.h"

#include <string>
#include <memory>

namespace agea
{
namespace model
{

object_constructor_context&
default_occ();

class object_constructor
{
public:
    static bool
    object_save(serialization::conteiner& c, const std::string& object_id);

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
    update_object_properties(smart_object& g, const serialization::conteiner& c);

    static bool
    clone_object_properties(smart_object& from, smart_object& to, object_constructor_context& occ);

private:
    static bool
    read_container(const std::string& object_id, serialization::conteiner& conteiner, category c);

    static bool
    object_properties_load(smart_object& obj,
                           const serialization::conteiner& jc,
                           object_constructor_context&);
};

}  // namespace model
}  // namespace agea
