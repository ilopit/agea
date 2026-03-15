#include "engine/private/ui/gizmo_editor.h"
#include "engine/private/ui/object_editor.h"
#include "engine/editor.h"
#include "engine/ui.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/components/game_object_component.h>

#include <global_state/global_state.h>
#include <native/native_window.h>
#include <vulkan_render/kryga_render.h>

namespace kryga::ui
{

void
gizmo_editor::draw()
{
    if (glob::glob_state().getr_game_editor().get_mode() == engine::editor_mode::playing)
    {
        return;
    }

    // Get camera data and set up ImGuizmo viewport (always, before anything else)
    auto cam = glob::glob_state().getr_game_editor().get_camera_data();

    // Un-flip Vulkan Y for ImGuizmo (expects OpenGL convention)
    glm::mat4 proj = cam.projection;
    proj[1][1] *= -1;

    const float* view_ptr = &cam.view[0][0];
    const float* proj_ptr = &proj[0][0];

    auto window_size = glob::glob_state().getr_native_window().get_size();
    ImGuizmo::SetRect(0, 0, (float)window_size.w, (float)window_size.h);

    // Toolbar (always visible)
    {
        ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("##gizmo_toolbar", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);

        // Mode
        bool t = (m_mode == gizmo_mode::translate);
        bool r = (m_mode == gizmo_mode::rotate);
        bool s = (m_mode == gizmo_mode::scale);

        if (ImGui::RadioButton("T (1)", t))
            m_mode = gizmo_mode::translate;
        ImGui::SameLine();
        if (ImGui::RadioButton("R (2)", r))
            m_mode = gizmo_mode::rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("S (3)", s))
            m_mode = gizmo_mode::scale;

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Local / World
        bool is_local = (m_space == ImGuizmo::LOCAL);
        if (ImGui::RadioButton("Local", is_local))
            m_space = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", !is_local))
            m_space = ImGuizmo::WORLD;

        // Snap
        ImGui::Checkbox("Snap", &m_snap_enabled);
        if (m_snap_enabled)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            switch (m_mode)
            {
            case gizmo_mode::translate:
                ImGui::DragFloat("##snap_val", &m_snap_translate, 0.1f, 0.1f, 100.0f, "%.1f");
                break;
            case gizmo_mode::rotate:
                ImGui::DragFloat("##snap_val", &m_snap_rotate, 1.0f, 1.0f, 180.0f, "%.0f");
                break;
            case gizmo_mode::scale:
                ImGui::DragFloat("##snap_val", &m_snap_scale, 0.01f, 0.01f, 10.0f, "%.2f");
                break;
            }
        }

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        bool grid_visible = glob::glob_state().getr_vulkan_render().is_grid_visible();
        if (ImGui::Checkbox("Grid", &grid_visible))
        {
            glob::glob_state().getr_vulkan_render().set_grid_visible(grid_visible);
        }

        ImGui::End();
    }

    // Object selection checks — everything below needs a selected object
    auto* obj_editor = get_window<object_editor>();
    if (!obj_editor || !obj_editor->current_object)
    {
        return;
    }

    auto* game_obj = obj_editor->current_object->as<root::game_object>();
    if (!game_obj)
    {
        return;
    }

    auto* root_comp = game_obj->get_root_component();
    if (!root_comp)
    {
        return;
    }

    // Keyboard mode switching (only when not using gizmo and not typing in ImGui)
    if (!ImGuizmo::IsUsing() && !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_1))
            m_mode = gizmo_mode::translate;
        if (ImGui::IsKeyPressed(ImGuiKey_2))
            m_mode = gizmo_mode::rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_3))
            m_mode = gizmo_mode::scale;
    }

    // Use the actual transform matrix the renderer uses — guarantees gizmo matches visual position
    glm::mat4 model = root_comp->get_transform_matrix();

    // Map gizmo_mode to ImGuizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_mode)
    {
    case gizmo_mode::translate:
        op = ImGuizmo::TRANSLATE;
        break;
    case gizmo_mode::rotate:
        op = ImGuizmo::ROTATE;
        break;
    case gizmo_mode::scale:
        op = ImGuizmo::SCALE;
        break;
    }

    float model_arr[16];
    memcpy(model_arr, &model[0][0], sizeof(float) * 16);

    // Build snap array if enabled
    float snap_values[3] = {};
    const float* snap_ptr = nullptr;
    if (m_snap_enabled)
    {
        switch (m_mode)
        {
        case gizmo_mode::translate:
            snap_values[0] = snap_values[1] = snap_values[2] = m_snap_translate;
            break;
        case gizmo_mode::rotate:
            snap_values[0] = snap_values[1] = snap_values[2] = m_snap_rotate;
            break;
        case gizmo_mode::scale:
            snap_values[0] = snap_values[1] = snap_values[2] = m_snap_scale;
            break;
        }
        snap_ptr = snap_values;
    }

    ImGuizmo::Manipulate(view_ptr, proj_ptr, op, m_space, model_arr, nullptr, snap_ptr);

    m_using = ImGuizmo::IsUsing();

    if (m_using)
    {
        // Decompose the manipulated matrix back into components
        // Engine convention: M = T * S * R
        // The 3x3 part = S * R, where S = diag(sx, sy, sz)
        // Row i of (S*R) = si * R_row_i, so row_length(i) = si
        glm::mat4 new_model;
        memcpy(&new_model[0][0], model_arr, sizeof(float) * 16);

        // Extract position from column 3
        glm::vec3 new_pos = glm::vec3(new_model[3]);

        // Extract the 3x3 part (= S * R)
        glm::mat3 sr = glm::mat3(new_model);

        // Scale from row lengths (glm is column-major, so rows are accessed per component)
        glm::vec3 new_scl;
        new_scl.x = glm::length(glm::vec3(sr[0][0], sr[1][0], sr[2][0]));
        new_scl.y = glm::length(glm::vec3(sr[0][1], sr[1][1], sr[2][1]));
        new_scl.z = glm::length(glm::vec3(sr[0][2], sr[1][2], sr[2][2]));

        // Remove scale from rows to get pure rotation
        // In column-major: row i components are [col0[i], col1[i], col2[i]]
        glm::mat3 rot_mat;
        rot_mat[0][0] = sr[0][0] / new_scl.x;
        rot_mat[1][0] = sr[1][0] / new_scl.x;
        rot_mat[2][0] = sr[2][0] / new_scl.x;
        rot_mat[0][1] = sr[0][1] / new_scl.y;
        rot_mat[1][1] = sr[1][1] / new_scl.y;
        rot_mat[2][1] = sr[2][1] / new_scl.y;
        rot_mat[0][2] = sr[0][2] / new_scl.z;
        rot_mat[1][2] = sr[1][2] / new_scl.z;
        rot_mat[2][2] = sr[2][2] / new_scl.z;

        glm::quat new_quat = glm::quat_cast(rot_mat);
        glm::vec3 new_euler_rad = glm::eulerAngles(new_quat);
        glm::vec3 new_rot_deg = glm::degrees(new_euler_rad);

        root_comp->set_position(root::vec3(new_pos));
        root_comp->set_rotation(root::vec3(new_rot_deg));
        root_comp->set_scale(root::vec3(new_scl));

        root_comp->update_matrix();
        root_comp->update_children_matrixes();
        root_comp->mark_transform_dirty();
    }
}

bool
gizmo_editor::is_using() const
{
    return m_using;
}

}  // namespace kryga::ui
