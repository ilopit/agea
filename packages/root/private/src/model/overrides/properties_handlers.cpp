#include "packages/root/model/overrides/properties_handlers.h"

#include "core/reflection/property.h"
#include "core/reflection/reflection_type_utils.h"

#include <global_state/global_state.h>
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
game_object_components_prototype(::agea::reflection::property_context__prototype& ctx)
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

        auto result = core::object_constructor::object_load_internal(item, *ctx.occ);

        if (!result || !result.value() ||
            result.value()->get_architype_id() != core::architype::component)
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        dst_properties[i] = result.value()->as<root::component>();
        dst_properties[i]->set_order_parent_idx(i, layout_mapping[i]);
    }
    return result_code::ok;
}

result_code
game_object_components_save(::agea::reflection::property_context__save& dc)
{
    auto& class_obj = *dc.obj;
    auto& container = *dc.sc;

    serialization::container components_container;
    serialization::container components_layout;
    components_layout.SetStyle(YAML::EmitterStyle::Flow);

    auto& obj_components = ::agea::reflection::utils::as_type<std::vector<root::component*>>(
        class_obj.as_blob() + dc.p->offset);

    // AGEA_check(obj_components.size() == parent_components.size(), "Should be same size!");
    auto size = obj_components.size();
    for (size_t i = 0; i < size; ++i)
    {
        // auto class_component = parent_components[i];
        auto obj_component = obj_components[i];

        serialization::container component_container;

        auto id = obj_component->get_id().str();
        component_container["id"] = id;

        auto pid = obj_component->get_class_obj()->get_id().str();
        component_container["class_id"] = pid;

        reflection::property_context__save internal_sc{nullptr, obj_component,
                                                       &component_container};
        std::vector<reflection::property*> diff;

        if (auto rc = core::object_constructor::diff_object_properties(
                *obj_component->get_class_obj(), *obj_component, diff);
            rc != result_code::ok)
        {
            return rc;
        }

        for (auto& p : diff)
        {
            internal_sc.p = p;
            if (auto rc = p->save_handler(internal_sc); rc != result_code::ok)
            {
                return rc;
            }
        }

        components_container[i] = component_container;
        components_layout.push_back((int)obj_component->get_parent_idx());
    }
    container["layout"] = components_layout;
    container[dc.p->name] = components_container;

    return result_code::ok;
}

result_code
game_object_components_compare(::agea::reflection::property_context__compare&)
{
    // Always different because of IDS
    return result_code::failed;
}

result_code
game_object_components_copy(::agea::reflection::property_context__copy& ctx)
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
        auto result = core::object_constructor::object_clone_create_internal(
            *src_col[i], gen->generate(src_col[i]->get_id()), *ctx.occ);

        if (!result)
        {
            return result.error();
        }

        auto comp = result.value()->as<root::component>();
        comp->set_order_parent_idx(src_col[i]->get_order_idx(), src_col[i]->get_parent_idx());

        dst_col[i] = comp;
    }

    return result_code::ok;
}

result_code
game_object_components_instantiate(reflection::property_context__instantiate& ctx)
{
    auto& src_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_property->get_blob(*ctx.src_obj));
    auto& dst_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_property->get_blob(*ctx.dst_obj));

    dst_col.resize(src_col.size());

    for (int i = 0; i < src_col.size(); ++i)
    {
        auto result = core::object_constructor::object_instanciate_internal(
            *src_col[i], src_col[i]->get_id(), *ctx.occ);

        if (!result)
        {
            return result.error();
        }

        auto comp = result.value()->as<root::component>();
        comp->set_order_parent_idx(src_col[i]->get_order_idx(), src_col[i]->get_parent_idx());

        dst_col[i] = comp;
    }

    return result_code::ok;
}

result_code
game_object_components__load(::agea::reflection::property_context__load& ctx)
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

            auto result = core::object_constructor::object_load_internal(item, *ctx.occ);

            if (!result || !result.value() ||
                result.value()->get_architype_id() != core::architype::component)
            {
                ALOG_LAZY_ERROR;
                return result_code::failed;
            }

            dst_components[i] = result.value()->as<root::component>();
            dst_components[i]->set_order_parent_idx(i, layout[i].as<int>());
        }
    }
    else
    {
        for (auto c : src_components)
        {
            AGEA_check(!c->get_flags().instance_obj, "Only protos are alowed");

            auto gid = glob::glob_state().get_id_generator()->generate(c->get_id());

            auto result = core::object_constructor::object_clone_create_internal(*c, gid, *ctx.occ);
            if (!result)
            {
                return result_code::fallback;
            }

            dst_components.push_back(result.value()->as<root::component>());
        }
    }

    return result_code::ok;
}

result_code
texture_sample_prototype(::agea::reflection::property_context__prototype& dc)
{
    return result_code::ok;
}

result_code
property_texture_sample__save(::agea::reflection::property_context__save& dc)
{
    return result_code::ok;
}

result_code
property_texture_sample__compare(::agea::reflection::property_context__compare& ctx)
{
    return result_code::ok;
}

result_code
property_texture_sample__copy(::agea::reflection::property_context__copy& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);

    dst->set_sample(id, src->get_sample(id));

    auto result = core::object_constructor::object_clone_create_internal(
        src->get_sample(id).txt->get_id(), src->get_sample(id).txt->get_id(), *ctx.occ);

    if (!result)
    {
        return result.error();
    }

    dst->get_sample(id).txt = result.value()->as<root::texture>();

    return result_code::ok;
}

result_code
property_texture_sample__instantiate(::agea::reflection::property_context__instantiate& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);

    auto& src_sample = src->get_sample(id);

    dst->set_sample(id, src_sample);

    root::smart_object* obj = ctx.occ->find_obj(src_sample.txt->get_id());

    if (!obj)
    {
        std::vector<smart_object*> objs;
        auto result = core::object_constructor::object_instantiate(
            *src_sample.txt, src_sample.txt->get_id(), *ctx.occ, objs);
        if (!result)
        {
            return result.error();
        }
        obj = result.value();
    }

    dst->get_sample(id).txt = obj->as<root::texture>();

    return result_code::ok;
}

result_code
property_texture_sample__load(reflection::property_context__load& ctx)
{
    auto src = ctx.dst_obj->as<root::material>();

    auto& sc = *ctx.sc;

    auto item = sc[ctx.src_property->name];

    const auto id = AID(ctx.src_property->name);
    const auto texture_id = AID(item["texture"].as<std::string>());

    auto result = core::object_constructor::object_load_internal(texture_id, *ctx.occ);

    if (!result)
    {
        ALOG_ERROR("Texture doesn't exist");
        return result.error();
    }

    auto obj = result.value();

    auto& s = obj->get_flags();

    const auto slot = item["slot"].as<uint32_t>();

    auto& sample = src->get_sample(id);
    sample.txt = obj->as<root::texture>();
    sample.sampler_id = AID(item["sampler"].as<std::string>());
    sample.slot = slot;

    return result_code::ok;
}

}  // namespace agea::root