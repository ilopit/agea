#pragma once

#include "engine/private/ui/object_editor.h"

#include <packages/root/game_object.h>
#include <packages/root/assets/material.h>
#include <engine/property_drawers.h>
#include <packages/global/type_ids.ar.h>

#include <core/reflection/reflection_type.h>  //
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/types.h>
#include <vulkan_render/vulkan_render_loader.h>

namespace agea::ui
{

namespace
{

template <typename T>
static result_code
read_value(reflection::property& p, root::smart_object& obj, T& value)
{
    // AGEA_check(p.rtype->type_id == reflection::agea_type_resolve<T>().type_id, "Wrong type");

    value = reflection::utils::as_type<T>(p.get_blob(obj));

    return result_code::ok;
}
}  // namespace

void
draw_v4(const root::vec4& v4)
{
    ImGui::Text("%f", v4.x);
    ImGui::NextColumn();

    ImGui::Text("%f", v4.y);
    ImGui::NextColumn();

    ImGui::Text("%f", v4.z);
    ImGui::NextColumn();

    ImGui::Text("%f", v4.w);
    ImGui::NextColumn();
}

void
draw_v3(int& i, root::vec3& v3)
{
    ImGui::PushID(i++);
    ImGui::InputFloat("##id", &v3.x);
    ImGui::NextColumn();
    ImGui::PopID();

    ImGui::PushID(i++);
    ImGui::InputFloat("##id", &v3.y);
    ImGui::NextColumn();
    ImGui::PopID();

    ImGui::PushID(i++);
    ImGui::InputFloat("##id", &v3.z);
    ImGui::NextColumn();
    ImGui::PopID();
}

void
draw_transformation(const std::unordered_map<std::string, reflection::property_list>& editor,
                    int& uid,
                    root::game_object_component& obj)
{
    auto find = [](const std::string& n, const reflection::property_list& pl)
    {
        for (auto& p : pl)
        {
            if (p->name == n)
            {
                return p;
            }
        }
        return std::shared_ptr<reflection::property>();
    };

    if (!ImGui::CollapsingHeader("Transform"))
        return;

    ImGui::Columns(4);

    auto& pl = editor.at("Transform");
    ImGui::BeginDisabled();

    {
        ImGui::Text("position");
        ImGui::NextColumn();

        auto p = find("position", pl);
        root::vec3 v3;
        read_value<root::vec3>(*p, obj, v3);
        draw_v3(uid, v3);
    }
    {
        ImGui::Text("scale");
        ImGui::NextColumn();

        auto p = find("scale", pl);
        root::vec3 v3;
        read_value<root::vec3>(*p, obj, v3);
        draw_v3(uid, v3);
    }

    {
        ImGui::Text("rotation");
        ImGui::NextColumn();

        auto p = find("rotation", pl);
        root::vec3 v3;
        read_value<root::vec3>(*p, obj, v3);
        draw_v3(uid, v3);
    }
    ImGui::EndDisabled();
    ImGui::Columns(1);
}

void
object_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto rt = m_obj->get_reflection();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    auto f = glob::vulkan_render_loader::getr().get_font(AID("big"));

    ImGui::PushFont(f);
    ImGui::Text("%s", m_obj->get_id().cstr());
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    if (ImGui::CollapsingHeader("Hierarchy"))
    {
        if (auto obj = m_obj->as<root::game_object>())
        {
            if (obj->get_root_component())
            {
                static selection_context sc;
                sc.i = 0;
                sc.base_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                ImGuiTreeNodeFlags_SpanAvailWidth;

                draw_components(obj->get_root_component(), sc);
            }
        }
    }

    auto& categories = m_obj->get_reflection()->m_editor_properties;
    int uid = 0;
    if (auto obj = m_obj->as<root::game_object_component>())
    {
        draw_transformation(categories, uid, *obj);
    }

    for (auto& c : categories)
    {
        if (c.first == "Meta" || c.first == "Transform")
        {
            continue;
        }

        auto& properties = c.second;

        if (ImGui::CollapsingHeader(c.first.c_str()))
        {
            ImGui::Columns(4);

            for (auto& p : properties)
            {
                if (!p->rtype)
                {
                    continue;
                }

                auto type_id = p->rtype->type_id;

                if (type_id == ::agea::root__vec3)
                {
                    ImGui::Text("%s", p->name.c_str());

                    ImGui::NextColumn();

                    root::vec3 v3;
                    read_value<root::vec3>(*p, *m_obj, v3);
                    draw_v3(uid, v3);
                }
                else
                {
                    reflection::property_to_string_context pts;
                    pts.obj = m_obj;
                    pts.prop = p.get();
                    p->to_string_handler(pts);

                    ImGui::Text("%s", p->name.c_str());

                    ImGui::NextColumn();

                    ImGui::Text("%s", pts.result.data());

                    ImGui::NextColumn();
                    ImGui::NextColumn();
                    ImGui::NextColumn();
                }
            }

            ImGui::Columns(1);
        }
    }

    handle_end();
}

void
object_editor::draw_components(root::game_object_component* root, selection_context& sc)
{
    if (!root)
    {
        return;
    }

    ImGuiTreeNodeFlags node_flags = sc.base_flags;
    const bool is_selected = sc.i == sc.selected;
    if (is_selected)
    {
        node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (root->get_children().empty())
    {
        node_flags |= ImGuiTreeNodeFlags_Leaf;
    }

    auto open = ImGui::TreeNodeEx((void*)(intptr_t)sc.i, node_flags, "%s", root->get_id().cstr());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        sc.selected = sc.i;
        get_window<components_editor>()->show(root);
    }

    ++sc.i;
    ImGui::NextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", root->get_id().cstr());
    ImGui::NextColumn();

    if (open)
    {
        if (!root->get_children().empty())
        {
            for (auto obj : root->get_children())
            {
                draw_components((root::game_object_component*)obj, sc);
            }
        }
        ImGui::TreePop();
    }
}

}  // namespace agea::ui