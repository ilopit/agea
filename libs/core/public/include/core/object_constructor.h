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

namespace kryga
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

    object_constructor(object_load_context* olc,
                       object_load_type mode = object_load_type::class_obj)
        : m_olc(olc)
        , m_mode(mode)
    {
    }

    object_load_context*
    get_olc() const
    {
        return m_olc;
    }

    object_load_type
    get_mode() const
    {
        return m_mode;
    }

    std::expected<root::smart_object*, result_code>
    load_package_obj(const utils::id& id);

    std::expected<root::smart_object*, result_code>
    load_level_obj(const utils::id& id);

    // Context-aware load — dispatches based on construction type stack
    std::expected<root::smart_object*, result_code>
    load_obj(const utils::id& id);

    std::expected<root::smart_object*, result_code>
    load_obj(serialization::container& c);

    std::expected<root::smart_object*, result_code>
    instantiate_obj(root::smart_object& proto, const utils::id& new_id);

    // Uses construction type from stack to determine flags
    std::expected<root::smart_object*, result_code>
    clone_obj(root::smart_object& src, const utils::id& new_id);

    std::expected<root::smart_object*, result_code>
    construct_obj(const utils::id& type_id,
                  const utils::id& id,
                  const root::smart_object::construct_params& params);

    // Static utilities (no OLC context needed)

    static result_code
    diff_object_properties(const root::smart_object& left,
                           const root::smart_object& right,
                           std::vector<reflection::property*>& diff);

    static result_code
    object_save(const root::smart_object& obj, const utils::path& object_path);

    static result_code
    object_properties_save(const root::smart_object& obj, serialization::container& jc);

    template <typename T>
    static std::shared_ptr<T>
    alloc_empty_object(const utils::id& id = T::AR_TYPE_id())
    {
        return T::AR_TYPE_create_empty_obj(id);
    }

    template <typename T>
    static result_code
    destroy_default_class_obj_impl(object_load_context& occ)
    {
        destroy_default_class_obj_impl(T::AR_TYPE_reflection().type_name, occ);
        return result_code::ok;
    }

private:
    std::expected<root::smart_object*, result_code>
    create_default_class_obj_impl(reflection::reflection_type* rt);

    std::expected<root::smart_object*, result_code>
    object_load_internal(serialization::container& c);

    result_code
    load_derive_object_properties(root::smart_object& from,
                                  root::smart_object& to,
                                  const serialization::container& c);

    std::expected<root::smart_object*, result_code>
    preload_proto(const utils::id& id);

    std::expected<root::smart_object*, result_code>
    object_load_derive(root::smart_object& prototype_obj, serialization::container& sc);

    std::expected<root::smart_object*, result_code>
    alloc_empty_object(const utils::id& type_id,
                       const utils::id& id,
                       root::smart_object_flags flags,
                       root::smart_object* parent_object);

    result_code
    clone_object_properties(root::smart_object& from, root::smart_object& to);

    result_code
    instantiate_object_properties(root::smart_object& from, root::smart_object& to);

    static result_code
    destroy_default_class_obj_impl(const utils::id& id, object_load_context& olc);

    static result_code
    object_save_full(serialization::container& sc, const root::smart_object& obj);

    static result_code
    object_save_internal(serialization::container& sc, const root::smart_object& obj);

    object_load_context* m_olc = nullptr;
    object_load_type m_mode = object_load_type::class_obj;
};

}  // namespace core
}  // namespace kryga
