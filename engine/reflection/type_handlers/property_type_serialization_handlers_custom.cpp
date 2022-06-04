#pragma once

#include "reflection/type_handlers/property_type_serialization_handlers_custom.h"

#include "reflection/property.h"

#include "utils/agea_log.h"

#include "model/object_construction_context.h"
#include "model/smart_object.h"
#include "model/components/component.h"

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
game_object_components_deserialize(deserialize_context& dc)
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
game_object_components_serialize(serialize_context& dc)
{
    auto& class_obj = *dc.obj;
    auto& conteiner = *dc.sc;

    if (class_obj.is_class_obj())
    {
        serialization::conteiner components_conteiner;

        auto& components =
            extract<std::vector<model::smart_object*>>(class_obj.as_blob() + dc.p->offset);

        int i = 0;
        for (auto instance_component : components)
        {
            auto class_component = instance_component->get_class_obj();

            serialization::conteiner component_conteiner;
            component_conteiner["object_class"] = class_component->get_id();
            component_conteiner["id"] = instance_component->get_id();

            reflection::serialize_context internal_sc{nullptr, instance_component,
                                                      &component_conteiner};
            reflection::compare_context compare_ctx{nullptr, class_component, instance_component};

            std::vector<reflection::property*> diff;
            model::object_constructor::diff_object_properties(*class_component, *instance_component,
                                                              diff);

            for (auto& p : diff)
            {
                internal_sc.p = p;

                p->serialization_handler(internal_sc);
            }
            components_conteiner[i++] = component_conteiner;
        }

        conteiner[dc.p->name] = components_conteiner;
    }
    else
    {
        serialization::conteiner components_conteiner;

        auto& obj_components =
            extract<std::vector<model::component*>>(class_obj.as_blob() + dc.p->offset);

        auto& parent_components = extract<std::vector<model::component*>>(
            class_obj.get_class_obj()->as_blob() + dc.p->offset);

        AGEA_check(obj_components.size() == parent_components.size(), "Shoild be same size!");

        for (size_t i = 0; i < obj_components.size(); ++i)
        {
            auto class_component = parent_components[i];
            auto obj_component = obj_components[i];

            serialization::conteiner component_conteiner;

            reflection::serialize_context internal_sc{nullptr, obj_component, &component_conteiner};

            std::vector<reflection::property*> diff;
            model::object_constructor::diff_object_properties(*class_component, *obj_component,
                                                              diff);

            if (!diff.empty())
            {
                component_conteiner["id"] = obj_component->get_id();
                component_conteiner["order_idx"] = obj_component->get_order_idx();
            }
            for (auto& p : diff)
            {
                internal_sc.p = p;

                p->serialization_handler(internal_sc);
            }
            if (!diff.empty())
            {
                components_conteiner.push_back(component_conteiner);
            }
        }

        conteiner[dc.p->name] = components_conteiner;
    }

    return true;
}

bool
game_object_components_compare(compare_context& ctx)
{
    auto& dst_components =
        extract<std::vector<model::smart_object*>>(ctx.dst_obj->as_blob() + ctx.p->offset);
    auto& src_components =
        extract<std::vector<model::smart_object*>>(ctx.src_obj->as_blob() + ctx.p->offset);

    if (dst_components.size() != src_components.size())
    {
        return false;
    }

    for (size_t i = 0; i < dst_components.size(); ++i)
    {
        std::vector<reflection::property*> props;

        model::object_constructor::diff_object_properties(*dst_components[i], *src_components[i],
                                                          props);

        if (!props.empty())
        {
            return false;
        }
    }

    return true;
}

bool
game_object_components_copy(copy_context& ctx)
{
    auto& src_col =
        extract<std::vector<model::smart_object*>>(ctx.src_property->get_blob(*ctx.src_obj));
    auto& dst_col =
        extract<std::vector<model::smart_object*>>(ctx.dst_property->get_blob(*ctx.dst_obj));

    dst_col.resize(src_col.size());

    for (int i = 0; i < src_col.size(); ++i)
    {
        ctx.src_property->types_copy_handler(*ctx.src_obj, *ctx.dst_obj, (blob_ptr)&src_col[i],
                                             (blob_ptr)&dst_col[i], *ctx.occ);
    }

    return true;
}

}  // namespace custom
}  // namespace reflection

}  // namespace agea