#pragma once

#include <serialization/serialization_fwds.h>

#include "core/object_load_type.h"
#include "core/model_minimal.h"
#include "core/model_fwds.h"

#include <packages/root/model/smart_object.h>
#include <packages/root/model/components/component.h>

#include <string>
#include <memory>
#include <expected>

namespace agea
{

namespace reflection
{
class property;
}

namespace core
{

static inline const auto ks_class_constructed = root::smart_object_flags{.instance_obj = false,
                                                                         .derived_obj = false,
                                                                         .runtime_obj = true,
                                                                         .mirror_obj = false,
                                                                         .default_obj = false};

static inline const auto ks_instance_constructed = root::smart_object_flags{.instance_obj = true,
                                                                            .derived_obj = false,
                                                                            .runtime_obj = true,
                                                                            .mirror_obj = false,
                                                                            .default_obj = false};

static inline const auto ks_class_default = root::smart_object_flags{.instance_obj = false,
                                                                     .derived_obj = false,
                                                                     .runtime_obj = true,
                                                                     .mirror_obj = false,
                                                                     .default_obj = true};

static inline const auto ks_class = root::smart_object_flags{.instance_obj = false,
                                                             .derived_obj = false,
                                                             .runtime_obj = false,
                                                             .mirror_obj = false,
                                                             .default_obj = false};

static inline const auto ks_class_derived = root::smart_object_flags{.instance_obj = false,
                                                                     .derived_obj = true,
                                                                     .runtime_obj = false,
                                                                     .mirror_obj = false,
                                                                     .default_obj = false};

static inline const auto ks_instance_derived = root::smart_object_flags{.instance_obj = true,
                                                                        .derived_obj = true,
                                                                        .runtime_obj = false,
                                                                        .mirror_obj = false,
                                                                        .default_obj = false};

static inline const auto ks_instance_mirror = root::smart_object_flags{.instance_obj = true,
                                                                       .derived_obj = false,
                                                                       .runtime_obj = true,
                                                                       .mirror_obj = true,
                                                                       .default_obj = false};

class object_constructor
{
public:
    // Public API

    static std::expected<root::smart_object*, result_code>
    object_load(const utils::id& id,
                object_load_type type,
                object_load_context& occ,
                std::vector<root::smart_object*>& loaded_obj);

    static std::expected<root::smart_object*, result_code>
    object_construct(const utils::id& type_id,
                     const utils::id& id,
                     const root::smart_object::construct_params& p,
                     object_load_context& olc);

    static std::expected<root::smart_object*, result_code>
    object_clone(root::smart_object& src,
                 object_load_type type,
                 const utils::id& new_object_id,
                 object_load_context& occ,
                 std::vector<root::smart_object*>& loaded_obj);

    static std::expected<root::smart_object*, result_code>
    object_instantiate(root::smart_object& proto_obj,
                       const utils::id& new_object_id,
                       object_load_context& occ,
                       std::vector<root::smart_object*>& loaded_obj);

    template <typename T>
    static result_code
    destroy_default_class_obj_impl(object_load_context& occ)
    {
        destroy_default_class_obj_impl(T::AR_TYPE_reflection().type_name, occ);

        return result_code::ok;
    }

    // Utils
    static result_code
    object_save(const root::smart_object& obj, const utils::path& object_path);

    static std::expected<root::smart_object*, result_code>
    object_clone_create_internal(const utils::id& src_object_id,
                                 const utils::id& new_object_id,
                                 object_load_context& occ);

    static std::expected<root::smart_object*, result_code>
    object_clone_create_internal(root::smart_object& src,
                                 const utils::id& new_object_id,
                                 object_load_context& occ);

    static std::expected<root::smart_object*, result_code>
    object_instanciate_internal(root::smart_object& src,
                                const utils::id& new_object_id,
                                object_load_context& occ);

    static result_code
    load_derive_object_properties(root::smart_object& from,
                                  root::smart_object& to,
                                  const serialization::conteiner& c,
                                  object_load_context& occ);

    static result_code
    clone_object_properties(root::smart_object& from,
                            root::smart_object& to,
                            object_load_context& occ);

    static result_code
    instantiate_object_properties(root::smart_object& from,
                                  root::smart_object& to,
                                  object_load_context& occ);

    static result_code
    diff_object_properties(const root::smart_object& left,
                           const root::smart_object& right,
                           std::vector<reflection::property*>& diff);

    static result_code
    object_properties_save(const root::smart_object& obj, serialization::conteiner& jc);

    static std::expected<root::smart_object*, result_code>
    alloc_empty_object(const utils::id& type_id,
                       const utils::id& id,
                       root::smart_object_flags flags,
                       root::smart_object* parent_object,
                       object_load_context& olc);

    static std::expected<root::smart_object*, result_code>
    object_load_internal(const utils::id& id, object_load_context& occ);

    static std::expected<root::smart_object*, result_code>
    object_load_internal(serialization::conteiner& c, object_load_context& occ);

    static std::expected<root::smart_object*, result_code>
    preload_proto(const utils::id& id, object_load_context& occ);

    static std::expected<root::smart_object*, result_code>
    create_default_default_class_proto(const utils::id& id, object_load_context& olc);

    template <typename T>
    static std::shared_ptr<T>
    alloc_empty_object(const utils::id& id = T::AR_TYPE_id())
    {
        auto empty = T::AR_TYPE_create_empty_obj(id);

        return empty;
    }

private:
    static std::expected<root::smart_object*, result_code>
    create_default_class_obj_impl(std::shared_ptr<root::smart_object> empty,
                                  object_load_context& olc);

    static result_code
    destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc);

    static result_code
    object_save_full(serialization::conteiner& sc, const root::smart_object& obj);

    static std::expected<root::smart_object*, result_code>
    object_load_derive(root::smart_object& prototype_obj,
                       serialization::conteiner& sc,
                       object_load_context& occ);

    static result_code
    object_save_partial(serialization::conteiner& sc, const root::smart_object& obj);
};

}  // namespace core
}  // namespace agea
