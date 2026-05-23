#pragma once

#include <gtest/gtest.h>

#include <asset_converter/converter_context.h>
#include <asset_converter/gltf_parser.h>
#include <project_paths/project_paths.h>

#include <utils/id.h>
#include <vfs/rid.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using namespace kryga::converter;

// Base fixture for engine serialization tests
class EngineTestFixture : public ::testing::Test
{
protected:
    fs::path m_output_dir;
    fs::path m_data_root;
    converter_context m_ctx;
    bool m_ctx_initialized = false;

    static fs::path
    find_data_root()
    {
        auto layout = kryga::paths::resolve();
        if (layout && !layout->source_root.empty())
        {
            auto resources = layout->source_root / "resources";
            if (fs::exists(resources / "packages" / "root.apkg"))
            {
                return resources;
            }
        }
        return {};
    }

    static fs::path
    find_test_assets()
    {
        auto layout = kryga::paths::resolve();
        if (layout && !layout->source_root.empty())
        {
            auto p = layout->source_root / "build" / "test_assets" / "converter";
            if (fs::exists(p))
            {
                return p;
            }
        }
        return {};
    }

    void
    SetUp() override
    {
        m_data_root = find_data_root();
        if (m_data_root.empty())
        {
            return;  // Tests will skip
        }

        m_output_dir = fs::temp_directory_path() / "kryga_engine_test";
        fs::remove_all(m_output_dir);
        fs::create_directories(m_output_dir);

        m_ctx_initialized = m_ctx.init(m_data_root, m_output_dir);
    }

    void
    TearDown() override
    {
        if (m_ctx_initialized)
        {
            m_ctx.shutdown();
        }
        fs::remove_all(m_output_dir);
    }

    bool
    convert_scene(const parsed_scene& scene,
                  const std::string& name,
                  converter_mode mode = converter_mode::package_new)
    {
        if (!m_ctx_initialized)
        {
            return false;
        }

        converter_options opts;
        opts.name = AID(name.c_str());
        opts.mode = mode;
        opts.output_root = kryga::vfs::rid("output", "");

        return m_ctx.convert(scene, opts);
    }

    bool
    convert_scene_with_level(const parsed_scene& scene, const std::string& name)
    {
        if (!m_ctx_initialized)
        {
            return false;
        }

        converter_options opts;
        opts.name = AID(name.c_str());
        opts.mode = converter_mode::level_with_package;
        opts.output_root = kryga::vfs::rid("output", "");

        return m_ctx.convert(scene, opts);
    }

    std::string
    read_file(const fs::path& path)
    {
        std::ifstream f(path);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    fs::path
    package_dir(const std::string& name)
    {
        return m_output_dir / (name + ".apkg");
    }

    fs::path
    level_dir(const std::string& name)
    {
        return m_output_dir / (name + ".alvl");
    }

    int
    count_files_with_extension(const fs::path& dir, const std::string& ext)
    {
        int count = 0;
        if (!fs::exists(dir))
        {
            return 0;
        }
        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() == ext)
            {
                ++count;
            }
        }
        return count;
    }
};
