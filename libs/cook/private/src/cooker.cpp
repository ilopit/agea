#include "cook/cooker.h"

#include <utils/kryga_log.h>
#include <utils/path.h>
#include <utils/process.h>
#include <serialization/serialization.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace kryga::cook
{

namespace fs = std::filesystem;

namespace
{

// Extensions we treat as shader entry points and feed into glslc.
// Include files (`.glsl`, `.h`) are resolved via `-I` at compile time and
// neither compiled nor copied.
const std::vector<std::string_view> k_shader_exts = {".vert", ".frag", ".comp"};

// Extensions that are pure include files — skipped entirely in the cook tree.
// Shader compiles pull them via `-I` from the source tree.
const std::vector<std::string_view> k_shader_include_exts = {".glsl"};

bool
has_ext(const fs::path& p, const std::vector<std::string_view>& exts)
{
    auto e = p.extension().string();
    for (auto& x : exts)
    {
        if (e == x)
            return true;
    }
    return false;
}

// `.vert` is overloaded in this repo — some packages (e.g. flight_helmet) ship
// binary mesh-vertex blobs with that extension. Sniff the first few KB for a
// GLSL `#version` directive to tell them apart.
bool
looks_like_glsl(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open())
        return false;
    char buf[512]{};
    f.read(buf, sizeof(buf) - 1);
    std::string_view s(buf, static_cast<size_t>(f.gcount()));
    // Skip leading whitespace / line comments.
    size_t i = 0;
    auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (i < s.size() && is_space(s[i]))
        ++i;
    // Accept a leading `//` or `/*` comment block, then look for `#version`
    // anywhere in the sniffed window — good enough for our shader sources.
    return s.find("#version") != std::string_view::npos;
}

bool
is_shader_entry_point(const fs::path& p)
{
    return has_ext(p, k_shader_exts) && looks_like_glsl(p);
}

bool
is_shader_include(const fs::path& p)
{
    return has_ext(p, k_shader_include_exts);
}

// mtime helper — returns 0 for missing files so comparisons treat them as
// "needs rebuild" when compared against a real mtime.
fs::file_time_type
mtime_or_min(const fs::path& p)
{
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    if (ec)
        return fs::file_time_type::min();
    return t;
}

bool
older_than_any(const fs::path& target, const std::vector<fs::path>& inputs)
{
    if (!fs::exists(target))
        return true;
    auto t = mtime_or_min(target);
    for (auto& in : inputs)
    {
        if (mtime_or_min(in) > t)
            return true;
    }
    return false;
}

void
ensure_dir(const fs::path& p)
{
    std::error_code ec;
    fs::create_directories(p, ec);
}

// ---------------------------------------------------------------------------
// Shader compilation

struct shader_job
{
    fs::path source;
    fs::path output_spv;
};

bool
compile_shader(const fs::path& glslc,
               const shader_job& job,
               const options& opts,
               const std::vector<fs::path>& include_dirs,
               std::string& err_out)
{
    ensure_dir(job.output_spv.parent_path());

    ipc::construct_params params;
    params.path_to_binary = utils::path(glslc);
    params.working_dir = utils::path(opts.source_root);

    // Keep debug info / block instance names — the SPIR-V reflection step
    // depends on them (see shader_reflection.cpp).
    std::string args = std::format(
        "--target-env={} --target-spv={} -O0 -o \"{}\" \"{}\"",
        opts.target_env,
        opts.target_spv,
        job.output_spv.generic_string(),
        job.source.generic_string());
    for (auto& inc : include_dirs)
    {
        args += std::format(" -I \"{}\"", inc.generic_string());
    }
    for (auto& def : opts.defines)
    {
        args += " -D" + def;
    }
    params.arguments = std::move(args);

    std::uint64_t rc = 0;
    if (!ipc::run_binary(params, rc) || rc != 0)
    {
        err_out = std::format("glslc returned {} for {}", rc, job.source.generic_string());
        return false;
    }
    return true;
}

std::vector<shader_job>
gather_shader_jobs(const options& opts)
{
    std::vector<shader_job> out;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(opts.source_root, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec)
        {
            ALOG_WARN("cook: directory iteration error: {}", ec.message());
            ec.clear();
            continue;
        }
        if (!it->is_regular_file())
            continue;
        auto& p = it->path();
        if (!is_shader_entry_point(p))
            continue;

        auto rel = fs::relative(p, opts.source_root, ec);
        if (ec)
        {
            ec.clear();
            continue;
        }

        shader_job j;
        j.source = p;
        j.output_spv = opts.output_root / rel;
        j.output_spv += ".spv";
        out.push_back(std::move(j));
    }
    return out;
}

// ---------------------------------------------------------------------------
// .aobj rewrite

bool
is_shader_effect_aobj(const YAML::Node& doc)
{
    auto t = doc["type_id"];
    return t && t.IsScalar() && t.as<std::string>() == "shader_effect";
}

// Rewrite a shader-effect .aobj. Returns true if the descriptor was written
// successfully (whether up-to-date or freshly rewritten).
bool
rewrite_shader_effect_aobj(const fs::path& src_aobj,
                           const fs::path& dst_aobj,
                           bool force,
                           bool& was_rewritten,
                           std::string& err_out)
{
    was_rewritten = false;

    if (!force && !older_than_any(dst_aobj, {src_aobj}))
    {
        return true;
    }

    YAML::Node doc;
    try
    {
        doc = YAML::LoadFile(src_aobj.generic_string());
    }
    catch (const std::exception& e)
    {
        err_out = std::format("parse failed: {}", e.what());
        return false;
    }

    auto redirect = [](YAML::Node& node, const char* path_key, const char* bin_key) {
        auto path_node = node[path_key];
        if (!path_node || !path_node.IsScalar())
            return;
        auto p = path_node.as<std::string>();
        // Append .spv — package-relative rid with a `.spv` suffix resolves
        // against the same `data://` mount the source would have.
        if (p.size() < 4 || p.substr(p.size() - 4) != ".spv")
        {
            p += ".spv";
            node[path_key] = p;
        }
        node[bin_key] = true;
    };

    redirect(doc, "vert", "is_vert_binary");
    redirect(doc, "frag", "is_frag_binary");

    ensure_dir(dst_aobj.parent_path());
    try
    {
        YAML::Emitter emitter;
        emitter << doc;
        if (!emitter.good())
        {
            err_out = std::format("emit failed: {}", emitter.GetLastError());
            return false;
        }
        std::ofstream out(dst_aobj, std::ios::binary);
        if (!out.is_open())
        {
            err_out = "cannot open output";
            return false;
        }
        out.write(emitter.c_str(), static_cast<std::streamsize>(emitter.size()));
        out.put('\n');
    }
    catch (const std::exception& e)
    {
        err_out = std::format("write failed: {}", e.what());
        return false;
    }
    was_rewritten = true;
    return true;
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Index manifests
//
// `vfs.mount(rid, real_path, cfg)` on the engine side only exists so that
// `build_index` can walk the mount directory and discover `.aobj` files.
// That doesn't work on Android (APK assets have no real_path, asset_manager
// enumerate is non-recursive). Instead we emit a flat manifest at cook time
// for each directory that needs indexing, and the loader reads it via VFS.
//
// Path per package (*.apkg) and level (*.alvl) directory under the cooked
// tree. Format: `# kryga-index v1` header + one relative `.aobj` path per
// line, paths use `/` separators.
void
write_index_manifest(const fs::path& dir_in_cooked,
                     const std::vector<std::string>& aobj_paths,
                     stats& s)
{
    if (aobj_paths.empty())
        return;
    auto manifest = dir_in_cooked / "kryga_index";
    ensure_dir(dir_in_cooked);
    std::ofstream out(manifest, std::ios::binary);
    if (!out.is_open())
    {
        ALOG_ERROR("cook: cannot write {}", manifest.generic_string());
        s.errors++;
        return;
    }
    out << "# kryga-index v1\n";
    for (auto& p : aobj_paths)
    {
        out << p << '\n';
    }
}

void
emit_index_manifests(const fs::path& cooked_root, stats& s)
{
    // First pass: seed a bucket for every `.apkg` / `.alvl` directory so we
    // emit a manifest even for empty ones (some packages are config-only,
    // with no `.aobj` files; the loader still expects a manifest).
    std::map<fs::path, std::vector<std::string>> buckets;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(cooked_root, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        if (!it->is_directory())
            continue;
        auto& p = it->path();
        auto ext = p.extension().string();
        if (ext == ".apkg" || ext == ".alvl")
        {
            buckets.emplace(p, std::vector<std::string>{});
        }
    }

    // Second pass: collect `.aobj` paths into their enclosing bucket.
    for (auto it = fs::recursive_directory_iterator(cooked_root, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file())
            continue;
        auto& p = it->path();
        if (p.extension() != ".aobj")
            continue;

        fs::path scan = p.parent_path();
        fs::path mount_root;
        while (scan != cooked_root && scan.has_parent_path())
        {
            auto ext = scan.extension().string();
            if (ext == ".apkg" || ext == ".alvl")
            {
                mount_root = scan;
                break;
            }
            scan = scan.parent_path();
        }
        if (mount_root.empty())
            continue;

        auto rel = fs::relative(p, mount_root, ec);
        if (ec)
        {
            ec.clear();
            continue;
        }
        buckets[mount_root].push_back(rel.generic_string());
    }

    for (auto& [root, paths] : buckets)
    {
        std::sort(paths.begin(), paths.end());
        // Force-emit even for empty paths so every mountable dir has a manifest.
        auto manifest = root / "kryga_index";
        ensure_dir(root);
        std::ofstream out(manifest, std::ios::binary);
        if (!out.is_open())
        {
            ALOG_ERROR("cook: cannot write {}", manifest.generic_string());
            s.errors++;
            continue;
        }
        out << "# kryga-index v1\n";
        for (auto& pth : paths)
        {
            out << pth << '\n';
        }
    }
}

bool
copy_file(const fs::path& src, const fs::path& dst, bool force)
{
    if (!force && fs::exists(dst) && mtime_or_min(dst) >= mtime_or_min(src))
    {
        return true;
    }
    ensure_dir(dst.parent_path());
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        ALOG_ERROR("cook: copy failed {} -> {}: {}",
                   src.generic_string(),
                   dst.generic_string(),
                   ec.message());
        return false;
    }
    return true;
}

}  // namespace

stats
cook(const options& opts)
{
    stats s;

    if (!fs::is_directory(opts.source_root))
    {
        ALOG_ERROR("cook: source_root not a directory: {}",
                   opts.source_root.generic_string());
        s.errors++;
        return s;
    }
    ensure_dir(opts.output_root);

    fs::path glslc = opts.glslc_path.empty()
                         ? opts.source_root / "tools" / "glslc.exe"
                         : opts.glslc_path;
    if (!fs::exists(glslc))
    {
        ALOG_ERROR("cook: glslc not found at {}", glslc.generic_string());
        s.errors++;
        return s;
    }

    // Assemble include dirs: source's own shaders_includes, plus whatever
    // the caller added (argen-generated gpu_types etc).
    std::vector<fs::path> includes;
    includes.push_back(opts.source_root / "shaders_includes");
    for (auto& p : opts.extra_include_dirs)
    {
        includes.push_back(p);
    }

    // --- 1. shader compilation -----------------------------------------
    auto jobs = gather_shader_jobs(opts);

    // Inputs that every shader compile depends on (coarse dependency: if any
    // include changes, we recompile everything).
    std::vector<fs::path> global_shader_deps;
    for (auto& inc : includes)
    {
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(inc, ec);
             it != fs::recursive_directory_iterator();
             it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file())
                continue;
            auto& p = it->path();
            if (has_ext(p, {".glsl", ".h"}))
            {
                global_shader_deps.push_back(p);
            }
        }
    }

    std::vector<shader_job> work;
    for (auto& j : jobs)
    {
        std::vector<fs::path> deps = global_shader_deps;
        deps.push_back(j.source);
        if (opts.force || older_than_any(j.output_spv, deps))
        {
            work.push_back(j);
        }
        else
        {
            s.shaders_up_to_date++;
        }
    }

    if (!work.empty())
    {
        ALOG_INFO("cook: compiling {} shaders ({} up-to-date)",
                  work.size(),
                  s.shaders_up_to_date);

        int jobs_n = opts.jobs > 0 ? opts.jobs : std::max(1u, std::thread::hardware_concurrency() - 1);
        std::atomic<size_t> next_idx{0};
        std::atomic<int> compiled{0};
        std::atomic<int> failed{0};
        std::mutex log_mu;

        auto worker = [&]() {
            while (true)
            {
                size_t idx = next_idx.fetch_add(1);
                if (idx >= work.size())
                    return;
                auto& j = work[idx];
                std::string err;
                if (compile_shader(glslc, j, opts, includes, err))
                {
                    compiled.fetch_add(1);
                    if (opts.verbose)
                    {
                        std::lock_guard g{log_mu};
                        ALOG_INFO("cook:   OK {}",
                                  fs::relative(j.source, opts.source_root).generic_string());
                    }
                }
                else
                {
                    failed.fetch_add(1);
                    std::lock_guard g{log_mu};
                    ALOG_ERROR("cook: FAIL {}: {}",
                               fs::relative(j.source, opts.source_root).generic_string(),
                               err);
                }
            }
        };

        std::vector<std::thread> pool;
        for (int i = 0; i < jobs_n; ++i)
        {
            pool.emplace_back(worker);
        }
        for (auto& t : pool)
            t.join();

        s.shaders_compiled = compiled.load();
        s.errors += failed.load();
    }

    // --- 2. walk remainder: .aobj and everything else ------------------
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(opts.source_root, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file())
            continue;
        auto& p = it->path();

        // Shader entry points: already handled in phase 1.
        if (is_shader_entry_point(p))
            continue;
        // Shader includes: don't ship to cooked tree.
        if (is_shader_include(p))
            continue;

        auto rel = fs::relative(p, opts.source_root, ec);
        if (ec)
        {
            ec.clear();
            continue;
        }
        auto dst = opts.output_root / rel;

        if (p.extension() == ".aobj")
        {
            // Peek at type_id — only rewrite shader_effect descriptors.
            YAML::Node doc;
            try
            {
                doc = YAML::LoadFile(p.generic_string());
            }
            catch (...)
            {
                // Corrupt/non-YAML .aobj — fall through to copy.
            }

            if (doc && is_shader_effect_aobj(doc))
            {
                bool rewritten = false;
                std::string err;
                if (rewrite_shader_effect_aobj(p, dst, opts.force, rewritten, err))
                {
                    if (rewritten)
                        s.aobj_rewritten++;
                }
                else
                {
                    ALOG_ERROR("cook: .aobj rewrite failed {}: {}",
                               rel.generic_string(),
                               err);
                    s.errors++;
                }
                continue;
            }

            if (copy_file(p, dst, opts.force))
                s.aobj_copied++;
            else
                s.errors++;
            continue;
        }

        if (copy_file(p, dst, opts.force))
            s.files_copied++;
        else
            s.errors++;
    }

    // --- 3. emit kryga_index manifests for every *.apkg / *.alvl ------
    emit_index_manifests(opts.output_root, s);

    ALOG_INFO("cook: {} shaders compiled, {} up-to-date, {} .aobj rewritten, {} copied, {} other files copied, {} errors",
              s.shaders_compiled,
              s.shaders_up_to_date,
              s.aobj_rewritten,
              s.aobj_copied,
              s.files_copied,
              s.errors);

    return s;
}

}  // namespace kryga::cook
