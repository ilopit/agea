#pragma once

#include "reflection/type_handlers/property_type_serialization_handlers_custom.h"

#include "reflection/property.h"

#include "utils/agea_log.h"

#include "model/object_construction_context.h"
#include "model/smart_object.h"

#include "model/object_constructor.h"

#include "serialization/serialization.h"

namespace agea
{
namespace reflection
{
namespace custom
{

namespace
{
model::component*
load_component(serialization::conteiner& sc, model::object_constructor_context& occ)
{
    auto class_id = sc["object_class"].as<std::string>();

    auto id = sc["id"].as<std::string>();

    auto obj = model::object_constructor::object_clone_create(class_id, id, occ);

    if (!obj)
    {
        ALOG_ERROR("object [{0}] doesn't exists", class_id);
        return false;
    }

    if (!model::object_constructor::update_object_properties(*obj, sc))
    {
        ALOG_ERROR("object [{0}] update failed", class_id);
        return false;
    }

    if (!occ.propagate_to_co_cache())
    {
        return false;
    }

    return (model::component*)obj;
}
}  // namespace

bool
deserialize_game_object_components(deserialize_context& dc)
{
    auto ptr = (blob_ptr)dc.obj;

    auto& sc = *dc.sc;

    auto items = sc[dc.p->name];
    auto items_size = items.size();
    auto& r = extract<std::vector<model::component*>>(ptr + dc.p->offset);

    if (r.empty())
    {
        r.resize(items_size);
    }

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];
        auto idx = item["order_idx"].as<std::uint32_t>();
        r[idx] = load_component(item, *dc.occ);
    }

    return true;
}

bool
serialize_game_object_components(serialize_context& dc)
{
    //     auto instance_ptr = (blob_ptr)dc.obj;
    //     auto class_ptr = (blob_ptr)dc.obj->m_class_obj;
    //
    //     auto& instance_components_list =
    //         extract<std::vector<model::smart_object*>>(instance_ptr + dc.p->offset);
    //     auto& class_components_list =
    //         extract<std::vector<model::smart_object*>>(class_ptr + dc.p->offset);
    //
    //     AGEA_check(instance_components_list.size() == class_components_list.size(),
    //                "Should be same size");
    //
    //     for (auto iitr = instance_components_list.begin(), citr = class_components_list.begin();
    //          iitr != instance_components_list.end(); ++citr, ++iitr)
    //     {
    //         auto& instance_component = **iitr;
    //         auto& class_component = **citr;
    //
    //         AGEA_check((instance_component.type_id() == class_component.type_id()) &&
    //                        (instance_component.m_class_obj->id() ==
    //                        class_component.m_class_obj->id()),
    //                    "Should have same origin");
    //
    //         auto& properties = instance_component.reflection()->m_serilalization_properties;
    //
    //         for (auto& p : properties)
    //         {
    //             auto same = p->types_compare_handler(instance_component, class_component,
    //                                                  (blob_ptr)&instance_component + p->offset,
    //                                                  (blob_ptr)&class_component + p->offset);
    //
    //             int i = 2;
    //         }
    //     }

    serialization::conteiner components_conteiner;

    auto& class_obj = *dc.obj;
    auto& conteiner = *dc.sc;
    AGEA_check(!class_obj.get_class_obj(), "Should be called only for class objs");

    auto& components =
        extract<std::vector<model::smart_object*>>(class_obj.as_blob() + dc.p->offset);

    int i = 0;
    for (auto instance_component : components)
    {
        auto class_component = instance_component->get_class_obj();
        auto& properties = instance_component->reflection()->m_serilalization_properties;

        serialization::conteiner component_conteiner;
        component_conteiner["object_class"] = class_component->get_id();
        component_conteiner["id"] = instance_component->get_id();

        reflection::serialize_context internal_sc{nullptr, instance_component,
                                                  &component_conteiner};
        reflection::compare_context compare_ctx{nullptr, class_component, instance_component};

        for (auto& p : properties)
        {
            compare_ctx.p = p.get();
            internal_sc.p = p.get();
            auto same = reflection::property::compare(compare_ctx);

            if (!same)
            {
                p->serialization_handler(internal_sc);
            }
        }
        components_conteiner[i++] = component_conteiner;
    }

    conteiner[dc.p->name] = components_conteiner;

    return true;
}

}  // namespace custom
}  // namespace reflection

}  // namespace agea