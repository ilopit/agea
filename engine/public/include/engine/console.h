#pragma once

#include <imgui.h>

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kryga
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
;
;

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

struct editor_console
{
    editor_console();
    ~editor_console();

    static editor_console*
    instance()
    {
        return s_instance;
    }

    static const char*
    window_title();

    void
    toggle();

    bool
    is_open() const
    {
        return m_show;
    }

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

    void
    exec_config(const std::string& line);

    static void
    handle_cmd_clear(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_reset(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_history(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_help(editor_console& e, const command_context& ctx);
    static void
    handle_cmd_run(editor_console& e, const command_context& ctx);

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

    int m_history_pos{-1};  // -1: new line, 0..History.Size-1 browsing history.

    ImGuiTextFilter m_filter;
    bool m_show = false;
    bool m_auto_scroll;
    bool m_scroll_to_bottom;
    bool m_focus_input = false;
    size_t m_lua_buffer_offset = 0;

    static editor_console* s_instance;
};
}  // namespace ui
}  // namespace kryga