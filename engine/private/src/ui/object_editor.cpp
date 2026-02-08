#include "engine/private/ui/object_editor.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/components/game_object_component.h>
#include <packages/root/model/assets/material.h>
#include <engine/property_drawers.h>
#include <glue/type_ids.ar.h>

#include <core/reflection/reflection_type.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/reflection/types.h>
#include <vulkan_render/vulkan_render_loader.h>

namespace kryga::ui
{

namespace
{

template <typename T>
static result_code
read_value(reflection::property& p, root::smart_object& obj, T& value)
{
    // KRG_check(p.rtype->type_id == reflection::kryga_type_resolve<T>().type_id, "Wrong type");

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
draw_transformation(int& uid, root::game_object_component& obj)
{
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    auto apply = [&obj]()
    {
        obj.update_matrix();
        obj.update_children_matrixes();
        obj.mark_transform_dirty();
    };

    root::vec3 pos = obj.get_position();
    root::vec3 rot = obj.get_rotation();
    root::vec3 scl = obj.get_scale();

    ImGui::PushID(uid++);
    ImGui::Text("Position");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##pos", &pos.x, 0.1f))
    {
        obj.set_position(pos);
        apply();
    }
    ImGui::PopID();

    ImGui::PushID(uid++);
    ImGui::Text("Rotation");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##rot", &rot.x, 0.5f))
    {
        obj.set_rotation(rot);
        apply();
    }
    ImGui::PopID();

    ImGui::PushID(uid++);
    ImGui::Text("Scale");
    ImGui::SameLine(100);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat3("##scl", &scl.x, 0.01f))
    {
        obj.set_scale(scl);
        apply();
    }
    ImGui::PopID();
}

void
object_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    auto f = glob::vulkan_render_loader::getr().get_font(AID("big"));

    ImGui::PushFont(f);
    ImGui::Text("%s", current_object->get_id().cstr());
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    if (ImGui::CollapsingHeader("Hierarchy"))
    {
        if (auto obj = current_object->as<root::game_object>())
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

    auto& categories = current_object->get_reflection()->m_editor_properties;
    int uid = 0;

    // Draw transform for game_object (via root component) or for direct component selection
    if (auto go = current_object->as<root::game_object>())
    {
        if (go->get_root_component())
        {
            draw_transformation(uid, *go->get_root_component());
        }
    }
    else if (auto comp = current_object->as<root::game_object_component>())
    {
        draw_transformation(uid, *comp);
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

                if (type_id == ::kryga::root__vec3)
                {
                    ImGui::Text("%s", p->name.c_str());

                    ImGui::NextColumn();

                    root::vec3 v3;
                    read_value<root::vec3>(*p, *current_object, v3);
                    draw_v3(uid, v3);
                }
                else
                {
                    reflection::property_context__to_string pts;
                    pts.obj = current_object;
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

    if (root->get_render_children().empty())
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
        for (auto child : root->get_render_children())
        {
            draw_components(child, sc);
        }
        ImGui::TreePop();
    }
}

}  // namespace kryga::ui