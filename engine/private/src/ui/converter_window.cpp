#include "engine/private/ui/converter_window.h"

#include <core/package.h>
#include <core/package_manager.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>

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
    if (real.has_value())
    {
        return *real;
    }
    return get_exe_dir().parent_path();
}

std::string
read_file_tail(const std::filesystem::path& p, size_t max_chars = 4096)
{
    std::ifstream in(p, std::ios::binary | std::ios::ate);
    if (!in)
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

bool
is_always_on_dep(const std::string& id)
{
    return id == "root" || id == "base";
}

void
rescan_deps(const std::string& output_root, std::vector<std::pair<std::string, bool>>& checklist)
{
    std::unordered_map<std::string, bool> prev;
    for (const auto& [id, checked] : checklist)
    {
        prev[id] = checked;
    }

    std::set<std::string> ids;
    ids.insert("root");
    ids.insert("base");

    auto& pm = glob::glob_state().getr_pm();
    for (const auto& [id, pkg] : pm.get_packages())
    {
        ids.insert(id.str());
    }

    if (!output_root.empty())
    {
        std::error_code ec;
        if (std::filesystem::exists(output_root, ec) &&
            std::filesystem::is_directory(output_root, ec))
        {
            for (auto& entry : std::filesystem::directory_iterator(output_root, ec))
            {
                if (entry.is_directory(ec) && entry.path().extension() == ".apkg")
                {
                    ids.insert(entry.path().stem().string());
                }
            }
        }
    }

    checklist.clear();
    for (const auto& id : ids)
    {
        bool checked = is_always_on_dep(id);
        if (!checked)
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
converter_window::set_completed_callback(converter_completed_callback cb)
{
    m_completed_callback = std::move(cb);
}

void
converter_window::poll()
{
    if (!m_async || !m_async->proc)
    {
        return;
    }

    boost::system::error_code ec;
    bool still_running = m_async->proc->running(ec);
    if (ec)
    {
        m_status = std::string("ERROR: running() failed: ") + ec.message();
        m_status_is_error = true;

        converter_result cr;
        cr.success = false;
        cr.exit_code = -1;
        cr.log_tail = m_status;
        m_async.reset();
        if (m_completed_callback)
        {
            m_completed_callback(cr);
        }
        return;
    }

    if (still_running)
    {
        return;
    }

    boost::system::error_code wec;
    int rc = m_async->proc->wait(wec);
    std::string log_tail = read_file_tail(m_async->log_path);

    converter_result cr;
    cr.exit_code = rc;

    if (wec)
    {
        m_status = "ERROR: wait failed: " + wec.message() + "\n" + log_tail;
        m_status_is_error = true;
        cr.success = false;
        cr.log_tail = m_status;
    }
    else if (rc != 0)
    {
        std::ostringstream os;
        os << "ERROR: asset_converter exited with code " << rc << "\n" << log_tail;
        m_status = os.str();
        m_status_is_error = true;
        cr.success = false;
        cr.log_tail = m_status;
    }
    else
    {
        m_status = "OK\n" + log_tail;
        m_status_is_error = false;
        cr.success = true;
        cr.log_tail = log_tail;
    }

    m_async.reset();
    if (m_completed_callback)
    {
        m_completed_callback(cr);
    }
}

bool
converter_window::is_running() const
{
    return m_async != nullptr;
}

std::string
converter_window::get_status_text() const
{
    return m_status;
}

bool
converter_window::is_status_error() const
{
    return m_status_is_error;
}

bool
converter_window::submit_conversion(const std::string& input,
                                    const std::string& output_root,
                                    const std::string& name,
                                    const std::string& mode,
                                    const std::string& existing_package,
                                    const std::vector<std::string>& deps)
{
    if (m_async)
    {
        return false;
    }

    if (input.empty() || output_root.empty() || name.empty())
    {
        m_status = "ERROR: input, output root, and name are required";
        m_status_is_error = true;
        return false;
    }
    if ((mode == "extend" || mode == "level") && existing_package.empty())
    {
        m_status = "ERROR: --existing-package id is required for this mode";
        m_status_is_error = true;
        return false;
    }

    auto exe_path = get_exe_dir() / "asset_converter.exe";
    if (!std::filesystem::exists(exe_path))
    {
        auto alt = get_exe_dir() / "asset_converter";
        if (std::filesystem::exists(alt))
        {
            exe_path = alt;
        }
    }

    auto data_root = resolve_data_root();

    std::error_code mkec;
    std::filesystem::create_directories(output_root, mkec);

    auto log_path = std::filesystem::temp_directory_path() / "kryga_asset_converter.log";

    std::vector<std::string> args = {
        "--input",
        input,
        "--data-root",
        data_root.string(),
        "--output-root",
        output_root,
        "--name",
        name,
        "--mode",
        mode,
    };
    if (mode == "extend" || mode == "level")
    {
        args.push_back("--existing-package");
        args.push_back(existing_package);
    }
    for (const auto& id : deps)
    {
        args.push_back("--dep");
        args.push_back(id);
    }

    boost::filesystem::path bexe(exe_path.string());
    boost::filesystem::path blog(log_path.string());

    auto async = std::make_unique<async_state>();
    async->log_path = log_path;

    try
    {
        async->proc.emplace(
            async->ctx.get_executor(), bexe, args, bp::process_stdio{{}, blog, blog});
        m_async = std::move(async);
        m_status.clear();
        m_status_is_error = false;
        return true;
    }
    catch (const std::exception& e)
    {
        m_status =
            std::string("ERROR: launch failed: ") + e.what() + " (exe: " + exe_path.string() + ")";
        m_status_is_error = true;
        return false;
    }
}

std::vector<std::pair<std::string, bool>>
converter_window::list_deps(const std::string& output_root)
{
    std::vector<std::pair<std::string, bool>> result;
    rescan_deps(output_root, result);
    return result;
}

void
converter_window::handle()
{
}

}  // namespace kryga::ui
