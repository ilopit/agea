#include "engine/console.h"

#include "utils/string_utility.h"

#include "core/reflection/lua_api.h"
#include "core/global_state.h"
#include <sol2_unofficial/sol.h>

#include <ctype.h>
#include <stdlib.h>
#include <fstream>
#include <filesystem>

namespace agea
{
namespace ui
{

namespace
{
const std::string LOG_FILE = "editor_console.items.log";
const std::string HISTORY_FILE = "editor_console.history.log";
}  // namespace

static void
Strtrim(char* s)
{
    char* str_end = s + strlen(s);
    while (str_end > s && str_end[-1] == ' ') str_end--;
    *str_end = 0;
}

void
editor_console::clear_log()
{
    m_items.clear();
    std::filesystem::remove(LOG_FILE);
}

void
editor_console::reset_log()
{
    clear_log();
    m_history.clear();
    std::filesystem::remove(HISTORY_FILE);

    add_log("Welcome AGEA 0.1 CLI!");
}

bool
editor_console::load_from_file()
{
    std::ifstream file(LOG_FILE);
    if (!file.is_open())
    {
        return false;
    }

    std::string buf;

    while (std::getline(file, buf))
    {
        m_items.emplace_back(buf);
    }

    file.close();

    file.open(HISTORY_FILE);
    if (!file.is_open())
    {
        return false;
    }

    while (std::getline(file, buf))
    {
        m_history.emplace_back(buf);
    }

    return true;
}

void
editor_console::save_to_file()
{
    std::ofstream file(LOG_FILE);
    if (!file.is_open())
    {
        return;
    }

    std::string buf;

    for (auto i : m_items)
    {
        if (i.back() != '\n')
        {
            i += '\n';
        }

        file << i;
    }
    file.close();

    file.open(HISTORY_FILE);
    for (auto i : m_history)
    {
        if (i.back() != '\n')
        {
            i += '\n';
        }
        file << i;
    }
}

void
editor_console::add_log(const char* fmt, ...) IM_FMTARGS(2)
{
    // FIXME-OPT
    m_buf.fill('\0');

    va_list args;
    va_start(args, fmt);
    auto idx = vsnprintf(m_buf.data(), 1024, fmt, args);
    m_buf[idx] = 0;
    va_end(args);
    m_items.push_back(m_buf.data());

    m_buf.fill('\0');
}

void
editor_console::handle()
{
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);

    if (!handle_begin())
    {
        return;
    }

    // As a specific feature guaranteed by the library, after calling Begin() the last Item
    // represent the title bar. So e.g. IsItemHovered() will return true when hovering the title
    // bar. Here we create a context menu only available from the title bar.
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close Console"))
        {
            m_show = false;
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
        ImGui::EndPopup();
    }

    // Options, Filter
    if (ImGui::Button("Options"))
    {
        ImGui::OpenPopup("Options");
    }

    ImGui::SameLine();
    m_filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Clear"))
        {
            clear_log();
        }

        ImGui::EndPopup();
    }

    // Display every line as a separate entry so we can change their color or add custom
    // widgets. If you only want raw text you can use ImGui::TextUnformatted(log.begin(),
    // log.end()); NB- if you have thousands of entries this approach may be too inefficient and
    // may require user-side clipping to only process visible items. The clipper will
    // automatically measure the height of your first item and then "seek" to display only items
    // in the visible area. To use the clipper we can replace your standard loop:
    //      for (int i = 0; i < Items.Size; i++)
    //   With:
    //      ImGuiListClipper clipper;
    //      clipper.Begin(Items.Size);
    //      while (clipper.Step())
    //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
    // - That your items are evenly spaced (same height)
    // - That you have cheap random access to your elements (you can access them given their
    // index,
    //   without processing all the ones before)
    // You cannot this code as-is if a filter is active because it breaks the 'cheap
    // random-access' property. We would need random-access on the post-filtered list. A typical
    // application wanting coarse clipping and filtering may want to pre-compute an array of
    // indices or offsets of items that passed the filtering test, recomputing this array when
    // user changes the filter, and appending newly elements as they are inserted. This is left
    // as a task to the user until we can manage to improve this example code! If your items are
    // of variable height:
    // - Split them into same height items would be simpler and facilitate random-seeking into
    // your list.
    // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from
    // your items.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));  // Tighten spacing

    for (auto& item : m_items)
    {
        if (!m_filter.PassFilter(item.c_str()))
        {
            continue;
        }

        // Normally you would store more information in your item than just a string.
        // (e.g. make Items[] an array of structure, store color/type etc.)
        ImVec4 color;
        bool has_color = false;
        if (strstr(item.c_str(), "[error]"))
        {
            color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            has_color = true;
        }
        else if (strncmp(item.c_str(), "# ", 2) == 0)
        {
            color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
            has_color = true;
        }
        if (has_color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
        }
        ImGui::TextUnformatted(item.c_str());
        if (has_color)
        {
            ImGui::PopStyleColor();
        }
    }

    if (m_scroll_to_bottom || (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
    {
        ImGui::SetScrollHereY(1.0f);
    }
    m_scroll_to_bottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    // Command-line
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                           ImGuiInputTextFlags_CallbackCompletion |
                                           ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("Input", m_buf.data(), m_buf.size(), input_text_flags,
                         &text_edit_callback_stub, (void*)this))
    {
        char* s = m_buf.data();
        Strtrim(s);
        if (s[0])
        {
            exec_command(s);
        }

        strcpy_s(s, 1, "");
        reclaim_focus = true;
    }

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
    {
        ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget
    }

    handle_end();
}

void
editor_console::exec_command(const std::string& command_line)
{
    add_log("# %s\n", command_line.c_str());

    // Insert into history. First find match and delete it so it can be pushed to the back.
    // This isn't trying to be smart or optimal.
    m_history_pos = -1;
    for (int i = (int)m_history.size() - 1; i >= 0; i--)
    {
        if (m_history[i] == command_line)
        {
            m_history.erase(m_history.begin() + i);
            break;
        }
    }
    if (command_line.front() == '-')
    {
        string_utils::split(command_line, " ", m_context.tokens);

        auto handle = m_commands.get(m_context.tokens, m_context.offset);

        if (handle)
        {
            handle(*this, m_context);
            m_history.push_back(command_line);
        }
        else
        {
            add_log("Unknown command: '%s'\n", command_line.c_str());
        }
    }
    else
    {
        auto lua = glob::state::getr().get_lua();

        auto result = lua->state().script(command_line);

        if (result.status() == sol::call_status::ok)
        {
            auto to_add = lua->buffer().substr(t);
            if (!to_add.empty())
            {
                m_items.push_back(to_add);
                t = lua->buffer().size();
            }
            m_history.push_back(command_line);
        }
        else
        {
            sol::error err = result;
            add_log("# [error] %s\n", err.what());
        }
    }

    // On command input, we scroll to bottom even if AutoScroll==false
    m_scroll_to_bottom = true;
}

int
editor_console::text_edit_callback_stub(ImGuiInputTextCallbackData* data)
{
    editor_console* console = (editor_console*)data->UserData;
    return console->text_edit_callback(data);
}

int
editor_console::text_edit_callback(ImGuiInputTextCallbackData* data)
{
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
        // Example of TEXT COMPLETION

        // Locate beginning of current word
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_start = data->Buf;
        while (word_start > data->Buf)
        {
            const char c = word_start[-1];
            if (c == ' ' || c == '\t' || c == ',' || c == ';')
            {
                break;
            }

            word_start--;
        }

        // Build a list of candidates
        std::string cmd(word_start, word_end);
        m_context.reset();
        string_utils::split(cmd, " ", m_context.tokens);

        std::vector<std::string> candidates;
        auto depth = m_commands.hints(m_context.tokens, candidates);

        if (candidates.size() == 0)
        {
            if (depth == 0)
            {
                add_log("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
            }
        }
        else if (candidates.size() == 1)
        {
            // Single match. Delete the beginning of the word and replace it entirely so we've
            // got nice casing.
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0].c_str());
            // data->InsertChars(data->CursorPos, " ");
        }
        else
        {
            // Multiple matches. Complete as much as we can..
            // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as
            // matches.
            int match_len = (int)(word_end - word_start);
            for (;;)
            {
                int c = 0;
                bool all_candidates_matches = true;
                for (int i = 0; i < candidates.size() && all_candidates_matches; i++)
                {
                    if (i == 0)
                    {
                        c = toupper(candidates[i][match_len]);
                    }
                    else if (c == 0 || c != toupper(candidates[i][match_len]))
                    {
                        all_candidates_matches = false;
                    }
                }
                if (!all_candidates_matches)
                {
                    break;
                }
                match_len++;
            }

            if (match_len > 0)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0].c_str(),
                                  candidates[0].c_str() + match_len);
            }

            // List matches
            add_log("Possible matches:\n");
            for (int i = 0; i < candidates.size(); i++)
            {
                add_log("- %s\n", candidates[i].c_str());
            }
        }

        break;
    }
    case ImGuiInputTextFlags_CallbackHistory:
    {
        // Example of HISTORY
        const int prev_history_pos = m_history_pos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (m_history_pos == -1)
            {
                m_history_pos = (int)m_history.size() - 1;
            }
            else if (m_history_pos > 0)
            {
                m_history_pos--;
            }
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (m_history_pos != -1)
            {
                if (++m_history_pos >= m_history.size())
                {
                    m_history_pos = -1;
                }
            }
        }

        // A better implementation would preserve the data on the current input line along with
        // cursor position.
        if (prev_history_pos != m_history_pos)
        {
            const char* history_str = (m_history_pos >= 0) ? m_history[m_history_pos].data() : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    }
    }
    return 0;
}

void
editor_console::handle_cmd_clear(editor_console& e, const command_context&)
{
    e.clear_log();
}

void
editor_console::handle_cmd_reset(editor_console& e, const command_context&)
{
    e.reset_log();
}

void
editor_console::handle_cmd_history(editor_console& e, const command_context&)
{
    int first = (int)e.m_history.size() - 10;
    for (int i = first > 0 ? first : 0; i < e.m_history.size(); i++)
    {
        e.add_log("%3d: %s\n", i, e.m_history[i].c_str());
    }
}

void
editor_console::handle_cmd_help(editor_console& e, const command_context&)
{
    e.add_log("Commands:");

    for (auto& c : e.m_commands.m_child)
    {
        e.add_log("- %s", c.first.c_str());
    }
}

void
editor_console::handle_cmd_run(editor_console& e, const command_context& ctx)
{
    std::string file_name = ctx.tokens[1];

    auto lua = glob::state::getr().get_lua();

    auto result = lua->state().script_file(file_name);

    if (result.status() == sol::call_status::ok)
    {
        auto to_add = lua->buffer().substr(e.t);
        if (!to_add.empty())
        {
            e.m_items.push_back(to_add);
        }

        e.t = lua->buffer().size();
    }
    else
    {
        sol::error err = result;
        e.add_log("# [error] %s\n", err.what());
    }
}

editor_console::editor_console()
    : window(window_title())
    , m_history_pos(-1)
{
    m_buf.fill('\0');

    m_commands.add({"--help"}, handle_cmd_help);
    m_commands.add({"--history"}, handle_cmd_history);
    m_commands.add({"--clear"}, handle_cmd_clear);
    m_commands.add({"--reset"}, handle_cmd_reset);
    m_commands.add({"--run"}, handle_cmd_run);

    m_auto_scroll = true;
    m_scroll_to_bottom = false;

    if (!load_from_file())
    {
        add_log("Welcome AGEA 0.1 CLI!");
    }
}

editor_console::~editor_console()
{
    save_to_file();
}

const char*
editor_console::window_title()
{
    return "Console";
}

commands_tree::commands_tree()
{
    m_child["#root"] = std::make_unique<node>();
}

void
commands_tree::add(const std::vector<std::string>& path, cmd_handler h)
{
    auto head = m_child["#root"].get();

    for (auto& p : path)
    {
        auto& child = head->m_child[p];
        if (!child)
        {
            child = std::make_unique<node>();
        }

        head = child.get();
    }
    head->m_handler = std::move(h);
}

agea::ui::cmd_handler
commands_tree::get(const std::vector<std::string>& path, int& depth)
{
    auto head = m_child["#root"].get();
    for (auto& p : path)
    {
        auto itr = head->m_child.find(p);
        if (itr == head->m_child.end())
        {
            return head->m_handler;
        }
        ++depth;
        auto& child = itr->second;

        head = child.get();
    }
    return head->m_handler;
}

int
commands_tree::hints(const std::vector<std::string>& path, std::vector<std::string>& hints)
{
    int depth = 0;
    if (path.empty())
    {
        return depth;
    }

    auto head = m_child["#root"].get();

    for (auto pitr = path.begin(); pitr != path.end() - 1; ++pitr)
    {
        auto& p = *pitr;
        auto itr = head->m_child.find(p);
        if (itr == head->m_child.end())
        {
            return depth;
        }

        auto& child = itr->second;
        if (!child)
        {
            child = std::make_unique<node>();
        }

        head = child.get();
        ++depth;
    }

    if (head)
    {
        auto itr = head->m_child.find(path.back());
        if (itr != head->m_child.end())
        {
            ++depth;
            auto& items = (*(itr->second)).m_child;
            for (auto& c : items)
            {
                hints.push_back(c.first);
            }
        }
        else
        {
            for (auto& c : head->m_child)
            {
                if (string_utils::starts_with(c.first, path.back()))
                {
                    hints.push_back(c.first);
                }
            }
        }
    }

    return depth;
}

}  // namespace ui
}  // namespace agea