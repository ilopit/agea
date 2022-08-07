#pragma once

#include "model/model_minimal.h"

#include <imgui.h>
#include "engine/ui.h"

namespace agea
{
namespace ui
{
struct editor_console;
class node;

struct command_context
{
    int offset = 0;
    std::vector<std::string> tokens;

    void
    reset()
    {
        offset = 0;
        tokens.clear();
    }
};

using cmd_handler = std::function<void(editor_console&, const command_context&)>;

using tree_container = std::unordered_map<std::string, std::unique_ptr<node>>;

class node
{
public:
    tree_container m_child;
    cmd_handler m_handler = nullptr;
};

class commands_tree
{
public:
    commands_tree();

    void
    add(const std::vector<std::string>& path, cmd_handler h);

    cmd_handler
    get(const std::vector<std::string>& path, int& depth);

    int
    hints(const std::vector<std::string>& path, std::vector<std::string>& hints);

    tree_container m_child;
};

struct editor_console : public window
{
    editor_console();
    ~editor_console();

    static const char*
    window_title();

    void
    clear_log();

    void
    reset_log();

    bool
    load_from_file();

    void
    save_to_file();

    void
    add_log(const char* fmt, ...) IM_FMTARGS(2);

    void
    handle();

    void
    exec_command(const std::string& command_line);

    static void
    handle_cmd_clear(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_reset(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_history(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_help(editor_console& e, const command_context& ctx);

private:
    static int
    text_edit_callback_stub(ImGuiInputTextCallbackData* data);

    int
    text_edit_callback(ImGuiInputTextCallbackData* data);

    std::array<char, 1024> m_buf;
    commands_tree m_commands;
    command_context m_context;

    std::vector<std::string> m_items;
    std::vector<std::string> m_history;

    int m_history_pos;  // -1: new line, 0..History.Size-1 browsing history.

    ImGuiTextFilter m_filter;
    bool m_auto_scroll;
    bool m_scroll_to_bottom;
    size_t t = 0;
};
}  // namespace ui
}  // namespace agea