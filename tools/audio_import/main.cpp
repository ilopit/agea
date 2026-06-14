// Standalone audio importer CLI.
//
// Imports an encoded audio file (WAV/MP3/FLAC/OGG) into a cooked audio_clip
// asset (<package>/class/audio_clips/<id>.aobj + .aaud). It spins up the same
// headless workspace the asset converter uses (reflection types + base/root
// packages) so serialization works, then calls the shared importer routine.
//
//   audio_import --input beep.wav --id beep --package resources/packages/base.apkg

#include <asset_converter/converter_context.h>
#include <assets_importer/audio_importer.h>

#include <project_paths/project_paths.h>

#include <utils/kryga_log.h>
#include <utils/path.h>
#include <utils/id.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

void
print_usage(const char* argv0)
{
    std::cerr << "Usage: " << argv0
              << " --input <file> --id <asset_id> --package <pkg_dir>\n"
                 "\n"
                 "  --input    source audio file (WAV/MP3/FLAC/OGG)\n"
                 "  --id       asset id (becomes the file stem)\n"
                 "  --package  destination package dir (e.g. resources/packages/base.apkg)\n";
}

}  // namespace

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::info);
    kryga::paths::set_exe_path(argc > 0 ? argv[0] : nullptr);

    std::string input, id, package;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view a = argv[i];
        auto take = [&](std::string& out)
        {
            if (i + 1 < argc)
                out = argv[++i];
        };
        if (a == "--input")
            take(input);
        else if (a == "--id")
            take(id);
        else if (a == "--package")
            take(package);
        else if (a == "-h" || a == "--help")
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << a << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }

    if (input.empty() || id.empty() || package.empty())
    {
        print_usage(argv[0]);
        return 2;
    }

    auto layout = kryga::paths::resolve();
    if (!layout || layout->source_root.empty())
    {
        ALOG_ERROR("audio_import: cannot resolve project layout from exe path");
        return 1;
    }
    std::filesystem::path data_root = layout->source_root / "resources";
    std::filesystem::path out_tmp = std::filesystem::temp_directory_path() / "kryga_audio_import";
    std::filesystem::create_directories(out_tmp);

    kryga::converter::converter_context ctx;
    if (!ctx.init(data_root, out_tmp))
    {
        ALOG_ERROR("audio_import: workspace init failed (data_root={})", data_root.string());
        return 1;
    }

    bool ok = kryga::assets_importer::convert_audio_to_aaud(
        kryga::utils::path(input), kryga::utils::path(package), AID(id.c_str()));

    ctx.shutdown();
    return ok ? 0 : 1;
}
