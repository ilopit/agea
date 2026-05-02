#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace kryga::paths
{

// Anchor filename placed at the repo root. Resolver walks up looking for it.
inline constexpr std::string_view anchor_filename = "kryga.project";

// Env var override — when set, treated as the source root and validated to
// contain `anchor_filename`. Lets CI / tests / dev tools override discovery.
inline constexpr std::string_view env_var_name = "KRYGA_PROJECT_ROOT";

struct resolved_layout
{
    // Absolute path to repo root, set only when running against a source tree
    // (anchor file found). Empty for shipped/staged-only layouts.
    std::filesystem::path source_root;

    // Where the running binary lives (or its conventional sibling on Android).
    // Always populated.
    std::filesystem::path staged_root;

    // Cooked content directory.
    //   dev:    <source_root>/build/cooked
    //   staged: <staged_root>
    std::filesystem::path cooked_dir;

    // argen.py codegen output.
    //   dev:    <source_root>/build/kryga_generated
    //   staged: <staged_root>/kryga_generated
    std::filesystem::path generated_dir;

    bool is_dev_layout = false;
};

// Hand the resolver argv[0]. Call once at the top of main(), before resolve().
// Pass nullptr on platforms where argv[0] is unreliable (Android) — resolver
// falls back to platform discovery or to the staged-only layout.
void
set_exe_path(const char* argv0);

// Returns the resolved layout, cached after the first call.
//
// Resolution order:
//   1. KRYGA_PROJECT_ROOT env var (must contain anchor file)
//   2. walk up from exe path looking for anchor → dev layout
//   3. walk up from CWD looking for anchor → dev layout
//   4. exe parent dir as staged_root → staged layout
//   5. nullopt (logs)
std::optional<resolved_layout>
resolve();

// Per-subsystem scratch dir, created if missing.
//   dev:    <source_root>/build/scratch/<name>
//   staged: <staged_root>/scratch/<name>
// Returns empty path if resolve() failed.
std::filesystem::path
scratch_dir(std::string_view name);

}  // namespace kryga::paths
