#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kryga::cook
{

struct options
{
    // Absolute path to the source tree (typically `<repo>/resources`).
    std::filesystem::path source_root;

    // Absolute path to the destination cooked tree. Will be created if missing.
    // Gets bundled into the APK on Android; desktop reads via VFS `data://`
    // mount pointed here.
    std::filesystem::path output_root;

    // Absolute path to `glslc.exe`. If empty, falls back to
    // `<source_root>/tools/glslc.exe`.
    std::filesystem::path glslc_path;

    // Extra `-I <dir>` entries passed to glslc for shader compilation.
    // The following are always included automatically in this order:
    //   1. <source_root>/shaders_includes
    //   2. <repo>/libs/render/gpu_types/public/include   (if extra_include_roots contains the argen
    //   dir)
    //   3. every entry in this vector, in order
    // The repo root is inferred as <source_root>/..
    std::vector<std::filesystem::path> extra_include_dirs;

    // Preprocessor defines (-D<value>) fed to glslc.
    std::vector<std::string> defines;

    // Vulkan target. Defaults match kryga's runtime env (vk1.2 / spv1.5).
    std::string target_env = "vulkan1.2";
    std::string target_spv = "spv1.5";

    // Parallelism for shader compilation. 0 = hardware_concurrency().
    int jobs = 0;

    // If true, recompile/rewrite every file regardless of mtime.
    bool force = false;

    // Verbose per-file logging.
    bool verbose = false;
};

struct stats
{
    int shaders_compiled = 0;
    int shaders_up_to_date = 0;
    int aobj_rewritten = 0;
    int aobj_copied = 0;
    int files_copied = 0;
    int errors = 0;
};

// Cooks `source_root` into `output_root`:
//   - `.vert` / `.frag` / `.comp` under `shader_effects/` and `shaders_includes/[bake/]`
//     are compiled to `.spv` via glslc.
//   - shader-effect `.aobj` descriptors (`type_id: shader_effect`) are rewritten so
//     `vert:` / `frag:` point at the cooked SPV rids and `is_*_binary: true`.
//   - every other file is copied as-is.
// Incremental: skips work when the output is newer than all relevant inputs.
//
// Returns a stats struct. Check `stats.errors` for failure.
stats
cook(const options& opts);

}  // namespace kryga::cook
