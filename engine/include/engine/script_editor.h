#pragma once

#include <engine/ui.h>

#include <TextEditor.h>

#include <utils/path.h>

namespace agea
{
namespace ui
{
class script_text_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "Script text editor";
    }

    script_text_editor();

    void
    handle() override;

    std::unique_ptr<TextEditor> m_editor_window;
    std::unique_ptr<TextEditor> m_output_window;

    TextEditor::Palette m_editor_palette;
    TextEditor::Palette m_output_palette;
    TextEditor::Palette m_output_palette_error;
    TextEditor::Palette* m_selected_output_pallette = nullptr;

    utils::path m_selected_file = APATH("temp.lua");

    bool m_output_visible = false;
};
}  // namespace ui
}  // namespace agea