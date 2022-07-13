#pragma once

#include "engine/ui.h"

#include "engine/console.h"
#include "engine/property_drawers.h"

#include "native/native_window.h"

#include "model/level.h"
#include "model/game_object.h"

#include "model/rendering/material.h"
#include "model/caches/materials_cache.h"

#include "model/reflection/property.h"

#include "vulkan_render_types/vulkan_texture_data.h"
#include "vulkan_render/render_loader.h"
#include "vulkan_render/render_device.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

#include <array>

namespace agea
{
namespace ui
{
struct selection_context
{
    ImGuiTreeNodeFlags base_flags;
    int i = 0;
    int selected = -1;
};

ui::ui()
{
    property_drawers::init();

    m_winodws[level_editor_window::window_title()] = std::make_unique<level_editor_window>();
    m_winodws[level_editor_window::window_title()]->m_show = true;

    m_winodws[materials_selector::window_title()] = std::make_unique<materials_selector>();
    m_winodws[object_editor::window_title()] = std::make_unique<object_editor>();
    m_winodws[components_editor::window_title()] = std::make_unique<components_editor>();
    m_winodws[editor_console::window_title()] = std::make_unique<editor_console>();
    m_winodws[editor_console::window_title()]->m_show = true;
}

ui::~ui()
{
}

void
ui::new_frame()
{
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(::agea::glob::native_window::get()->handle());

    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    for (auto& w : m_winodws)
    {
        w.second->handle();
    }
}

void
ui::draw(VkCommandBuffer cmd)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void
window::handle()
{
    ImGui::Begin(m_str.data(), &m_show, ImGuiWindowFlags_MenuBar);

    ImGui::End();
}

bool
window::handle_begin(int flags)
{
    if (!m_show)
    {
        return false;
    }

    ImGui::Begin(m_str.data(), &m_show, flags);

    return true;
}

void
window::handle_end()
{
    ImGui::End();
}

void
level_editor_window::handle()
{
    if (!handle_begin(ImGuiWindowFlags_MenuBar))
    {
        return;
    }

    auto level = ::agea::glob::level::get();

    if (ImGui::TreeNode("Level Objects"))
    {
        for (auto& o : level->get_game_objects().get_items())
        {
            if (auto game_obj = o.second->as<model::game_object>())
            {
                draw_oject(game_obj);
            }
        }
        ImGui::TreePop();
    }
    handle_end();
}

void
level_editor_window::draw_oject(model::game_object* obj)
{
    bool opened = ImGui::TreeNode(obj->get_id().cstr());
    if (ImGui::IsItemClicked())
    {
        get_window<object_editor>()->show(obj);
    }
    if (opened)
    {
        ImGui::TreePop();
    }
}

void
materials_selector::handle()
{
    if (!handle_begin())
    {
        return;
    }

    ImGui::Text("Select material:");
    ImGui::InputText("##material", m_filtering_text.data(), m_filtering_text.size(), 0);
    ImGui::Separator();

    glob::materials_cache::get()->call_on_items(
        [this](model::material* m)
        {
            {
                auto mat_id = m->get_id().str();
                if (mat_id.find(m_filtering_text.data()) == std::string::npos)
                {
                    return true;
                }

                auto open = ImGui::TreeNodeEx(mat_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (open)
                {
                    ImGui::Separator();
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(-1, 120);
                    ImGui::Text("Preview");
                    if (ImGui::Button("Use"))
                    {
                        m_selection_cb(mat_id);
                    }
                    ImGui::NextColumn();
                    ImGui::Image(m->get_material_data()->texture_set, ImVec2{160, 160});

                    ImGui::Columns(1);

                    ImGui::Separator();
                    ImGui::TreePop();
                }
            }
            return true;
        });

    handle_end();
}

void
components_editor::handle()
{
    if (!handle_begin(ImGuiWindowFlags_NoFocusOnAppearing))
    {
        return;
    }

    ImGui::Columns(2);
    ImGui::Separator();
    auto& list = m_obj->reflection()->m_editor_properties;

    for (auto& categories : list)
    {
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::Text("%s", categories.first.c_str());
        ImGui::Separator();
        ImGui::Columns(2);

        for (auto& p : categories.second)
        {
            ImGui::Text("%s", p->name.data());
            ImGui::NextColumn();
            if (p->name == "material_id")
            {
                if (ImGui::Button("e"))
                {
                    auto handler = [&p, this](const std::string& selected)
                    {
                        auto str = (std::string*)((char*)m_obj + p->offset);

                        (*str) = selected;

                        m_obj->mark_dirty();
                    };

                    get_window<materials_selector>()->show(handler);
                }
                ImGui::SameLine();
            }
            property_drawers::draw_ro(m_obj, *p);
            ImGui::NextColumn();
        }
    }

    handle_end();
}

void
object_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    ImGui::Columns(2);
    ImGui::Separator();

    for (auto& categories : m_obj->reflection()->m_editor_properties)
    {
        ImGui::Separator();
        ImGui::Text("%s", categories.first.c_str());
        ImGui::Separator();
        for (auto& p : categories.second)
        {
            ImGui::Text("%s", p->name.data());
            ImGui::NextColumn();
            property_drawers::draw_ro(m_obj, *p);
            ImGui::NextColumn();
        }
    }

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::Text("Components :");
    ImGui::Separator();
    ImGui::Columns(2);

    static selection_context sc;
    sc.i = 0;
    sc.base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
    draw_components(m_obj->get_root_component(), sc);

    ImGui::Separator();

    handle_end();
}

void
object_editor::draw_components(model::game_object_component* root, selection_context& sc)
{
    ImGuiTreeNodeFlags node_flags = sc.base_flags;
    const bool is_selected = sc.i == sc.selected;
    if (is_selected)
    {
        node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (root->get_attached_components().empty())
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
        if (!root->get_attached_components().empty())
        {
            for (auto obj : root->get_attached_components())
            {
                draw_components((model::game_object_component*)obj, sc);
            }
        }
        ImGui::TreePop();
    }
}

}  // namespace ui
}  // namespace agea