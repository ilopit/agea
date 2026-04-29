#include "engine/private/ui/converter_window.h"

#include <core/package.h>
#include <core/package_manager.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>

#include <imgui.h>
#include <ImGuiFileDialog.h>

#include <set>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if WIN32
#include <windows.h>
#endif

#include <cstring>

namespace kryga::ui
{

namespace bp = boost::process::v2;

struct converter_window::async_state
{
    boost::asio::io_context ctx;
    std::optional<bp::process> proc;
    std::filesystem::path log_path;
};

namespace
{

std::filesystem::path
get_exe_dir()
{
#if WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

std::filesystem::path
resolve_data_root()
{
    auto& vfs = glob::glob_state().getr_vfs();
    auto real = vfs.real_path(vfs::rid("data", ""));
    if(real.has_value())
    {
        return *real;
    }
    return get_exe_dir().parent_path();
}

std::string
read_file_tail(const std::filesystem::path& p, size_t max_chars = 4096)
{
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if(!in)
    {
        return {};
    }
    auto size = static_cast<size_t>(in.tellg());
    size_t to_read = std::min(size, max_chars);
    in.seekg(static_cast<std::streamoff>(size - to_read), std::ios::beg);
    std::string out(to_read, '\0');
    in.read(out.data(), static_cast<std::streamsize>(to_read));
    return out;
}

constexpr const char* k_input_dlg_key = "ConverterInputDlg";
constexpr const char* k_output_dlg_key = "ConverterOutputDlg";

void
copy_path_to_buffer(const std::string& s, char* buf, size_t cap)
{
    size_t n = std::min(s.size(), cap - 1);
    std::memcpy(buf, s.data(), n);
    buf[n] = '\0';
}

bool
is_always_on_dep(const std::string& id)
{
    return id == "root" || id == "base";
}

void
rescan_deps(const std::string& output_root,
            std::vector<std::pair<std::string, bool>>& checklist)
{
    std::unordered_map<std::string, bool> prev;
    for(const auto& [id, checked] : checklist)
    {
        prev[id] = checked;
    }

    std::set<std::string> ids;
    ids.insert("root");
    ids.insert("base");

    auto& pm = glob::glob_state().getr_pm();
    for(const auto& [id, pkg] : pm.get_packages())
    {
        ids.insert(id.str());
    }

    if(!output_root.empty())
    {
        std::error_code ec;
        if(std::filesystem::exists(output_root, ec) &&
           std::filesystem::is_directory(output_root, ec))
        {
            for(auto& entry : std::filesystem::directory_iterator(output_root, ec))
            {
                if(entry.is_directory(ec) && entry.path().extension() == ".apkg")
                {
                    ids.insert(entry.path().stem().string());
                }
            }
        }
    }

    checklist.clear();
    for(const auto& id : ids)
    {
        bool checked = is_always_on_dep(id);
        if(!checked)
        {
            auto it = prev.find(id);
            checked = it != prev.end() ? it->second : false;
        }
        checklist.emplace_back(id, checked);
    }
}

}  // namespace

converter_window::converter_window()
    : window(window_title())
{
    m_input_path.fill('\0');
    m_output_path.fill('\0');
    m_name.fill('\0');
    m_existing_package.fill('\0');
}

converter_window::~converter_window() = default;

void
converter_window::handle()
{
    if(!handle_begin())
    {
        return;
    }

    static const char* modes[] = {"package", "extend", "level", "level+package"};

    // Poll the running converter, if any.
    if(m_async && m_async->proc)
    {
        boost::system::error_code ec;
        bool still_running = m_async->proc->running(ec);
        if(ec)
        {
            m_status = std::string("ERROR: running() failed: ") + ec.message();
            m_status_is_error = true;
            m_async.reset();
        }
        else if(!still_running)
        {
            boost::system::error_code wec;
            int rc = m_async->proc->wait(wec);
            std::string log_tail = read_file_tail(m_async->log_path);

            if(wec)
            {
                m_status = "ERROR: wait failed: " + wec.message() + "\n" + log_tail;
                m_status_is_error = true;
            }
            else if(rc != 0)
            {
                std::ostringstream os;
                os << "ERROR: asset_converter exited with code " << rc << "\n" << log_tail;
                m_status = os.str();
                m_status_is_error = true;
            }
            else
            {
                m_status = "OK\n" + log_tail;
                m_status_is_error = false;
            }

            m_async.reset();
        }
    }

    const bool busy = (m_async != nullptr);

    ImGui::Text("Import 3D assets via asset_converter.exe");
    ImGui::Separator();

    ImGui::BeginDisabled(busy);

    ImGui::InputText("##input_path", m_input_path.data(), m_input_path.size());
    ImGui::SameLine();
    if(ImGui::Button("Browse...##input"))
    {
        IGFD::FileDialogConfig cfg;
        cfg.path = ".";
        cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(k_input_dlg_key, "Select input asset",
                                                "3D assets{.glb,.gltf,.obj},All files{.*}", cfg);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Input file (.glb/.gltf/.obj)");

    ImGui::InputText("##output_root", m_output_path.data(), m_output_path.size());
    ImGui::SameLine();
    if(ImGui::Button("Browse...##output"))
    {
        IGFD::FileDialogConfig cfg;
        cfg.path = ".";
        cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(k_output_dlg_key, "Select output directory",
                                                nullptr, cfg);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Output root (directory)");

    ImGui::InputText("Name (id + filename base)", m_name.data(), m_name.size());
    ImGui::Combo("Mode", &m_mode_index, modes, IM_ARRAYSIZE(modes));
    ImGui::InputText("Existing package id (extend/level)", m_existing_package.data(),
                     m_existing_package.size());
    ImGui::EndDisabled();

    ImGui::Separator();

    std::string output_root_now(m_output_path.data());
    if(!m_deps_initialized || output_root_now != m_last_scanned_output_root)
    {
        rescan_deps(output_root_now, m_dep_checklist);
        m_last_scanned_output_root = output_root_now;
        m_deps_initialized = true;
    }

    ImGui::TextUnformatted("Dependencies:");
    ImGui::SameLine();
    ImGui::BeginDisabled(busy);
    if(ImGui::SmallButton("Refresh##deps"))
    {
        rescan_deps(output_root_now, m_dep_checklist);
    }
    ImGui::EndDisabled();

    ImGui::BeginChild("##deps_child", ImVec2(0, 120), true);
    for(auto& [id, checked] : m_dep_checklist)
    {
        bool locked = is_always_on_dep(id);
        ImGui::BeginDisabled(busy || locked);
        bool c = checked;
        if(ImGui::Checkbox(id.c_str(), &c) && !locked)
        {
            checked = c;
        }
        if(locked)
        {
            checked = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginDisabled(busy);
    bool clicked = ImGui::Button("Convert", ImVec2(120, 0));
    ImGui::EndDisabled();

    if(busy)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Running...");
    }

    if(clicked && !busy)
    {
        std::string input(m_input_path.data());
        std::string output_root(m_output_path.data());
        std::string name(m_name.data());
        std::string existing(m_existing_package.data());
        std::string mode = modes[m_mode_index];

        if(input.empty() || output_root.empty() || name.empty())
        {
            m_status = "ERROR: input, output root, and name are required";
            m_status_is_error = true;
        }
        else if((mode == "extend" || mode == "level") && existing.empty())
        {
            m_status = "ERROR: --existing-package id is required for this mode";
            m_status_is_error = true;
        }
        else
        {
            auto exe_path = get_exe_dir() / "asset_converter.exe";
            if(!std::filesystem::exists(exe_path))
            {
                auto alt = get_exe_dir() / "asset_converter";
                if(std::filesystem::exists(alt))
                {
                    exe_path = alt;
                }
            }

            auto data_root = resolve_data_root();

            std::error_code mkec;
            std::filesystem::create_directories(output_root, mkec);

            auto log_path = std::filesystem::temp_directory_path() / "kryga_asset_converter.log";

            std::vector<std::string> args = {
                "--input",       input,
                "--data-root",   data_root.string(),
                "--output-root", output_root,
                "--name",        name,
                "--mode",        mode,
            };
            if(mode == "extend" || mode == "level")
            {
                args.push_back("--existing-package");
                args.push_back(existing);
            }
            for(const auto& [id, checked] : m_dep_checklist)
            {
                if(checked)
                {
                    args.push_back("--dep");
                    args.push_back(id);
                }
            }

            boost::filesystem::path bexe(exe_path.string());
            boost::filesystem::path blog(log_path.string());

            auto async = std::make_unique<async_state>();
            async->log_path = log_path;

            try
            {
                async->proc.emplace(async->ctx.get_executor(), bexe, args,
                                    bp::process_stdio{{}, blog, blog});
                m_async = std::move(async);
                m_status.clear();
                m_status_is_error = false;
            }
            catch(const std::exception& e)
            {
                m_status = std::string("ERROR: launch failed: ") + e.what() + " (exe: " +
                           exe_path.string() + ")";
                m_status_is_error = true;
            }
        }
    }

    if(!m_status.empty())
    {
        ImGui::Separator();
        if(m_status_is_error)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_status.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", m_status.c_str());
        }
    }

    ImVec2 min_size(600.0f, 400.0f);
    ImVec2 max_size(FLT_MAX, FLT_MAX);

    if(ImGuiFileDialog::Instance()->Display(k_input_dlg_key, ImGuiWindowFlags_NoCollapse, min_size,
                                            max_size))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            copy_path_to_buffer(ImGuiFileDialog::Instance()->GetFilePathName(),
                                m_input_path.data(), m_input_path.size());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if(ImGuiFileDialog::Instance()->Display(k_output_dlg_key, ImGuiWindowFlags_NoCollapse,
                                            min_size, max_size))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            copy_path_to_buffer(ImGuiFileDialog::Instance()->GetCurrentPath(),
                                m_output_path.data(), m_output_path.size());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    handle_end();
}

}  // namespace kryga::ui
