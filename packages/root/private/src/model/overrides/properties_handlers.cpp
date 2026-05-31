#include "packages/root/model/overrides/properties_handlers.h"

#include "core/object_layer_flags.h"
#include "core/reflection/property.h"
#include "core/reflection/reflection_type_utils.h"

#include <global_state/global_state.h>
#include <core/model_system.h>
#include "core/object_load_context.h"
#include "core/caches/cache_set.h"
#include "core/object_constructor.h"
#include "core/id_generator.h"

#include "packages/root/model/smart_object.h"
#include "packages/root/model/components/component.h"
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

        auto result = ctx.ctor->load_sub_object(item);

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
        component_container["proto_id"] = pid;

        reflection::property_context__save internal_sc{
            nullptr, obj_component, &component_container};
        std::vector<reflection::property*> diff;

        if (auto rc =
                core::diff_object_properties(*obj_component->get_class_obj(), *obj_component, diff);
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
    auto& gen = glob::glob_state().getr_model().id_gen;

    for (int i = 0; i < src_col.size(); ++i)
    {
        auto result = ctx.ctor->clone_obj(*src_col[i], gen.generate(src_col[i]->get_id()));

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
game_object_components_snapshot(::kryga::reflection::property_context__copy& ctx)
{
    // Play-mode snapshot/restore of the component layout. Unlike copy/instantiate,
    // this is IDENTITY-PRESERVING: it does NOT clone the components — it just
    // records (and on restore re-applies) the ordered vector of the SAME component
    // pointers, so each component lands back at its proper position. Component
    // *values* are snapshotted per-component (each is its own level object); this
    // handler only remembers which components belong and in what order.
    KRG_check(ctx.src_property == ctx.dst_property, "Should be SAME properties!");
    KRG_check(ctx.src_obj != ctx.dst_obj, "Should not be SAME objects!");

    auto& src_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.src_property->get_blob(*ctx.src_obj));
    auto& dst_col = reflection::utils::as_type<std::vector<root::component*>>(
        ctx.dst_property->get_blob(*ctx.dst_obj));

    dst_col = src_col;

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

    auto& id_gen = glob::glob_state().getr_model().id_gen;

    for (int i = 0; i < src_col.size(); ++i)
    {
        auto inst_id = id_gen.generate(src_col[i]->get_id());
        auto result = ctx.ctor->instantiate_obj(*src_col[i], inst_id);

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

            if (item["id"].IsDefined())
            {
                auto comp_id = AID(item["id"].as<std::string>());
                if (glob::glob_state().getr_model().caches.objects.get_item(comp_id))
                {
                    ALOG_ERROR(
                        "Component [{}] already exists at index [{}] — "
                        "inline component conflicts with cached object",
                        item["id"].as<std::string>(),
                        i);
                    return result_code::failed;
                }
            }

            auto result = ctx.ctor->load_sub_object(item);

            if (!result || !result.value() ||
                result.value()->get_architype_id() != core::architype::component)
            {
                auto comp_id = item["id"].IsDefined() ? item["id"].as<std::string>() : "unknown";
                auto comp_class =
                    item["proto_id"].IsDefined() ? item["proto_id"].as<std::string>() : "unknown";
                ALOG_ERROR(
                    "Failed to load component [{}] (class [{}]) at index [{}] for object [{}]",
                    comp_id,
                    comp_class,
                    i,
                    ctx.dst_obj->get_id().cstr());
                return result_code::failed;
            }

            dst_components[i] = result.value()->as<root::component>();

            auto* comp_class = dst_components[i]->get_class_obj();
            if (comp_class && comp_class->get_flags().instance_obj)
            {
                auto comp_id = item["id"].IsDefined() ? item["id"].as<std::string>() : "unknown";
                ALOG_ERROR("Component [{}] type_id references instance [{}] at index [{}]",
                           comp_id,
                           comp_class->get_id().cstr(),
                           i);
                return result_code::failed;
            }

            dst_components[i]->set_order_parent_idx(i, layout[i].as<int>());
        }
    }
    else
    {
        for (auto c : src_components)
        {
            KRG_check(!c->get_flags().instance_obj, "Only protos are allowed");

            auto gid = glob::glob_state().getr_model().id_gen.generate(c->get_id());

            auto result = ctx.ctor->clone_obj(*c, gid);
            if (!result)
            {
                return result_code::fallback;
            }

            auto comp = result.value()->as<root::component>();
            comp->set_order_parent_idx(c->get_order_idx(), c->get_parent_idx());
            dst_components.push_back(comp);
        }
    }

    return result_code::ok;
}

result_code
property_layer_mask__save(::kryga::reflection::property_context__save& dc)
{
    auto& mask = ::kryga::reflection::utils::as_type<core::object_layer_flags>(dc.obj->as_blob() +
                                                                               dc.p->offset);

    serialization::container c;
    c["visible"] = (bool)mask.visible;
    c["editor_only"] = (bool)mask.editor_only;
    c["cast_shadows"] = (bool)mask.cast_shadows;
    c["receive_light"] = (bool)mask.receive_light;
    c["contribute_gi"] = (bool)mask.contribute_gi;
    c["static_object"] = (bool)mask.static_object;

    (*dc.sc)[dc.p->name] = c;

    return result_code::ok;
}

result_code
property_layer_mask__compare(::kryga::reflection::property_context__compare& ctx)
{
    auto& src = ::kryga::reflection::utils::as_type<core::object_layer_flags>(
        ctx.src_obj->as_blob() + ctx.p->offset);
    auto& dst = ::kryga::reflection::utils::as_type<core::object_layer_flags>(
        ctx.dst_obj->as_blob() + ctx.p->offset);

    return src.bits == dst.bits ? result_code::ok : result_code::failed;
}

result_code
property_layer_mask__load(::kryga::reflection::property_context__load& ctx)
{
    auto& sc = *ctx.sc;
    auto node = sc[ctx.dst_property->name];

    auto& mask = ::kryga::reflection::utils::as_type<core::object_layer_flags>(
        ctx.dst_obj->as_blob() + ctx.dst_property->offset);

    if (node && node.IsMap())
    {
        mask.visible = node["visible"].as<bool>(true);
        mask.editor_only = node["editor_only"].as<bool>(false);
        mask.cast_shadows = node["cast_shadows"].as<bool>(true);
        mask.receive_light = node["receive_light"].as<bool>(true);
        mask.contribute_gi = node["contribute_gi"].as<bool>(true);
        mask.static_object = node["static_object"].as<bool>(true);
    }

    return result_code::ok;
}

}  // namespace kryga::root