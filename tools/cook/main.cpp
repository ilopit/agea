#include <cook/cooker.h>

#include <utils/kryga_log.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

void
print_usage(const char* argv0)
{
    std::cerr <<
        "Usage: " << argv0 << " --source <dir> --output <dir> [options]\n"
        "\n"
        "Options:\n"
        "  --source <dir>         Source tree (typically <repo>/resources)\n"
        "  --output <dir>         Destination cooked tree (gets bundled into APK)\n"
        "  --glslc <path>         Path to glslc.exe (default: <source>/tools/glslc.exe)\n"
        "  --include <dir>        Additional shader -I include root (may repeat)\n"
        "  --define <D>           Preprocessor define (may repeat)\n"
        "  --jobs <N>             Parallel shader compiles (default: hw cores - 1)\n"
        "  --force                Rebuild everything, ignore mtimes\n"
        "  --verbose              Log every shader compile\n"
        "  -h, --help             This message\n";
}

std::optional<std::string>
take_arg(int& i, int argc, char** argv)
{
    if (i + 1 >= argc)
        return std::nullopt;
    return std::string(argv[++i]);
}

}  // namespace

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::info);

    kryga::cook::options opts;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view a = argv[i];
        if (a == "-h" || a == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        if (a == "--source")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.source_root = std::filesystem::absolute(*v);
            continue;
        }
        if (a == "--output")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.output_root = std::filesystem::absolute(*v);
            continue;
        }
        if (a == "--glslc")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.glslc_path = *v;
            continue;
        }
        if (a == "--include")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.extra_include_dirs.emplace_back(*v);
            continue;
        }
        if (a == "--define")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.defines.push_back(*v);
            continue;
        }
        if (a == "--jobs")
        {
            auto v = take_arg(i, argc, argv);
            if (!v) { print_usage(argv[0]); return 2; }
            opts.jobs = std::atoi(v->c_str());
            continue;
        }
        if (a == "--force")
        {
            opts.force = true;
            continue;
        }
        if (a == "--verbose")
        {
            opts.verbose = true;
            continue;
        }
        std::cerr << "Unknown argument: " << a << "\n";
        print_usage(argv[0]);
        return 2;
    }

    if (opts.source_root.empty() || opts.output_root.empty())
    {
        print_usage(argv[0]);
        return 2;
    }

    auto s = kryga::cook::cook(opts);
    return s.errors == 0 ? 0 : 1;
}
