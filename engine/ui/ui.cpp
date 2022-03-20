#pragma once

#include "ui/ui.h"

#include <inttypes.h>
#include <array>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

#include "native_window.h"
#include "model/level.h"
#include "reflection/property.h"

namespace agea
{
namespace ui
{

ui::ui()
{
    property_drawers::init();

    m_winodws.emplace_back(std::make_unique<editor_window>("EDITOR"));
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
        w->handle();
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
    m_window = true;
    ImGui::Begin(m_str.data(), &m_window, ImGuiWindowFlags_MenuBar);

    ImGui::End();
}

void
editor_window::handle()
{
    auto level = ::agea::glob::level::get();

    ImGui::Begin(m_str.data(), &m_window, ImGuiWindowFlags_MenuBar);
    if (ImGui::TreeNode("Level Objects"))
    {
        for (auto& o : level->m_objects)
        {
            draw_oject(o.get());
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

void
editor_window::draw_oject(model::level_object* obj)
{
    if (ImGui::TreeNode(obj->id().data()))
    {
        draw_oject_editor(obj);
        ImGui::TreePop();
    }
}

void
editor_window::draw_oject_editor(model::level_object* obj)
{
    ImGui::Begin("Object Editor");
    ImGui::Columns(2);
    ImGui::Separator();
    auto list = obj->reflection_table();
    while (list)
    {
        for (auto& p : list->properties)
        {
            ImGui::Text("%s", p.name.data());
            ImGui::NextColumn();
            property_drawers::drawers[(size_t)p.type]((char*)obj + p.ofsset);
            ImGui::NextColumn();
        }
        list = list->parent;
    }
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::Text("Components :");
    ImGui::Separator();
    ImGui::Columns(2);

    draw_components(obj->m_root_component);

    ImGui::Separator();
    ImGui::End();
}

void
editor_window::draw_components(model::level_object_component* root)
{
    auto open = ImGui::TreeNode(root->id().data());

    ImGui::NextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", root->class_id());
    ImGui::NextColumn();

    if (open)
    {
        for (auto obj : root->m_attached_components)
        {
            draw_components((model::level_object_component*)obj);
        }
        ImGui::TreePop();
    }
}

void
property_drawers::init()
{
    using namespace reflection;

    drawers[(size_t)supported_type::t_str] = property_drawers::draw_t_str;

    drawers[(size_t)supported_type::t_i8] = property_drawers::draw_t_i8;
    drawers[(size_t)supported_type::t_i16] = property_drawers::draw_t_i16;
    drawers[(size_t)supported_type::t_i32] = property_drawers::draw_t_i32;
    drawers[(size_t)supported_type::t_i64] = property_drawers::draw_t_i64;

    drawers[(size_t)supported_type::t_u8] = property_drawers::draw_t_u8;
    drawers[(size_t)supported_type::t_u16] = property_drawers::draw_t_u16;
    drawers[(size_t)supported_type::t_u32] = property_drawers::draw_t_u32;
    drawers[(size_t)supported_type::t_u64] = property_drawers::draw_t_u64;

    drawers[(size_t)supported_type::t_f] = property_drawers::draw_t_f;
    drawers[(size_t)supported_type::t_d] = property_drawers::draw_t_d;
}

void
property_drawers::draw_t_str(char* v)
{
    auto p = (std::string*)v;

    ImGui::Text("%s", p->data());
}

void
property_drawers::draw_t_i8(char* v)
{
    auto p = (std::int8_t*)v;
    ImGui::Text("%" PRIi8 "", *p);
}

void
property_drawers::draw_t_i16(char* v)
{
    auto p = (std::int16_t*)v;
    ImGui::Text("%" PRIi16 "", *p);
}

void
property_drawers::draw_t_i32(char* v)
{
    auto p = (std::int32_t*)v;
    ImGui::Text("%" PRIi32 "", *p);
}

void
property_drawers::draw_t_i64(char* v)
{
    auto p = (std::int64_t*)v;
    ImGui::Text("%" PRIi64 "", *p);
}

void
property_drawers::draw_t_u8(char* v)
{
    auto p = (std::uint8_t*)v;
    ImGui::Text("%" PRIu8 "", *p);
}

void
property_drawers::draw_t_u16(char* v)
{
    auto p = (std::uint16_t*)v;
    ImGui::Text("%" PRIu16 "", *p);
}

void
property_drawers::draw_t_u32(char* v)
{
    auto p = (std::uint32_t*)v;
    ImGui::Text("%" PRIu32 "", *p);
}

void
property_drawers::draw_t_u64(char* v)
{
    auto p = (std::uint64_t*)v;
    ImGui::Text("%" PRIu64 "", *p);
}

void
property_drawers::draw_t_f(char* v)
{
    auto p = (float*)v;
    ImGui::Text("%f", *p);
}

void
property_drawers::draw_t_d(char* v)
{
    auto p = (double*)v;
    ImGui::Text("%lf", *p);
}

std::function<void(char*)>
    property_drawers::drawers[(size_t)::agea::reflection::supported_type::t_last];

}  // namespace ui
}  // namespace agea