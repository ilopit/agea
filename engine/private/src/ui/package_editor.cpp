#include "engine/private/ui/package_editor.h"

#include "engine/private/ui/object_editor.h"

#include <core/package.h>
#include <core/package_manager.h>

#include <packages/root/game_object.h>
#include <packages/root/assets/material.h>

#include <imgui.h>

namespace agea::ui
{

void
package_editor::handle()
{
    if (!handle_begin())
    {
        return;
    }

    auto& pkgs = glob::package_manager::getr().get_packages();

    // if (ImGui::TreeNode("Packages"))
    {
        for (auto& p : glob::package_manager::getr().get_packages())
        {
            bool opened = ImGui::TreeNode(p.first.cstr(), "[%s] %s", "s", p.first.cstr());
            if (opened)
            {
                int i = 0;
                for (auto& obj : p.second->get_objects())
                {
                    if (ImGui::TreeNodeEx((void*)(i), ImGuiTreeNodeFlags_Bullet,
                                          obj->get_id().cstr()))
                    {
                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                        {
                            get_window<object_editor>()->show(obj.get());
                        }

                        ImGui::TreePop();
                    }
                    ++i;
                }
                ImGui::TreePop();
            }
        }
        // ImGui::TreePop();
    }

    handle_end();
}

void
package_editor::draw_package_obj(core::package* p, selection_context& sc)
{
    ++sc.i;
    ImGui::NextColumn();
    ImGui::AlignTextToFramePadding();
    // ImGui::Text("%s", root->get_id().cstr());
    ImGui::NextColumn();

    //     if (open)
    //     {
    //         if (!root->get_children().empty())
    //         {
    //             for (auto obj : root->get_children())
    //             {
    //                 draw_package_obj((root::game_object_component*)obj, sc);
    //             }
    //         }
    //         ImGui::TreePop();
    //     }
}

}  // namespace agea::ui