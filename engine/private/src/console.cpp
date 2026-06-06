#include "engine/console.h"

#include "utils/string_utility.h"

#include "core/reflection/lua_api.h"
#include "global_state/global_state.h"
#include <vulkan_render/render_system.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_config.h>
#include <vulkan_render/render_enums.h>
#include <vfs/rid.h>
#include <sol2_unofficial/sol.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <span>

namespace kryga
{
namespace ui
{

namespace
{
const std::string LOG_FILE = "editor_console.items.log";
const std::string HISTORY_FILE = "editor_console.history.log";
}  // namespace

namespace
{

using rcfg = render::render_config;

std::string
trim(const std::string& s)
{
    auto a = s.find_first_not_of(' ');
    if (a == std::string::npos)
    {
        return "";
    }
    auto b = s.find_last_not_of(' ');
    return s.substr(a, b - a + 1);
}

// --- generic field accessors via member pointer ---

template <typename T>
std::string
fmt_val(const T& v);

template <>
std::string
fmt_val(const bool& v)
{
    return v ? "on" : "off";
}
template <>
std::string
fmt_val(const uint32_t& v)
{
    return std::format("{}", v);
}
template <>
std::string
fmt_val(const float& v)
{
    return std::format("{}", v);
}
template <>
std::string
fmt_val(const render::pcf_mode& v)
{
    return render::to_string(v);
}

template <typename T>
bool
parse_val(const std::string& s, T& out);

template <>
bool
parse_val(const std::string& s, bool& out)
{
    if (s == "true" || s == "1" || s == "on")
    {
        out = true;
        return true;
    }
    if (s == "false" || s == "0" || s == "off")
    {
        out = false;
        return true;
    }
    return false;
}

template <>
bool
parse_val(const std::string& s, uint32_t& out)
{
    try
    {
        out = static_cast<uint32_t>(std::stoul(s));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

template <>
bool
parse_val(const std::string& s, float& out)
{
    try
    {
        out = std::stof(s);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

template <>
bool
parse_val(const std::string& s, render::pcf_mode& out)
{
    return render::from_string(s, out);
}

// --- config variable table ---

using config_getter = std::string (*)(const rcfg&);
using config_setter = bool (*)(rcfg&, const std::string&);

struct config_var
{
    const char* key;
    config_getter get;
    config_setter set;
};

template <auto MemberOuter, auto MemberInner>
config_var
make_var(const char* key)
{
    return {
        key,
        [](const rcfg& c) -> std::string { return fmt_val((c.*MemberOuter).*MemberInner); },
        [](rcfg& c, const std::string& s) -> bool
        { return parse_val(s, (c.*MemberOuter).*MemberInner); },
    };
}

// clang-format off
const config_var config_vars_arr[] = {
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::enabled>("shadows.enabled"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::pcf>("shadows.pcf"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::bias>("shadows.bias"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::normal_bias>("shadows.normal_bias"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::local_bias>("shadows.local_bias"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::local_normal_bias>("shadows.local_normal_bias"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::pcf_world_radius>("shadows.pcf_world_radius"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::hardware_pcf>("shadows.hardware_pcf"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::hardware_pcf_local>("shadows.hardware_pcf_local"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::depth_16bit>("shadows.depth_16bit"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::cascade_count>("shadows.cascade_count"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::distance>("shadows.distance"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::atlas_size>("shadows.atlas_size"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::csm_tile_size>("shadows.csm_tile_size"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::local_tile_size>("shadows.local_tile_size"),
    make_var<&rcfg::shadows, &rcfg::shadow_cfg::max_local_lights>("shadows.max_local_lights"),

    make_var<&rcfg::clusters, &rcfg::cluster_cfg::tile_size>("clusters.tile_size"),
    make_var<&rcfg::clusters, &rcfg::cluster_cfg::depth_slices>("clusters.depth_slices"),
    make_var<&rcfg::clusters, &rcfg::cluster_cfg::max_lights_per_cluster>("clusters.max_lights_per_cluster"),

    make_var<&rcfg::lighting, &rcfg::lighting_cfg::directional_enabled>("lighting.directional_enabled"),
    make_var<&rcfg::lighting, &rcfg::lighting_cfg::local_enabled>("lighting.local_enabled"),
    make_var<&rcfg::lighting, &rcfg::lighting_cfg::baked_enabled>("lighting.baked_enabled"),

    make_var<&rcfg::debug, &rcfg::debug_cfg::editor_mode>("debug.editor_mode"),
    make_var<&rcfg::debug, &rcfg::debug_cfg::show_grid>("debug.show_grid"),
    make_var<&rcfg::debug, &rcfg::debug_cfg::light_wireframe>("debug.light_wireframe"),
    make_var<&rcfg::debug, &rcfg::debug_cfg::light_icons>("debug.light_icons"),
    make_var<&rcfg::debug, &rcfg::debug_cfg::frustum_culling>("debug.frustum_culling"),

    make_var<&rcfg::render_scale, &rcfg::render_scale_cfg::enabled>("render_scale.enabled"),
    make_var<&rcfg::render_scale, &rcfg::render_scale_cfg::divisor>("render_scale.divisor"),

    make_var<&rcfg::outline, &rcfg::outline_cfg::enabled>("outline.enabled"),
    make_var<&rcfg::outline, &rcfg::outline_cfg::depth_threshold>("outline.depth_threshold"),
    make_var<&rcfg::outline, &rcfg::outline_cfg::normal_threshold>("outline.normal_threshold"),
};
// clang-format on

const std::span<const config_var> config_vars{config_vars_arr};

// --- tab completion ---

void
config_completions(const std::string& prefix, std::vector<std::string>& candidates)
{
    auto try_add = [&](const std::string& full_path)
    {
        if (!full_path.starts_with(prefix) || full_path.size() <= prefix.size())
        {
            return;
        }
        auto dot = full_path.find('.', prefix.size());
        auto candidate = dot != std::string::npos ? full_path.substr(0, dot) : full_path;
        for (auto& c : candidates)
        {
            if (c == candidate)
            {
                return;
            }
        }
        candidates.push_back(candidate);
    };

    for (auto& v : config_vars)
    {
        try_add(std::string("config.render.") + v.key);
    }
    try_add("config.render.save");
    try_add("config.render.reset");
}

// --- get / set by key ---

std::string
config_get_value(const rcfg& cfg, const std::string& key)
{
    for (auto& v : config_vars)
    {
        if (key == v.key)
        {
            return v.get(cfg);
        }
    }
    return {};
}

bool
config_set_value(rcfg& cfg, const std::string& key, const std::string& value)
{
    for (auto& v : config_vars)
    {
        if (key == v.key)
        {
            return v.set(cfg, value);
        }
    }
    return false;
}

// --- section print / reset ---

void
config_print_section(editor_console& e, const rcfg& cfg, const std::string& section)
{
    const std::string prefix = section.empty() ? "" : section + ".";
    bool any = false;
    std::string last_section;

    for (auto& v : config_vars)
    {
        std::string_view k = v.key;
        if (!k.starts_with(prefix))
        {
            continue;
        }

        auto dot = k.find('.');
        auto sec = dot != std::string_view::npos ? k.substr(0, dot) : k;

        if (section.empty() && sec != last_section)
        {
            e.add_log("[%.*s]", (int)sec.size(), sec.data());
            last_section = sec;
        }

        e.add_log("  %-36s = %s", v.key, v.get(cfg).c_str());
        any = true;
    }

    if (!any)
    {
        e.add_log("[error] unknown config path: %s", section.c_str());
    }
}

void
config_reset_section(editor_console& e, rcfg& cfg, const std::string& section)
{
    const rcfg defaults;
    const std::string prefix = section.empty() ? "" : section + ".";
    bool any = false;

    for (auto& v : config_vars)
    {
        std::string_view k = v.key;
        if (!k.starts_with(prefix))
        {
            continue;
        }
        v.set(cfg, v.get(defaults));
        any = true;
    }

    if (!any)
    {
        e.add_log("[error] unknown section: %s", section.c_str());
        return;
    }

    if (section.empty())
    {
        e.add_log("config.render reset to defaults");
    }
    else
    {
        e.add_log("config.render.%s reset to defaults", section.c_str());
    }
}

}  // namespace

static void
strtrim(char* s)
{
    char* str_end = s + strlen(s);
    while (str_end > s && str_end[-1] == ' ')
    {
        str_end--;
    }
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

    add_log("Welcome KRYGA 0.1 CLI!");
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

    for (const auto& i : m_items)
    {
        file << i;
        if (i.empty() || i.back() != '\n')
        {
            file << '\n';
        }
    }
    file.close();

    file.open(HISTORY_FILE);
    for (const auto& i : m_history)
    {
        file << i;
        if (i.empty() || i.back() != '\n')
        {
            file << '\n';
        }
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
    if (!m_show)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float console_height = io.DisplaySize.y * 0.28f;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, console_height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.65f));

    ImGui::Begin("##console_overlay", nullptr, flags);

    // Filter bar
    m_filter.Draw("Filter", 180);
    ImGui::Separator();

    // Scrolling log region — reserve space for separator + input line
    const float footer_height =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild(
        "##console_scroll", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    for (auto& item : m_items)
    {
        if (!m_filter.PassFilter(item.c_str()))
        {
            continue;
        }

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

    // Command input
    bool reclaim_focus = m_focus_input;
    m_focus_input = false;

    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                           ImGuiInputTextFlags_CallbackCompletion |
                                           ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("##console_input",
                         m_buf.data(),
                         m_buf.size(),
                         input_text_flags,
                         &text_edit_callback_stub,
                         (void*)this))
    {
        char* s = m_buf.data();
        strtrim(s);
        if (s[0])
        {
            exec_command(s);
        }

        s[0] = '\0';
        reclaim_focus = true;
    }

    ImGui::SetItemDefaultFocus();
    ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // Block all input while console is open
    io.WantCaptureKeyboard = true;
    io.WantCaptureMouse = true;
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
    if (command_line.starts_with("config"))
    {
        exec_config(command_line);
        m_history.push_back(command_line);
    }
    else if (command_line.front() == '-')
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
        auto lua = glob::glob_state().get_lua();

        auto result = lua->state().script(command_line);

        if (result.status() == sol::call_status::ok)
        {
            auto to_add = lua->buffer().substr(m_lua_buffer_offset);
            if (!to_add.empty())
            {
                m_items.push_back(to_add);
                m_lua_buffer_offset = lua->buffer().size();
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

        std::string word(word_start, word_end);
        std::vector<std::string> candidates;

        if (word.starts_with("config") || word.starts_with("conf") || word.starts_with("con"))
        {
            // Config path completion
            std::string prefix = word;
            // Ensure trailing dot for intermediate completions
            config_completions(prefix, candidates);
            // Also try with trailing dot if no results and word matches a node exactly
            if (candidates.empty())
            {
                config_completions(prefix + ".", candidates);
            }
        }
        else
        {
            // Command tree completion
            m_context.reset();
            string_utils::split(word, " ", m_context.tokens);
            m_commands.hints(m_context.tokens, candidates);
        }

        if (candidates.empty())
        {
            add_log("No match for \"%s\"\n", word.c_str());
        }
        else if (candidates.size() == 1)
        {
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0].c_str());
        }
        else
        {
            int match_len = (int)word.size();
            for (;;)
            {
                int c = 0;
                bool all_match = true;
                for (size_t i = 0; i < candidates.size() && all_match; i++)
                {
                    if ((int)candidates[i].size() <= match_len)
                    {
                        all_match = false;
                    }
                    else if (i == 0)
                    {
                        c = candidates[i][match_len];
                    }
                    else if (c != candidates[i][match_len])
                    {
                        all_match = false;
                    }
                }
                if (!all_match)
                {
                    break;
                }
                match_len++;
            }

            if (match_len > 0)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(
                    data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + match_len);
            }

            add_log("Possible matches:\n");
            for (auto& c : candidates)
            {
                add_log("- %s", c.c_str());
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
    if (ctx.tokens.size() < 2)
    {
        e.add_log("[error] --run requires a file path argument\n");
        return;
    }
    std::string file_name = ctx.tokens[1];

    auto lua = glob::glob_state().get_lua();

    auto result = lua->state().script_file(file_name);

    if (result.status() == sol::call_status::ok)
    {
        auto to_add = lua->buffer().substr(e.m_lua_buffer_offset);
        if (!to_add.empty())
        {
            e.m_items.push_back(to_add);
        }

        e.m_lua_buffer_offset = lua->buffer().size();
    }
    else
    {
        sol::error err = result;
        e.add_log("# [error] %s\n", err.what());
    }
}

editor_console* editor_console::s_instance = nullptr;

editor_console::editor_console()
    : m_history_pos(-1)
{
    s_instance = this;
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
        add_log("Welcome KRYGA 0.1 CLI!");
    }
}

editor_console::~editor_console()
{
    s_instance = nullptr;
    save_to_file();
}

void
editor_console::toggle()
{
    m_show = !m_show;
    if (m_show)
    {
        m_focus_input = true;
    }
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

kryga::ui::cmd_handler
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

void
editor_console::exec_config(const std::string& line)
{
    auto eq = line.find('=');
    if (eq != std::string::npos)
    {
        auto path = trim(line.substr(0, eq));
        auto value = trim(line.substr(eq + 1));

        if (!path.starts_with("config.render.") || path.size() <= 14)
        {
            add_log("[error] unknown config path: %s", path.c_str());
            return;
        }
        auto key = path.substr(14);

        auto& cfg = glob::glob_state().getr_render().renderer.get_pending_render_config();
        if (!config_set_value(cfg, key, value))
        {
            add_log("[error] invalid: %s = %s", key.c_str(), value.c_str());
            return;
        }
        cfg.validate();

        auto actual = config_get_value(cfg, key);
        add_log("  %-36s = %s", key.c_str(), actual.c_str());
        return;
    }

    auto path = trim(line);

    if (path == "config" || path == "config.render")
    {
        auto& cfg = glob::glob_state().getr_render().renderer.get_render_config();
        config_print_section(*this, cfg, "");
        return;
    }

    if (!path.starts_with("config.render."))
    {
        add_log("[error] unknown config path: %s", path.c_str());
        return;
    }

    auto rest = path.substr(14);

    if (rest == "save")
    {
        auto& cfg = glob::glob_state().getr_render().renderer.get_render_config();
        if (cfg.save_to_cache(vfs::rid("rtcache://render.acfg")))
        {
            add_log("config saved to rtcache://render.acfg");
        }
        else
        {
            add_log("[error] failed to save config");
        }
        return;
    }

    if (rest == "reset")
    {
        auto& cfg = glob::glob_state().getr_render().renderer.get_pending_render_config();
        config_reset_section(*this, cfg, "");
        return;
    }

    if (rest.ends_with(".reset"))
    {
        auto section = rest.substr(0, rest.size() - 6);
        auto& cfg = glob::glob_state().getr_render().renderer.get_pending_render_config();
        config_reset_section(*this, cfg, section);
        return;
    }

    auto& cfg = glob::glob_state().getr_render().renderer.get_render_config();
    auto val = config_get_value(cfg, rest);
    if (!val.empty())
    {
        add_log("  %-36s = %s", rest.c_str(), val.c_str());
    }
    else
    {
        config_print_section(*this, cfg, rest);
    }
}

}  // namespace ui
}  // namespace kryga