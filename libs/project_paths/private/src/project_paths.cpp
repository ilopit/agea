#include "project_paths/project_paths.h"

#include <utils/kryga_log.h>

#include <cstdlib>
#include <mutex>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(__ANDROID__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace kryga::paths
{

namespace
{

std::mutex g_mtx;
std::optional<fs::path> g_exe_path_override;
std::optional<resolved_layout> g_cached;

fs::path
discover_exe_path_native()
{
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH)
    {
        return {};
    }
    return fs::path(buf, buf + n);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
    {
        return {};
    }
    std::error_code ec;
    auto canonical = fs::canonical(buf, ec);
    return ec ? fs::path{buf} : canonical;
#elif defined(__ANDROID__)
    // argv[0] on Android is the package name, not a path. Caller should have
    // passed nullptr to set_exe_path; we have nothing to discover.
    return {};
#else
    std::error_code ec;
    auto p = fs::canonical("/proc/self/exe", ec);
    return ec ? fs::path{} : p;
#endif
}

fs::path
current_exe_path()
{
    if (g_exe_path_override && !g_exe_path_override->empty())
    {
        return *g_exe_path_override;
    }
    return discover_exe_path_native();
}

bool
has_anchor(const fs::path& dir)
{
    std::error_code ec;
    return fs::exists(dir / anchor_filename, ec);
}

std::optional<fs::path>
walk_up_for_anchor(const fs::path& start)
{
    if (start.empty())
    {
        return std::nullopt;
    }
    std::error_code ec;
    auto cur = fs::weakly_canonical(start, ec);
    if (ec)
    {
        cur = start;
    }
    while (true)
    {
        if (has_anchor(cur))
        {
            return cur;
        }
        if (!cur.has_parent_path() || cur.parent_path() == cur)
        {
            return std::nullopt;
        }
        cur = cur.parent_path();
    }
}

resolved_layout
make_dev_layout(const fs::path& source_root, const fs::path& staged_root)
{
    resolved_layout l;
    l.source_root = source_root;
    l.staged_root = staged_root;
    l.cooked_dir = source_root / "build" / "cooked";
    l.generated_dir = source_root / "build" / "kryga_generated";
    l.is_dev_layout = true;
    return l;
}

resolved_layout
make_staged_layout(const fs::path& staged_root)
{
    resolved_layout l;
    l.staged_root = staged_root;
    l.cooked_dir = staged_root;
    l.generated_dir = staged_root / "kryga_generated";
    l.is_dev_layout = false;
    return l;
}

}  // namespace

void
set_exe_path(const char* argv0)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    g_cached.reset();
    if (argv0 == nullptr || argv0[0] == '\0')
    {
        g_exe_path_override.reset();
        return;
    }
    std::error_code ec;
    auto canonical = fs::weakly_canonical(fs::path(argv0), ec);
    g_exe_path_override = ec ? fs::path(argv0) : canonical;
}

std::optional<resolved_layout>
resolve()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_cached)
    {
        return g_cached;
    }

    auto exe = current_exe_path();
    fs::path staged_root;
    if (!exe.empty())
    {
        // Kryga staged layout puts the binary in a `bin/` subdir of the
        // staged root (next to packages/, shaders/, cache/, etc.). When
        // running outside that convention, the exe's parent itself is the
        // staged root.
        auto exe_parent = exe.parent_path();
        if (exe_parent.filename() == "bin")
        {
            staged_root = exe_parent.parent_path();
        }
        else
        {
            staged_root = exe_parent;
        }
    }

    // 1. Env var
    if (const char* env_root = std::getenv(std::string(env_var_name).c_str()); env_root && *env_root)
    {
        fs::path root(env_root);
        if (!has_anchor(root))
        {
            ALOG_ERROR("paths::resolve: KRYGA_PROJECT_ROOT={} has no {} anchor",
                       root.string(),
                       anchor_filename);
            return std::nullopt;
        }
        g_cached = make_dev_layout(root, staged_root.empty() ? root : staged_root);
        ALOG_INFO("paths::resolve: dev layout (env) source_root=[{}] staged_root=[{}]",
                  g_cached->source_root.string(),
                  g_cached->staged_root.string());
        return g_cached;
    }

    // 2. Walk up from exe path
    if (auto anchor_dir = walk_up_for_anchor(staged_root))
    {
        g_cached = make_dev_layout(*anchor_dir, staged_root);
        ALOG_INFO("paths::resolve: dev layout (exe-walk) source_root=[{}] staged_root=[{}]",
                  g_cached->source_root.string(),
                  g_cached->staged_root.string());
        return g_cached;
    }

    // 3. Walk up from CWD
    std::error_code ec;
    auto cwd = fs::current_path(ec);
    if (!ec)
    {
        if (auto anchor_dir = walk_up_for_anchor(cwd))
        {
            g_cached = make_dev_layout(*anchor_dir,
                                       staged_root.empty() ? *anchor_dir : staged_root);
            ALOG_INFO("paths::resolve: dev layout (cwd-walk) source_root=[{}] staged_root=[{}]",
                      g_cached->source_root.string(),
                      g_cached->staged_root.string());
            return g_cached;
        }
    }

    // 4. Staged-only — exe parent dir
    if (!staged_root.empty())
    {
        g_cached = make_staged_layout(staged_root);
        ALOG_INFO("paths::resolve: staged layout staged_root=[{}]", g_cached->staged_root.string());
        return g_cached;
    }

    // 5. Failure
    ALOG_ERROR("paths::resolve: could not determine project root — no {} anchor reachable, no "
               "exe path discovered, no {} env var",
               anchor_filename,
               env_var_name);
    return std::nullopt;
}

fs::path
scratch_dir(std::string_view name)
{
    auto layout = resolve();
    if (!layout)
    {
        return {};
    }
    fs::path base = layout->is_dev_layout ? (layout->source_root / "build" / "scratch")
                                          : (layout->staged_root / "scratch");
    fs::path dir = base / fs::path(name);
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

}  // namespace kryga::paths
