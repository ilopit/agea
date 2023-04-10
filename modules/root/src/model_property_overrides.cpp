#include "root/root_properties_custom.h"

#include "model/reflection/property.h"
#include "model/reflection/reflection_type_utils.h"

#include "model/object_load_context.h"
#include "root/smart_object.h"
#include "root/components/component.h"
#include "model/caches/cache_set.h"
#include "model/object_constructor.h"
#include "model/caches/objects_cache.h"
#include "model/id_generator.h"
#include "root/assets/material.h"

#include <serialization/serialization.h>

#include <utils/agea_log.h>

namespace agea
{
namespace reflection
{
namespace custom
{

result_code
game_object_components_deserialize(deserialize_context& dc)
{
    auto ptr = (blob_ptr)dc.obj;

    auto& sc = *dc.sc;

    auto items = sc[dc.p->name];
    auto items_size = items.size();
    auto& r = utils::as_type<std::vector<root::component*>>(ptr + dc.p->offset);

    if (r.empty())
    {
        r.resize(items_size);
    }
    else
    {
        ALOG_LAZY_ERROR;
    }

    auto layout = sc["layout"];
    auto layout_size = items_size;

    std::vector<int> layout_mapping;
    for (unsigned i = 0; i < layout_size; ++i)
    {
        layout_mapping.push_back(layout[i].as<int>());
    }

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];

        root::smart_object* obj = nullptr;
        auto rc = model::object_constructor::object_load_internal(item, *dc.occ, obj);

        if (rc != result_code::ok || !obj || obj->get_architype_id() != model::architype::component)
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        r[i] = obj->as<root::component>();
        r[i]->set_order_parent_idx(i, layout_mapping[i]);
    }

    return result_code::ok;
}

result_code
game_object_components_prototype(property_prototype_context& ctx)
{
    auto& sc = *ctx.sc;

    auto items = sc[ctx.dst_property->name];
    auto items_size = items.size();

    auto layout = sc["layout"];
    auto layout_size = items_size;

    std::vector<int> layout_mapping;
    for (unsigned i = 0; i < layout_size; ++i)
    {
        layout_mapping.push_back(layout[i].as<int>());
    }

    auto& src_properties = utils::as_type<std::vector<root::component*>>(ctx.src_obj->as_blob() +
                                                                         ctx.dst_property->offset);

    if (src_properties.empty())
    {
        return result_code::ok;
    }

    auto& dst_properties = utils::as_type<std::vector<root::component*>>(ctx.dst_obj->as_blob() +
                                                                         ctx.dst_property->offset);

    AGEA_check(dst_properties.empty(), "Should alway be empty!!");
    AGEA_check(items_size == src_properties.size(), "Should alway be same!!");

    dst_properties.resize(src_properties.size());

    for (unsigned i = 0; i < items_size; ++i)
    {
        auto item = items[i];

        root::smart_object* obj = nullptr;
        auto rc = model::object_constructor::object_load_internal(item, *ctx.occ, obj);

        if (rc != result_code::ok || !obj || obj->get_architype_id() != model::architype::component)
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        dst_properties[i] = obj->as<root::component>();
        dst_properties[i]->set_order_parent_idx(i, layout_mapping[i]);
    }
    return result_code::ok;
}

result_code
game_object_components_serialize(serialize_context& dc)
{
    auto& class_obj = *dc.obj;
    auto& conteiner = *dc.sc;

    if (class_obj.has_flag(root::smart_object_state_flag::standalone))
    {
        serialization::conteiner components_conteiner;
        serialization::conteiner components_layout;
        components_layout.SetStyle(YAML::EmitterStyle::Flow);

        auto& components = agea::reflection::utils::as_type<std::vector<root::component*>>(
            class_obj.as_blob() + dc.p->offset);

        int i = 0;
        for (auto instance_component : components)
        {
            auto class_component = instance_component->get_class_obj();
            serialization::conteiner component_conteiner;

            component_conteiner["id"] = instance_component->get_id().str();
            component_conteiner["class_id"] = class_component->get_id().str();

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
            components_layout.push_back((int)instance_component->get_parent_idx());
        }
        conteiner["layout"] = components_layout;
        conteiner[dc.p->name] = components_conteiner;
    }
    else
    {
        serialization::conteiner components_conteiner;
        serialization::conteiner components_layout;
        components_layout.SetStyle(YAML::EmitterStyle::Flow);

        auto& obj_components =
            utils::as_type<std::vector<root::component*>>(class_obj.as_blob() + dc.p->offset);

        auto& parent_components = utils::as_type<std::vector<root::smart_object*>>(
            class_obj.get_class_obj()->as_blob() + dc.p->offset);

        AGEA_check(obj_components.size() == parent_components.size(), "Should be same size!");

        for (size_t i = 0; i < obj_components.size(); ++i)
        {
            auto class_component = parent_components[i];
            auto obj_component = obj_components[i];

            serialization::conteiner component_conteiner;

            component_conteiner["id"] = obj_component->get_id().str();
            component_conteiner["class_id"] = class_component->get_id().str();

            reflection::serialize_context internal_sc{nullptr, obj_component, &component_conteiner};

            std::vector<reflection::property*> diff;
            model::object_constructor::diff_object_properties(*class_component, *obj_component,
                                                              diff);

            for (auto& p : diff)
            {
                internal_sc.p = p;
                p->serialization_handler(internal_sc);
            }

            components_conteiner[i++] = component_conteiner;
            components_layout.push_back((int)obj_component->get_parent_idx());
        }
        conteiner["layout"] = components_layout;
        conteiner[dc.p->name] = components_conteiner;
    }

    return result_code::ok;
}

result_code
game_object_components_compare(compare_context&)
{
    // Always different because of IDS
    return result_code::failed;
}

result_code
game_object_components_copy(copy_context& ctx)
{
    //     AGEA_check(ctx.occ->get_construction_type() ==
    //                    model::object_constructor_context::construction_type::mirror_obj,
    //                "Should alway be empty!!");

    auto& src_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_property->get_blob(*ctx.src_obj));
    auto& dst_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_property->get_blob(*ctx.dst_obj));

    dst_col.resize(src_col.size());

    for (int i = 0; i < src_col.size(); ++i)
    {
        root::smart_object* obj = nullptr;
        result_code rc = result_code::nav;

        if (ctx.occ->get_construction_type() == model::object_load_type::mirror_copy)
        {
            rc = model::object_constructor::object_clone_create_internal(
                *src_col[i], src_col[i]->get_id(), *ctx.occ, obj);
        }
        else
        {
            auto id =
                glob::id_generator::getr().generate(ctx.src_obj->get_id(), src_col[i]->get_id());

            rc = model::object_constructor::object_clone_create_internal(*src_col[i], id, *ctx.occ,
                                                                         obj);
        }

        if (rc != result_code::ok)
        {
            return rc;
        }

        auto comp = obj->as<root::component>();
        comp->set_order_parent_idx(src_col[i]->get_order_idx(), src_col[i]->get_parent_idx());

        dst_col[i] = comp;
    }

    return result_code::ok;
}

result_code
texture_sample_deserialize(deserialize_context& dc)
{
    auto src = dc.obj->as<root::material>();

    auto& sc = *dc.sc;

    auto item = sc[dc.p->name];

    const auto id = AID(dc.p->name);
    const auto texture_id = AID(item["texture"].as<std::string>());

    root::smart_object* obj = nullptr;
    auto rc = model::object_constructor::object_load_internal(texture_id, *dc.occ, obj);
    if (rc != result_code::ok)
    {
        return rc;
    }

    const auto slot = item["slot"].as<uint32_t>();

    auto& sample = src->get_sample(id);
    sample.txt = obj->as<root::texture>();
    sample.sampler_id = AID(item["sampler"].as<std::string>());
    sample.slot = slot;

    return result_code::ok;
}

result_code
texture_sample_prototype(property_prototype_context& dc)
{
    return result_code::ok;
}

result_code
texture_sample_serialize(serialize_context& dc)
{
    return result_code::ok;
}

result_code
texture_sample_compare(compare_context& ctx)
{
    return result_code::ok;
}

result_code
texture_sample_copy(copy_context& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);

    result_code rc = result_code::ok;

    if (ctx.occ->get_construction_type() != model::object_load_type::mirror_copy)
    {
        dst->get_sample(id) = src->get_sample(id);
    }
    else
    {
        dst->get_sample(id) = src->get_sample(id);

        root::smart_object* obj = nullptr;

        rc = model::object_constructor::object_clone_create_internal(
            src->get_sample(id).txt->get_id(), src->get_sample(id).txt->get_id(), *ctx.occ, obj);

        dst->get_sample(id).txt = obj->as<root::texture>();
    }

    return result_code::ok;
}

}  // namespace custom
}  // namespace reflection

}  // namespace agea