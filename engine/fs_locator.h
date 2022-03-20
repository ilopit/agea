#pragma once

#include "utils/weird_singletone.h"

#include <filesystem>
#include <optional>

namespace agea
{

enum class category : int
{
    all = 0,
    assets,
    configs,
    shaders_compiled,
    shaders_raw,
    levels,
    objects,
    components,
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
    std::string resource(category c, const std::string& resource);
    std::string resource_dir(category c);

    temp_dir_context temp_dir();

    bool init_local_dirs();

private:
    std::filesystem::path m_root = std::filesystem::current_path().parent_path();
};

namespace glob
{
struct resource_locator : public weird_singleton<::agea::resource_locator>
{
};
}  // namespace glob

}  // namespace agea
