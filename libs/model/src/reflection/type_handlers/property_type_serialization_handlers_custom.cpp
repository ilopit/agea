#pragma once

#include "model/reflection/type_handlers/property_type_serialization_handlers_custom.h"

#include "model/reflection/property.h"

#include "utils/agea_log.h"

#include "model/object_construction_context.h"
#include "model/smart_object.h"
#include "model/components/component.h"
#include "model/caches/cache_set.h"
#include "model/object_constructor.h"
#include "model/caches/objects_cache.h"
#include "serialization/serialization.h"

namespace agea
{
namespace reflection
{
namespace custom
{

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
        auto idx = i;

        auto class_id = utils::id::from(item["id"].as<std::string>());
        auto obj = dc.occ->find_class_obj(class_id);

        if (!obj || obj->get_architype_id() != model::architype::component)
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        r[idx] = obj->as<model::component>();
    }

    return true;
}

bool
game_object_components_prototype(property_prototype_context& ctx)
{
    auto& sc = *ctx.sc;

    auto items = sc[ctx.dst_property->name];
    auto items_size = items.size();

    auto& src_properties =
        extract<std::vector<model::component*>>(ctx.src_obj->as_blob() + ctx.dst_property->offset);

    if (src_properties.empty())
    {
        return true;
    }

    auto& dst_properties =
        extract<std::vector<model::component*>>(ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    AGEA_check(dst_properties.empty(), "Should alway be empty!!");
    AGEA_check(items_size == src_properties.size(), "Should alway be same!!");

    dst_properties.resize(src_properties.size());

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];

        auto class_id = utils::id::from(item["id"].as<std::string>());
        auto obj = ctx.occ->m_instance_local_set.objects->get_item(class_id);
        if (!obj || dst_properties[i])
        {
            ALOG_LAZY_ERROR;
            return false;
        }
        dst_properties[i] = obj->as<model::component>();

        AGEA_check(src_properties[i]->get_id() == obj->get_class_obj()->get_id(),
                   "Should have parent-child");
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
            serialization::conteiner component_conteiner;
            // component_conteiner["class_id"] = class_component->get_id().str();
            component_conteiner["id"] = instance_component->get_id().str();

            //             reflection::serialize_context internal_sc{nullptr, instance_component,
            //                                                       &component_conteiner};
            //             reflection::compare_context compare_ctx{nullptr, class_component,
            //             instance_component};
            //
            //             std::vector<reflection::property*> diff;
            //             model::object_constructor::diff_object_properties(*class_component,
            //             *instance_component,
            //                                                               diff);
            //
            //             for (auto& p : diff)
            //             {
            //                 internal_sc.p = p;
            //
            //                 p->serialization_handler(internal_sc);
            //             }
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

        AGEA_check(obj_components.size() == parent_components.size(), "Should be same size!");

        for (size_t i = 0; i < obj_components.size(); ++i)
        {
            auto class_component = parent_components[i];
            auto obj_component = obj_components[i];

            serialization::conteiner component_conteiner;

            reflection::serialize_context internal_sc{nullptr, obj_component, &component_conteiner};

            std::vector<reflection::property*> diff;
            model::object_constructor::diff_object_properties(*class_component, *obj_component,
                                                              diff);

            component_conteiner["id"] = obj_component->get_id().str();

            for (auto& p : diff)
            {
                internal_sc.p = p;

                p->serialization_handler(internal_sc);
            }

            components_conteiner.push_back(component_conteiner);
        }

        conteiner[dc.p->name] = components_conteiner;
    }

    return true;
}

bool
game_object_components_compare(compare_context&)
{
    // Always different because of IDS
    return false;
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
        auto candidate_id = utils::id::from(ctx.dst_obj->get_id().str() + "/" +
                                            src_col[i]->get_class_obj()->get_id().str());

        auto p =
            model::object_constructor::object_clone_create(*src_col[i], candidate_id, *ctx.occ);
        dst_col[i] = p;
    }

    return true;
}

}  // namespace custom
}  // namespace reflection

}  // namespace agea