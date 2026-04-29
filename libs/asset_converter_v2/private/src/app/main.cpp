#include <asset_converter/converter_context.h>
#include <asset_converter/gltf_parser.h>
#include <asset_converter/obj_parser.h>

#include <utils/id.h>
#include <utils/kryga_log.h>
#include <vfs/rid.h>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

kryga::converter::converter_mode
parse_mode(const std::string& s)
{
    if (s == "package")
    {
        return kryga::converter::converter_mode::package_new;
    }
    if (s == "extend")
    {
        return kryga::converter::converter_mode::package_extend;
    }
    if (s == "level")
    {
        return kryga::converter::converter_mode::level_standalone;
    }
    if (s == "level+package")
    {
        return kryga::converter::converter_mode::level_with_package;
    }
    throw CLI::ValidationError("--mode", "must be one of: package, extend, level, level+package");
}

enum class input_format
{
    gltf,
    obj,
};

input_format
detect_format(const std::filesystem::path& p)
{
    auto ext = p.extension().string();
    std::transform(ext.begin(),
                   ext.end(),
                   ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".glb" || ext == ".gltf")
    {
        return input_format::gltf;
    }
    if (ext == ".obj")
    {
        return input_format::obj;
    }
    throw CLI::ValidationError("--input", "unsupported extension: " + ext);
}

}  // namespace

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::info);

    CLI::App app{"Kryga asset converter"};

    std::string input_path;
    std::string data_root;
    std::string output_root;
    std::string name;
    std::string mode_str = "package";
    std::string prefix;
    std::string existing_package;
    std::vector<std::string> deps_input;
    bool no_dedup_textures = false;
    bool no_dedup_materials = false;

    app.add_option("-i,--input", input_path, "Input asset file (.glb/.gltf/.obj)")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--data-root",
                   data_root,
                   "Data directory containing engine packages (mounted as data://)")
        ->required()
        ->check(CLI::ExistingDirectory);
    app.add_option(
           "--output-root", output_root, "Output directory (mounted as output://, will be created)")
        ->required();
    app.add_option("-n,--name", name, "Output name (used for .apkg/.alvl filename and id)")
        ->required();
    app.add_option("-m,--mode", mode_str, "Mode: package | extend | level | level+package");
    app.add_option("-p,--prefix", prefix, "Prefix for generated ids");
    app.add_option("--existing-package",
                   existing_package,
                   "Existing package id (required for extend/level modes)");
    app.add_option("-d,--dep", deps_input, "Package id to declare as dependency (repeatable)");
    app.add_flag("--no-dedup-textures", no_dedup_textures, "Disable texture deduplication");
    app.add_flag("--no-dedup-materials", no_dedup_materials, "Disable material deduplication");

    CLI11_PARSE(app, argc, argv);

    kryga::converter::converter_options opts;
    try
    {
        opts.mode = parse_mode(mode_str);
    }
    catch (const CLI::ValidationError& e)
    {
        ALOG_ERROR("{}", e.what());
        return 1;
    }
    opts.name = kryga::utils::id::make_id(name);
    opts.output_root = kryga::vfs::rid("output", "");
    opts.prefix = prefix;
    opts.deduplicate_textures = !no_dedup_textures;
    opts.deduplicate_materials = !no_dedup_materials;

    for (const auto& d : deps_input)
    {
        opts.dependencies.push_back(kryga::utils::id::make_id(d));
    }

    if (opts.mode == kryga::converter::converter_mode::package_extend ||
        opts.mode == kryga::converter::converter_mode::level_standalone)
    {
        if (existing_package.empty())
        {
            ALOG_ERROR("--existing-package is required for mode '{}'", mode_str);
            return 1;
        }
        opts.existing_package_id = kryga::utils::id::make_id(existing_package);
    }

    std::filesystem::path input(input_path);
    input_format fmt;
    try
    {
        fmt = detect_format(input);
    }
    catch (const CLI::ValidationError& e)
    {
        ALOG_ERROR("{}", e.what());
        return 1;
    }

    kryga::converter::parsed_scene scene;
    bool parsed = false;
    if (fmt == input_format::gltf)
    {
        parsed = kryga::converter::parse_gltf(input_path, scene);
    }
    else
    {
        parsed = kryga::converter::parse_obj(input_path, scene);
    }
    if (!parsed)
    {
        ALOG_ERROR("Failed to parse input: {}", input_path);
        return 2;
    }

    ALOG_INFO("Parsed scene: {} meshes, {} textures, {} materials, {} lights, {} cameras, {} nodes",
              scene.meshes.size(),
              scene.textures.size(),
              scene.materials.size(),
              scene.lights.size(),
              scene.cameras.size(),
              scene.nodes.size());

    kryga::converter::converter_context ctx;
    if (!ctx.init(data_root, output_root))
    {
        ALOG_ERROR("converter_context init failed");
        return 3;
    }

    if (!ctx.convert(scene, opts))
    {
        ALOG_ERROR("conversion failed");
        return 4;
    }

    ALOG_INFO("Conversion complete");
    return 0;
}
