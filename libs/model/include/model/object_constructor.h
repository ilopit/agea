#pragma once

#include "serialization/serialization_fwds.h"
#include "model/model_minimal.h"

#include "model/model_fwds.h"

#include <string>
#include <memory>

namespace agea
{

namespace reflection
{
class property;
}

namespace model
{

object_constructor_context&
default_occ();

class object_constructor
{
public:
    static smart_object*
    class_object_load(const utils::path& path, object_constructor_context& occ = default_occ());

    static smart_object*
    instance_object_load(const utils::path& path, object_constructor_context& occ = default_occ());

    static smart_object*
    object_load_internal(const utils::path& path, object_constructor_context& occ);

    static bool
    class_object_save(const smart_object& obj, const utils::path& object_path);

    static bool
    instance_object_save(const smart_object& obj, const utils::path& object_path);

    static smart_object*
    object_clone_create(const utils::id& src_object_id,
                        const utils::id& new_object_id,
                        object_constructor_context& occ);

    static smart_object*
    object_clone_create(smart_object& src,
                        const utils::id& new_object_id,
                        object_constructor_context& occ);

    static bool
    update_object_properties(smart_object& g,
                             const serialization::conteiner& c,
                             object_constructor_context& occ);
    static bool
    prototype_object_properties(smart_object& from,
                                smart_object& to,
                                const serialization::conteiner& c,
                                object_constructor_context& occ);
    static bool
    clone_object_properties(smart_object& from, smart_object& to, object_constructor_context& occ);

    static bool
    diff_object_properties(const smart_object& left,
                           const smart_object& right,
                           std::vector<reflection::property*>& diff);

    static bool
    read_container(const std::string& object_id, serialization::conteiner& conteiner, category c);

    static bool
    object_properties_load(smart_object& obj,
                           const serialization::conteiner& jc,
                           object_constructor_context& occ);

    static bool
    object_properties_save(const smart_object& obj, serialization::conteiner& jc);

    static smart_object*
    object_load_full(serialization::conteiner& sc, object_constructor_context& occ);

    static bool
    object_save_full(serialization::conteiner& sc, const smart_object& obj);

    static smart_object*
    object_load_partial(smart_object& prototype_obj,
                        serialization::conteiner& sc,
                        object_constructor_context& occ);

    static bool
    object_save_partial(serialization::conteiner& sc, const smart_object& obj);
};

}  // namespace model
}  // namespace agea
