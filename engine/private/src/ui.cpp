#pragma once

#include "engine/ui.h"
#include "engine/script_editor.h"

#include "engine/console.h"
#include "engine/property_drawers.h"
#include "engine/engine_counters.h"

#include "engine/private/ui/package_editor.h"
#include "engine/private/ui/object_editor.h"

#include <core/level.h>
#include <core/caches/caches_map.h>
#include <core/caches/materials_cache.h>
#include <core/reflection/property.h>
#include <core/reflection/lua_api.h>

#include <packages/root/game_object.h>
#include <packages/root/assets/material.h>

#include <core/package.h>
#include <core/package_manager.h>

#include <native/native_window.h>

#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/vulkan_render_device.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>

#include <array>

namespace
{

}  // namespace

namespace agea
{

glob::ui::type glob::ui::type::s_instance;

namespace ui
{

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

    m_winodws[package_editor::window_title()] = std::make_unique<package_editor>();
    m_winodws[package_editor::window_title()]->m_show = true;

    m_winodws[performance_counters_window::window_title()] =
        std::make_unique<performance_counters_window>();
    m_winodws[performance_counters_window::window_title()]->m_show = true;
}

ui::~ui()
{
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
}

void
ui::init()
{
    // Init ImGui
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForSDLRenderer(glob::native_window::getr().handle(), nullptr);

    // Color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
}

void
ui::new_frame(float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    auto s = glob::native_window::getr().get_size();

    io.DisplaySize = ImVec2((float)s.w, (float)s.h);
    io.DeltaTime = dt;

    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();

    for (auto& w : m_winodws)
    {
        w.second->handle();
    }

    ImGui::Render();
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
            if (auto game_obj = o.second->as<root::game_object>())
            {
                draw_oject(game_obj);
            }
        }
        ImGui::TreePop();
    }
    handle_end();
}

void
level_editor_window::draw_oject(root::game_object* obj)
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
        [this](root::material* m)
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
                    // ImGui::Image(m->get_material_data()->texture_set, ImVec2{160, 160});

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
    auto& list = m_obj->get_reflection()->m_editor_properties;

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

                        // m_obj->mark_dirty();
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
performance_counters_window::handle()
{
    if (!lock)
    {
        frame_avg = glob::engine_counters::getr().frame.avg / 1000;
        fps = 1000000.0 / glob::engine_counters::getr().frame.avg;
        input_avg = glob::engine_counters::getr().input.avg / 1000;
        tick_avg = glob::engine_counters::getr().tick.avg / 1000;
        ui_tick_avg = glob::engine_counters::getr().ui_tick.avg / 1000;
        consume_updates_avg = glob::engine_counters::getr().consume_updates.avg / 1000;
        draw_avg = glob::engine_counters::getr().draw.avg / 1000;
        lock = 30;
    }

    ImGui::Separator();
    ImGui::Text("Frame   : %3.3lf", frame_avg);
    ImGui::Text("FPS     : %3.3lf", fps);
    ImGui::Text("Input   : %3.3lf", input_avg);
    ImGui::Separator();
    ImGui::Text("Tick    : %3.3lf", tick_avg);
    ImGui::Text("UI tick : %3.3lf", ui_tick_avg);
    ImGui::Text("Update  : %3.3lf", consume_updates_avg);
    ImGui::Separator();
    ImGui::Text("Draw    : %3.3lf", draw_avg);

    --lock;
}

}  // namespace ui
}  // namespace agea