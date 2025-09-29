#include "engine/script_editor.h"

#include <core/reflection/lua_api.h>
#include <sol2_unofficial/sol.h>
#include <chrono>
#include <utils/buffer.h>

#include <TextEditor.h>

namespace agea
{
namespace ui
{

script_text_editor::script_text_editor()
    : window(window_title())
    , m_editor_window(std::make_unique<TextEditor>())
    , m_output_window(std::make_unique<TextEditor>())
    , m_editor_palette(TextEditor::GetDarkPalette())
    , m_output_palette(TextEditor::GetDarkPalette())
    , m_output_palette_error(TextEditor::GetDarkPalette())
    , m_selected_output_pallette(nullptr)
{
    m_editor_palette[(int)TextEditor::PaletteIndex::LineNumber] = ImU32(0xfcfcfcff);
    m_editor_window->SetPalette(m_editor_palette);
    m_editor_window->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());

    m_output_palette[(int)TextEditor::PaletteIndex::LineNumber] =
        m_editor_palette[(int)TextEditor::PaletteIndex::Background];

    m_output_palette[(int)TextEditor::PaletteIndex::Default] = ImU32(0x88FF88ff);

    m_output_palette_error[(int)TextEditor::PaletteIndex::LineNumber] =
        m_editor_palette[(int)TextEditor::PaletteIndex::Background];

    m_output_palette_error[(int)TextEditor::PaletteIndex::Default] = ImU32(0xff8888ff);

    m_output_window->SetPalette(m_output_palette);
    m_output_window->SetReadOnly(true);
    m_selected_output_pallette = &m_output_palette;

    if (m_selected_file.exists())
    {
        utils::buffer b;
        utils::buffer::load(m_selected_file, b);

        std::string s((char*)b.full_data().data(), b.full_data().size());
        m_editor_window->SetText(s);
    }
}

void
script_text_editor::handle()
{
    if (!m_show)
    {
        return;
    }

    auto& editor = *m_editor_window;
    auto& error_window = *m_output_window;

    ImGui::Begin("Script Editor", &m_show,
                 ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
    ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save"))
            {
            }
            if (ImGui::MenuItem("Quit", "Alt-F4"))
                return;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            bool ro = editor.IsReadOnly();
            if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                editor.SetReadOnly(ro);
            ImGui::Separator();

            if (ImGui::MenuItem("Undo", "ALT-Backspace", nullptr, !ro && editor.CanUndo()))
                editor.Undo();
            if (ImGui::MenuItem("Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
                editor.Redo();

            ImGui::Separator();

            if (ImGui::MenuItem("Copy", "Ctrl-C", nullptr, editor.HasSelection()))
                editor.Copy();
            if (ImGui::MenuItem("Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
                editor.Cut();
            if (ImGui::MenuItem("Delete", "Del", nullptr, !ro && editor.HasSelection()))
                editor.Delete();
            if (ImGui::MenuItem("Paste", "Ctrl-V", nullptr,
                                !ro && ImGui::GetClipboardText() != nullptr))
                editor.Paste();

            ImGui::Separator();

            if (ImGui::MenuItem("Select all", nullptr, nullptr))
                editor.SetSelection(TextEditor::Coordinates(),
                                    TextEditor::Coordinates(editor.GetTotalLines(), 0));

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();

        if (ImGui::IsKeyPressed(ImGuiKey_F5))
        {
            std::time_t t = std::time(nullptr);
            char mbstr[100];

            std::strftime(mbstr, sizeof(mbstr), "%F  %T", std::localtime(&t));

            std::string output_header = std::format("Execution finished:  {0}\n", mbstr);

            output_header +=
                "------------------------------------------------------------------------\n";

            glob::lua_api::getr().reset();
            auto result = glob::lua_api::getr().state().script(editor.GetText());

            if (result.status() == sol::call_status::ok)
            {
                if (m_selected_output_pallette != &m_output_palette)
                {
                    m_output_window->SetPalette(m_output_palette);
                    m_selected_output_pallette = &m_output_palette;
                }

                output_header += glob::lua_api::getr().buffer();
                error_window.SetText(output_header);
            }
            else
            {
                if (m_selected_output_pallette != &m_output_palette_error)
                {
                    m_output_window->SetPalette(m_output_palette_error);
                    m_selected_output_pallette = &m_output_palette_error;
                }

                sol::error err = result;
                output_header += err.what();
                error_window.SetText(output_header);
            }
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            utils::buffer b;

            auto ss = m_editor_window->GetText();

            b.write((uint8_t*)ss.data(), ss.size());
            b.set_file(m_selected_file);

            utils::buffer::save(b);
        }
    }

    ImVec2 last_size = ImGui::GetWindowSize();

    editor.Render("ScriptEditor", false, ImVec2(last_size.x - 25, last_size.y - 350));

    ImGui::Separator();

    error_window.Render("ScriptEditorOutputWindow", false, ImVec2(last_size.x - 25, 250));

    ImGui::End();
}

}  // namespace ui
}  // namespace agea