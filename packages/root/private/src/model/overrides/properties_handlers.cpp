#include "packages/root/model/overrides/properties_handlers.h"

#include "core/reflection/property.h"
#include "core/reflection/reflection_type_utils.h"

#include <global_state/global_state.h>
#include "core/object_load_context.h"
#include "core/caches/cache_set.h"
#include "core/object_constructor.h"
#include "core/id_generator.h"

#include "packages/root/model/smart_object.h"
#include "packages/root/model/components/component.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/sampler.h"

#include <serialization/serialization.h>

#include <utils/kryga_log.h>
#include <error_handling/error_handling.h>

namespace kryga::root
{

result_code
game_object_components_prototype(::kryga::reflection::property_context__prototype& ctx)
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

    auto& src_properties = ::kryga::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_obj->as_blob() + ctx.dst_property->offset);

    if (src_properties.empty())
    {
        return result_code::ok;
    }

    auto& dst_properties = ::kryga::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    KRG_check(dst_properties.empty(), "Should always be empty!!");
    KRG_check(number_of_components == src_properties.size(), "Should always be same!!");

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
game_object_components_save(::kryga::reflection::property_context__save& dc)
{
    auto& class_obj = *dc.obj;
    auto& container = *dc.sc;

    serialization::container components_container;
    serialization::container components_layout;
    components_layout.SetStyle(YAML::EmitterStyle::Flow);

    auto& obj_components = ::kryga::reflection::utils::as_type<std::vector<root::component*>>(
        class_obj.as_blob() + dc.p->offset);

    // KRG_check(obj_components.size() == parent_components.size(), "Should be same size!");
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
game_object_components_compare(::kryga::reflection::property_context__compare&)
{
    // Always different because of IDS
    return result_code::failed;
}

result_code
game_object_components_copy(::kryga::reflection::property_context__copy& ctx)
{
    //     KRG_check(ctx.occ->get_construction_type() ==
    //                    model::object_constructor_context::construction_type::mirror_obj,
    //                "Should always be empty!!");

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
        auto result = core::object_constructor::object_instantiate_internal(
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
game_object_components__load(::kryga::reflection::property_context__load& ctx)
{
    KRG_check(ctx.dst_property->name == "components", "Only components expected");

    auto& sc = *ctx.sc;

    auto components = sc[ctx.dst_property->name];
    auto layout = sc["layout"];

    if ((!components) != (!layout))
    {
        ALOG_ERROR("Both layout and components should exist/not exist");
        return result_code::failed;
    }

    auto& src_components = ::kryga::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_obj->as_blob() + ctx.dst_property->offset);

    auto& dst_components = ::kryga::reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    KRG_check(dst_components.empty(), "Should always be empty!!");

    if (components)
    {
        auto components_size = components.size();
        auto layout_size = layout.size();

        if (components_size != layout_size)
        {
            ALOG_ERROR("Misconfigured layout");
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
                auto comp_id = item["id"].IsDefined() ? item["id"].as<std::string>() : "unknown";
                auto comp_class = item["class_id"].IsDefined() ? item["class_id"].as<std::string>()
                                                               : "unknown";
                ALOG_ERROR("Failed to load component [{}] (class [{}]) at index [{}] for object [{}]",
                           comp_id, comp_class, i, ctx.dst_obj->get_id().cstr());
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
            KRG_check(!c->get_flags().instance_obj, "Only protos are allowed");

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
texture_slot_prototype(::kryga::reflection::property_context__prototype& dc)
{
    return result_code::ok;
}

result_code
property_texture_slot__save(::kryga::reflection::property_context__save& dc)
{
    return result_code::ok;
}

result_code
property_texture_slot__compare(::kryga::reflection::property_context__compare& ctx)
{
    return result_code::ok;
}

result_code
property_texture_slot__copy(::kryga::reflection::property_context__copy& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);
    auto& src_slot = src->get_slot(id);

    dst->set_slot(id, src_slot);

    // Clone texture
    if (src_slot.txt)
    {
        auto result = core::object_constructor::object_clone_create_internal(
            src_slot.txt->get_id(), src_slot.txt->get_id(), *ctx.occ);

        if (!result)
        {
            return result.error();
        }

        dst->get_slot(id).txt = result.value()->as<root::texture>();
    }

    // Samplers are shared assets, no need to clone - just copy the pointer
    dst->get_slot(id).smp = src_slot.smp;

    return result_code::ok;
}

result_code
property_texture_slot__instantiate(::kryga::reflection::property_context__instantiate& ctx)
{
    auto src = ctx.src_obj->as<root::material>();
    auto dst = ctx.dst_obj->as<root::material>();

    auto id = AID(ctx.src_property->name);

    auto& src_slot = src->get_slot(id);

    dst->set_slot(id, src_slot);

    // Instantiate texture
    if (src_slot.txt)
    {
        root::smart_object* obj = ctx.occ->find_obj(src_slot.txt->get_id());

        if (!obj)
        {
            std::vector<smart_object*> objs;
            auto result = core::object_constructor::object_instantiate(
                *src_slot.txt, src_slot.txt->get_id(), *ctx.occ, objs);
            if (!result)
            {
                return result.error();
            }
            obj = result.value();
        }

        dst->get_slot(id).txt = obj->as<root::texture>();
    }

    // Samplers are shared assets - just copy the pointer
    dst->get_slot(id).smp = src_slot.smp;

    return result_code::ok;
}

result_code
property_texture_slot__load(reflection::property_context__load& ctx)
{
    auto src = ctx.dst_obj->as<root::material>();

    auto& sc = *ctx.sc;

    auto item = sc[ctx.src_property->name];

    const auto id = AID(ctx.src_property->name);
    const auto texture_id = AID(item["texture"].as<std::string>());

    // Load texture
    auto tex_result = core::object_constructor::object_load_internal(texture_id, *ctx.occ);
    if (!tex_result)
    {
        ALOG_ERROR("Texture doesn't exist: {}", texture_id.str());
        return tex_result.error();
    }

    auto tex_obj = tex_result.value();

    const auto slot = item["slot"].as<uint32_t>();

    auto& tex_slot = src->get_slot(id);
    tex_slot.txt = tex_obj->as<root::texture>();
    tex_slot.slot = slot;

    // Load sampler (optional - may not exist in older files)
    if (item["sampler"] && item["sampler"].IsScalar())
    {
        const auto sampler_id = AID(item["sampler"].as<std::string>());
        auto smp_result = core::object_constructor::object_load_internal(sampler_id, *ctx.occ);
        if (smp_result)
        {
            tex_slot.smp = smp_result.value()->as<root::sampler>();
        }
        else
        {
            ALOG_WARN("Sampler not found: {}, using default", sampler_id.str());
            tex_slot.smp = nullptr;
        }
    }
    else
    {
        tex_slot.smp = nullptr;
    }

    return result_code::ok;
}

}  // namespace kryga::root