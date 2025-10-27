#pragma once

#include <serialization/serialization_fwds.h>

#include "core/object_load_type.h"
#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/component.h>

#include <string>
#include <memory>

namespace agea
{

namespace reflection
{
class property;
}

namespace core
{
class object_constructor
{
public:
    // Public API
    static result_code
    object_load(const utils::path& path_in_package,
                object_load_type type,
                object_load_context& occ,
                root::smart_object*& obj,
                std::vector<root::smart_object*>& loaded_obj);

    static result_code
    object_load(const utils::id& id,
                object_load_type type,
                object_load_context& occ,
                root::smart_object*& obj,
                std::vector<root::smart_object*>& loaded_obj);

    static result_code
    mirror_object(const utils::id& class_object_id,
                  object_load_context& occ,
                  root::smart_object*& obj,
                  std::vector<root::smart_object*>& loaded_obj);

    static root::smart_object*
    object_construct(const utils::id& type_id,
                     const utils::id& id,
                     const root::smart_object::construct_params& p,
                     object_load_context& olc);

    static result_code
    object_clone(root::smart_object& src,
                 const utils::id& new_object_id,
                 object_load_context& occ,
                 root::smart_object*& obj,
                 std::vector<root::smart_object*>& loaded_obj);

    template <typename T>
    static result_code
    create_default_class_obj_impl(object_load_context& occ)
    {
        return create_default_class_obj_impl(alloc_empty_object<T>(), occ);
    }

    template <typename T>
    static result_code
    destroy_default_class_obj_impl(object_load_context& occ)
    {
        destroy_default_class_obj_impl(T::AR_TYPE_reflection().type_name, occ);

        return result_code::ok;
    }

    static result_code
    object_load_internal(const utils::path& path_in_package,
                         object_load_context& occ,
                         root::smart_object*& obj);

    static result_code
    object_load_internal(const utils::id& id, object_load_context& occ, root::smart_object*& obj);

    static result_code
    object_load_internal(serialization::conteiner& c,
                         object_load_context& occ,
                         root::smart_object*& obj);

    // Utils
    static result_code
    object_save(const root::smart_object& obj, const utils::path& object_path);

    static result_code
    object_clone_create_internal(const utils::id& src_object_id,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 root::smart_object*& obj);

    static result_code
    object_clone_create_internal(root::smart_object& src,
                                 const utils::id& new_object_id,
                                 object_load_context& occ,
                                 root::smart_object*& obj);

    static result_code
    update_object_properties(root::smart_object& g,
                             const serialization::conteiner& c,
                             object_load_context& occ);
    static result_code
    prototype_object_properties(root::smart_object& from,
                                root::smart_object& to,
                                const serialization::conteiner& c,
                                object_load_context& occ);
    static result_code
    clone_object_properties(root::smart_object& from,
                            root::smart_object& to,
                            object_load_context& occ);

    static result_code
    diff_object_properties(const root::smart_object& left,
                           const root::smart_object& right,
                           std::vector<reflection::property*>& diff);

    static result_code
    object_properties_load(root::smart_object& obj,
                           const serialization::conteiner& jc,
                           object_load_context& occ);

    static result_code
    object_properties_save(const root::smart_object& obj, serialization::conteiner& jc);

    template <typename T>
    static std::shared_ptr<T>
    alloc_empty_object(const utils::id& id = T::AR_TYPE_id())
    {
        auto empty = T::AR_TYPE_create_empty_obj(id);

        return empty;
    }

    static root::smart_object*
    alloc_empty_object(const utils::id& proto_id,
                       const utils::id& id,
                       uint32_t extra_flags,
                       object_load_context& olc);

    static root::smart_object*
    alloc_empty_object(const utils::id& id,
                       reflection::reflection_type* rt,
                       uint32_t extra_flags,
                       object_load_context& olc);

private:
    static result_code
    create_default_class_obj_impl(std::shared_ptr<root::smart_object> empty,
                                  object_load_context& olc);

    static result_code
    destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc);

    static result_code
    object_load_full(serialization::conteiner& sc,
                     object_load_context& occ,
                     root::smart_object*& obj);

    static result_code
    object_save_full(serialization::conteiner& sc, const root::smart_object& obj);

    static result_code
    object_load_partial(root::smart_object& prototype_obj,
                        serialization::conteiner& sc,
                        object_load_context& occ,
                        root::smart_object*& obj);

    static result_code
    object_save_partial(serialization::conteiner& sc, const root::smart_object& obj);
};

}  // namespace core
}  // namespace agea
