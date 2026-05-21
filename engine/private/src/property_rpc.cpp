#include "engine/private/property_rpc.h"

#include <core/level.h>
#include <core/model_system.h>
#include <core/reflection/property.h>
#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>

#include <global_state/global_state.h>

#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_system.h>
#include <vulkan_render/render_cache.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>

#include <packages/root/model/smart_object.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/components/component.h>
#include <packages/root/model/components/game_object_component.h>

#include <utils/id.h>
#include <utils/kryga_log.h>

namespace kryga::engine_private
{

Json::Value
encode_owner(root::smart_object& obj)
{
    Json::Value owner(Json::objectValue);
    owner["id"] = obj.get_id().str();

    auto* rt = obj.get_reflection();
    owner["type"] = rt ? rt->type_name.str() : std::string();

    Json::Value cats(Json::arrayValue);
    if (!rt)
    {
        owner["categories"] = cats;
        return owner;
    }

    for (const auto& [cat_name, props] : rt->m_editor_properties)
    {
        // Skip Meta — internal bookkeeping.
        if (cat_name == "Meta")
        {
            continue;
        }

        Json::Value cat(Json::objectValue);
        cat["name"] = cat_name;

        Json::Value fields(Json::arrayValue);
        for (auto& p : props)
        {
            if (!p->rtype)
            {
                continue;
            }
            Json::Value field(Json::objectValue);
            field["name"] = p->name;
            // `kind` is the registered type name. The webview matches known
            // kinds (bool, float, vec3, …) for specialized widgets and
            // falls back to readonly for the rest. Adding a new package =
            // new type names appear with a generic editor; ship custom
            // widgets later if needed.
            field["kind"] = p->rtype->type_name.str();

            if (p->type.is_collection)
            {
                field["readonly"] = true;
                field["value"] = std::string("(collection)");
                fields.append(field);
                continue;
            }

            Json::Value val;
            reflection::property_context__json_get get_ctx{p.get(), &obj, &val};
            if (p->json_get(get_ctx) == result_code::ok)
            {
                field["value"] = val;
                if (!p->rtype->json_load || !p->serializable)
                {
                    field["readonly"] = true;
                }
            }
            else
            {
                field["readonly"] = true;
                field["value"] = std::string();
            }

            if (field["value"].isString() && field["value"].asString().empty() && p->rtype)
            {
                auto& rcache = glob::glob_state().getr_render().renderer.get_cache();
                if (auto* robj = rcache.objects.find_by_id(obj.get_id()))
                {
                    auto tn = p->rtype->type_name.str();
                    if (tn == "material" && robj->material)
                        field["value"] = robj->material->get_id().str();
                    else if (tn == "mesh" && robj->mesh)
                        field["value"] = robj->mesh->get_id().str();
                }
            }
            fields.append(field);
        }

        cat["fields"] = fields;
        cats.append(cat);
    }

    owner["categories"] = cats;

    auto& cache = glob::glob_state().getr_render().renderer.get_cache();
    auto* robj = cache.objects.find_by_id(obj.get_id());
    if (robj && robj->material)
    {
        owner["material_id"] = robj->material->get_id().str();
    }

    return owner;
}

namespace
{

void
walk_components(root::game_object_component* root_comp,
                std::vector<root::game_object_component*>& out)
{
    if (!root_comp)
    {
        return;
    }
    out.push_back(root_comp);
    for (auto* child : root_comp->get_render_children())
    {
        walk_components(child, out);
    }
}

}  // namespace

Json::Value
encode_game_object_properties(root::game_object& go)
{
    Json::Value res(Json::objectValue);
    res["id"] = go.get_id().str();

    Json::Value owners(Json::arrayValue);
    owners.append(encode_owner(go));

    std::vector<root::game_object_component*> comps;
    walk_components(go.get_root_component(), comps);
    for (auto* c : comps)
    {
        owners.append(encode_owner(*c));
    }

    res["owners"] = owners;
    return res;
}

Json::Value
encode_component_properties(root::component& comp)
{
    auto* go = comp.get_owner();
    if (go)
    {
        Json::Value res = encode_game_object_properties(*go);
        res["selected"] = comp.get_id().str();
        return res;
    }

    Json::Value res(Json::objectValue);
    res["id"] = comp.get_id().str();
    res["selected"] = comp.get_id().str();

    Json::Value owners(Json::arrayValue);
    owners.append(encode_owner(comp));
    res["owners"] = owners;
    return res;
}

Json::Value
encode_smart_object_properties(root::smart_object& obj)
{
    Json::Value res(Json::objectValue);
    res["id"] = obj.get_id().str();
    res["selected"] = obj.get_id().str();

    Json::Value owners(Json::arrayValue);
    owners.append(encode_owner(obj));
    res["owners"] = owners;
    return res;
}

root::smart_object*
find_owner(const std::string& id_str)
{
    return glob::glob_state().getr_model().caches.objects.get_item(AID(id_str));
}

reflection::property*
find_editor_property(const reflection::reflection_type& rt, const std::string& name)
{
    for (auto& [cat_name, props] : rt.m_editor_properties)
    {
        for (auto& p : props)
        {
            if (p->name == name)
            {
                return p.get();
            }
        }
    }
    return nullptr;
}

std::string
write_property(root::smart_object& owner,
               const std::string& field_name,
               const Json::Value& value,
               Json::Value& out_value)
{
    if (owner.get_flags().readonly)
    {
        return "object is readonly";
    }

    auto* rt = owner.get_reflection();
    if (!rt)
    {
        return "owner has no reflection";
    }

    auto* prop = find_editor_property(*rt, field_name);
    if (!prop)
    {
        return "field not found: " + field_name;
    }
    if (!prop->rtype)
    {
        return "field is not editable: " + field_name;
    }

    reflection::property_context__json_set set_ctx{prop, &owner, &value};
    if (prop->json_set(set_ctx) != result_code::ok)
    {
        return "value did not match field type '" + prop->rtype->type_name.str() + "'";
    }

    Json::Value echo;
    reflection::property_context__json_get get_ctx{prop, &owner, &echo};
    if (prop->json_get(get_ctx) == result_code::ok)
    {
        out_value = echo;
    }
    else
    {
        out_value = Json::Value(Json::nullValue);
    }
    return std::string();
}

}  // namespace kryga::engine_private
