#pragma once

#include "utils/weird_singletone.h"

#include "utils/path.h"

#include <filesystem>
#include <optional>
#include <functional>

namespace agea
{

extern const std::string TEXTURE_EXT;
extern const std::string MATERIAL_EXT;
extern const std::string MESH_EXT;

enum class category : int
{
    all = 0,
    assets,
    components,
    configs,
    fonts,
    levels,
    objects,
    packages,
    shaders_compiled,
    shaders_raw,
    tmp,
    last = tmp
};

using resource_path = std::optional<std::string>;

struct temp_dir_context
{
    temp_dir_context() = default;

    temp_dir_context(std::string s)
        : folder(std::move(s))
    {
    }

    resource_path folder;

    ~temp_dir_context();
};

class resource_locator
{
public:
    using cb = std::function<bool(const std::string& path)>;

    utils::path
    resource(category c, const std::string& resource_id);

    utils::path
    resource_dir(category c);

    bool
    run_over_folder(category c, const cb& callback, const std::string& extention);

    temp_dir_context
    temp_dir();

    bool
    init_local_dirs();

private:
    std::filesystem::path m_root = std::filesystem::current_path().parent_path();
};

namespace glob
{
struct resource_locator : public ::agea::selfcleanable_singleton<::agea::resource_locator>
{
};
}  // namespace glob

}  // namespace agea
