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
    static result_code
    object_load(const utils::path& path_in_package,
                object_constructor_context& occ,
                smart_object*& obj);

    static result_code
    object_load(const utils::id& id, object_constructor_context& occ, smart_object*& obj);

    static result_code
    object_load(serialization::conteiner& c, object_constructor_context& occ, smart_object*& obj);

    static result_code
    object_save(const smart_object& obj, const utils::path& object_path);

    static result_code
    object_clone_create(const utils::id& src_object_id,
                        const utils::id& new_object_id,
                        object_constructor_context& occ,
                        smart_object*& obj);

    static result_code
    object_clone_create(smart_object& src,
                        const utils::id& new_object_id,
                        object_constructor_context& occ,
                        smart_object*& obj);

    static result_code
    update_object_properties(smart_object& g,
                             const serialization::conteiner& c,
                             object_constructor_context& occ);
    static result_code
    prototype_object_properties(smart_object& from,
                                smart_object& to,
                                const serialization::conteiner& c,
                                object_constructor_context& occ);
    static result_code
    clone_object_properties(smart_object& from, smart_object& to, object_constructor_context& occ);

    static result_code
    diff_object_properties(const smart_object& left,
                           const smart_object& right,
                           std::vector<reflection::property*>& diff);

    static result_code
    object_properties_load(smart_object& obj,
                           const serialization::conteiner& jc,
                           object_constructor_context& occ);

    static result_code
    object_properties_save(const smart_object& obj, serialization::conteiner& jc);

    static std::shared_ptr<smart_object>
    create_empty_object(const utils::id& type_id, const utils::id& obj_id);

    template <typename T>
    static std::shared_ptr<T>
    create_empty_object(const utils::id& obj_id)
    {
        return std::static_pointer_cast<T>(create_empty_object(T::META_type_id(), obj_id));
    }

    static result_code
    object_load_full(serialization::conteiner& sc,
                     object_constructor_context& occ,
                     smart_object*& obj);

    static result_code
    object_save_full(serialization::conteiner& sc, const smart_object& obj);

    static result_code
    object_load_partial(smart_object& prototype_obj,
                        serialization::conteiner& sc,
                        object_constructor_context& occ,
                        smart_object*& obj);

    static result_code
    object_save_partial(serialization::conteiner& sc, const smart_object& obj);
};

}  // namespace model
}  // namespace agea
