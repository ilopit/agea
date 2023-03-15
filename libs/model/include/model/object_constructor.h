#pragma once

#include <serialization/serialization_fwds.h>

#include "model/object_load_type.h"
#include "model/model_minimal.h"
#include "model/model_fwds.h"
#include "model/smart_object.h"

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
class object_constructor
{
public:
    // Public API
    static result_code
    object_load(const utils::path& path_in_package,
                object_load_type type,
                object_load_context& occ,
                smart_object*& obj,
                std::vector<smart_object*>& loaded_obj);

    static result_code
    object_load(const utils::id& id,
                object_load_type type,
                object_load_context& occ,
                smart_object*& obj,
                std::vector<smart_object*>& loaded_obj);

    static result_code
    mirror_object(const utils::id& class_object_id,
                  object_load_context& occ,
                  smart_object*& obj,
                  std::vector<smart_object*>& loaded_obj);

    static smart_object*
    construct_package_object(const utils::id& type_id,
                             const utils::id& id,
                             const model::smart_object::construct_params& p,
                             object_load_context& olc);

    static result_code
    object_clone(smart_object& src,
                 const utils::id& new_object_id,
                 object_load_context& occ,
                 smart_object*& obj,
                 std::vector<smart_object*>& loaded_obj);

    template <typename T>
    static result_code
    register_package_type(object_load_context& occ)
    {
        return register_package_type_impl(alloc_empty_object<T>(), occ);
    }

    static result_code
    object_load_internal(const utils::path& path_in_package,
                         object_load_context& occ,
                         smart_object*& obj);

    static result_code
    object_load_internal(const utils::id& id, object_load_context& occ, smart_object*& obj);

    static result_code
    object_load_internal(serialization::conteiner& c, object_load_context& occ, smart_object*& obj);

    // Utils
    static result_code
    object_save(const smart_object& obj, const utils::path& object_path);

    static result_code
    object_clone_create_internal(const utils::id& src_object_id,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 smart_object*& obj);

    static result_code
    object_clone_create_internal(smart_object& src,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 smart_object*& obj);

    static result_code
    update_object_properties(smart_object& g,
                             const serialization::conteiner& c,
                             object_load_context& occ);
    static result_code
    prototype_object_properties(smart_object& from,
                                smart_object& to,
                                const serialization::conteiner& c,
                                object_load_context& occ);
    static result_code
    clone_object_properties(smart_object& from, smart_object& to, object_load_context& occ);

    static result_code
    diff_object_properties(const smart_object& left,
                           const smart_object& right,
                           std::vector<reflection::property*>& diff);

    static result_code
    object_properties_load(smart_object& obj,
                           const serialization::conteiner& jc,
                           object_load_context& occ);

    static result_code
    object_properties_save(const smart_object& obj, serialization::conteiner& jc);

    template <typename T>
    static std::shared_ptr<T>
    alloc_empty_object(const utils::id& id = T::META_type_id())
    {
        auto empty = T::META_class_create_empty_obj();
        empty->META_set_id(id);

        return empty;
    }

    static smart_object*
    alloc_empty_object(const utils::id& type_id,
                       const utils::id& id,
                       uint32_t extra_flags,
                       bool add_global,
                       object_load_context& olc);

private:
    static result_code
    register_package_type_impl(std::shared_ptr<smart_object> empty, object_load_context& olc);

    static result_code
    object_load_full(serialization::conteiner& sc, object_load_context& occ, smart_object*& obj);

    static result_code
    object_save_full(serialization::conteiner& sc, const smart_object& obj);

    static result_code
    object_load_partial(smart_object& prototype_obj,
                        serialization::conteiner& sc,
                        object_load_context& occ,
                        smart_object*& obj);

    static result_code
    object_save_partial(serialization::conteiner& sc, const smart_object& obj);
};

}  // namespace model
}  // namespace agea
