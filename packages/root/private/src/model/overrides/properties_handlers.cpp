#include "packages/root/model/overrides/properties_handlers.h"

#include "core/reflection/property.h"
#include "core/reflection/reflection_type_utils.h"

#include "core/object_load_context.h"
#include "core/caches/cache_set.h"
#include "core/object_constructor.h"
#include "core/id_generator.h"
#include "core/global_state.h"

#include "packages/root/model/smart_object.h"
#include "packages/root/model/components/component.h"
#include "packages/root/model/assets/material.h"

#include <serialization/serialization.h>

#include <utils/agea_log.h>
#include <error_handling/error_handling.h>

namespace agea::root
{

result_code
game_object_components_deserialize(::agea::reflection::deserialize_context& dc)
{
    auto ptr = (blob_ptr)dc.obj;

    auto& sc = *dc.sc;

    auto items = sc[dc.p->name];
    auto items_size = items.size();
    auto& r = ::agea::reflection::utils::as_type<std::vector<root::component*>>(ptr + dc.p->offset);

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
        auto rc = core::object_constructor::object_load_internal(item, *dc.occ, obj);

        if (rc != result_code::ok || !obj || obj->get_architype_id() != core::architype::component)
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
game_object_components_prototype(::agea::reflection::property_prototype_context& ctx)
{
    auto& sc = *ctx.sc;

    auto items = sc[ctx.dst_property->name];
    auto number_of_components = items.size();

    auto layout = sc["layout"];
    auto layout_size = number_of_components;

    std::vector<int> layout_mapping;
    for (unsigned i = 0; i < layout_size; ++i)
    {
        layout_mapping.push_back(layout[i].as<int>());
    }

    auto& src_properties = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_obj->as_blob() + ctx.dst_property->offset);

    if (src_properties.empty())
    {
        return result_code::ok;
    }

    auto& dst_properties = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    AGEA_check(dst_properties.empty(), "Should alway be empty!!");
    AGEA_check(number_of_components == src_properties.size(), "Should alway be same!!");

    dst_properties.resize(src_properties.size());

    for (unsigned i = 0; i < number_of_components; ++i)
    {
        auto item = items[i];

        root::smart_object* obj = nullptr;
        auto rc = core::object_constructor::object_load_internal(item, *ctx.occ, obj);

        if (rc != result_code::ok || !obj || obj->get_architype_id() != core::architype::component)
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
game_object_components_serialize(::agea::reflection::serialize_context& dc)
{
    auto& class_obj = *dc.obj;
    auto& conteiner = *dc.sc;

    if (class_obj.get_flags().standalone)
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
            core::object_constructor::diff_object_properties(*class_component, *instance_component,
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

        auto& obj_components = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
            class_obj.as_blob() + dc.p->offset);

        auto& parent_components =
            ::agea::reflection::utils::as_type<std::vector<root::smart_object*>>(
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
            core::object_constructor::diff_object_properties(*class_component, *obj_component,
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
game_object_components_compare(::agea::reflection::compare_context&)
{
    // Always different because of IDS
    return result_code::failed;
}

result_code
game_object_components_copy(::agea::reflection::copy_context& ctx)
{
    //     AGEA_check(ctx.occ->get_construction_type() ==
    //                    model::object_constructor_context::construction_type::mirror_obj,
    //                "Should alway be empty!!");

    auto& src_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_property->get_blob(*ctx.src_obj));
    auto& dst_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_property->get_blob(*ctx.dst_obj));

    dst_col.resize(src_col.size());
    auto gen = glob::glob_state().get_id_generator();

    for (int i = 0; i < src_col.size(); ++i)
    {
        root::smart_object* obj = nullptr;
        result_code rc = result_code::nav;

        if (ctx.occ->get_construction_type() == core::object_load_type::mirror_copy)
        {
            rc = core::object_constructor::object_clone_create_internal(
                *src_col[i], src_col[i]->get_id(), *ctx.occ, obj);
        }
        else
        {
            auto id = gen->generate(ctx.src_obj->get_id());

            rc = core::object_constructor::object_clone_create_internal(*src_col[i], id, *ctx.occ,
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
game_object_load_derive(::agea::reflection::property_load_derive_context& ctx)
{
    AGEA_check(ctx.dst_property->name == "components", "Only compoentns expected");

    auto& sc = *ctx.sc;

    auto components = sc[ctx.dst_property->name];
    auto layout = sc["layout"];

    if ((!components) != (!layout))
    {
        ALOG_ERROR("Both layout and components should exist/not exist");
        return result_code::failed;
    }

    auto& src_components = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_obj->as_blob() + ctx.dst_property->offset);

    auto& dst_components = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    AGEA_check(dst_components.empty(), "Should alway be empty!!");

    if (components)
    {
        auto components_size = components.size();
        auto layout_size = layout.size();

        if (components_size != layout_size)
        {
            ALOG_ERROR("Missconfigured layout");
            return result_code::failed;
        }

        dst_components.resize(components_size);

        for (unsigned i = 0; i < components_size; ++i)
        {
            auto item = components[i];

            root::smart_object* obj = nullptr;
            auto rc = core::object_constructor::object_load_internal(item, *ctx.occ, obj);

            if (rc != result_code::ok || !obj ||
                obj->get_architype_id() != core::architype::component)
            {
                ALOG_LAZY_ERROR;
                return result_code::failed;
            }

            dst_components[i] = obj->as<root::component>();
            dst_components[i]->set_order_parent_idx(i, layout[i].as<int>());
        }
    }
    else
    {
        for (auto c : src_components)
        {
            AGEA_check(c->get_flags().proto_obj, "Only protos are alowed");

            auto gid = glob::glob_state().get_id_generator()->generate(c->get_id());

            root::smart_object* obj = nullptr;
            if (core::object_constructor::object_clone_create_internal(*c, gid, *ctx.occ, obj) !=
                result_code::ok)
            {
                return result_code::fallback;
            }

            dst_components.push_back(obj->as<root::component>());
        }
    }

    return result_code::ok;
}

result_code
texture_sample_deserialize(::agea::reflection::deserialize_context& dc)
{
    auto src = dc.obj->as<root::material>();

    auto& sc = *dc.sc;

    auto item = sc[dc.p->name];

    const auto id = AID(dc.p->name);
    const auto texture_id = AID(item["texture"].as<std::string>());

    root::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load_internal(texture_id, *dc.occ, obj);
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
texture_sample_prototype(::agea::reflection::property_prototype_context& dc)
{
    return result_code::ok;
}

result_code
texture_sample_serialize(::agea::reflection::serialize_context& dc)
{
    return result_code::ok;
}

result_code
texture_sample_compare(::agea::reflection::compare_context& ctx)
{
    return result_code::ok;
}

result_code
texture_sample_copy(::agea::reflection::copy_context& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);

    result_code rc = result_code::ok;

    if (ctx.occ->get_construction_type() != core::object_load_type::mirror_copy)
    {
        dst->set_sample(id, src->get_sample(id));
    }
    else
    {
        dst->set_sample(id, src->get_sample(id));

        root::smart_object* obj = nullptr;

        rc = core::object_constructor::object_clone_create_internal(
            src->get_sample(id).txt->get_id(), src->get_sample(id).txt->get_id(), *ctx.occ, obj);

        dst->get_sample(id).txt = obj->as<root::texture>();
    }

    return result_code::ok;
}

result_code
texture_load_derive(reflection::property_load_derive_context& ctx)
{
    auto src = ctx.dst_obj->as<root::material>();

    auto& sc = *ctx.sc;

    auto item = sc[ctx.src_property->name];

    const auto id = AID(ctx.src_property->name);
    const auto texture_id = AID(item["texture"].as<std::string>());

    root::smart_object* obj = nullptr;
    auto rc = core::object_constructor::object_load_internal(texture_id, *ctx.occ, obj);

    if (rc != result_code::ok)
    {
        ALOG_ERROR("Texture doesn't exist");
        return rc;
    }

    auto& s = obj->get_flags();

    const auto slot = item["slot"].as<uint32_t>();

    auto& sample = src->get_sample(id);
    sample.txt = obj->as<root::texture>();
    sample.sampler_id = AID(item["sampler"].as<std::string>());
    sample.slot = slot;

    return result_code::ok;
}

}  // namespace agea::root