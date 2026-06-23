#pragma once

#include <serialization/serialization_fwds.h>

#include "core/object_load_type.h"
#include "core/construction_utils.h"
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
}  // namespace reflection

namespace core
{

class object_load_context;

struct name_of
{
    utils::id pattern;
};

class object_constructor
{
public:
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

    // --- Loading ---

    std::expected<root::smart_object*, result_code>
    load_obj(const utils::id& id);

    std::expected<root::smart_object*, result_code>
    load_sub_object(serialization::container& c);

    // --- Saving ---

    result_code
    save_obj(const root::smart_object& obj);

    // --- Instantiation ---

    std::expected<root::smart_object*, result_code>
    instantiate_obj(root::smart_object& proto, const utils::id& new_id);

    std::expected<root::smart_object*, result_code>
    clone_obj(root::smart_object& src, const utils::id& new_id);

    std::expected<root::smart_object*, result_code>
    construct_obj(const utils::id& type_id,
                  const utils::id& id,
                  const root::smart_object::construct_params& params,
                  bool is_proto);

    std::expected<root::smart_object*, result_code>
    construct_obj(const utils::id& type_id,
                  const name_of& name,
                  const root::smart_object::construct_params& params,
                  bool is_proto);

    std::expected<root::smart_object*, result_code>
    create_default_class_obj_impl(reflection::reflection_type* rt);

private:
    std::expected<root::smart_object*, result_code>
    object_load_internal(serialization::container& c);

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
    load_derive_object_properties(root::smart_object& from,
                                  root::smart_object& to,
                                  const serialization::container& c);

    result_code
    clone_object_properties(root::smart_object& from, root::smart_object& to);

    result_code
    instantiate_object_properties(root::smart_object& from, root::smart_object& to);

    object_load_context* m_olc = nullptr;
    object_load_type m_mode = object_load_type::class_obj;
};

}  // namespace core
}  // namespace kryga
