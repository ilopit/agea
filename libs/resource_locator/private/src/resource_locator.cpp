#include "resource_locator/resource_locator.h"

#include <utils/string_utility.h>
#include <utils/agea_log.h>

#include <global_state/global_state.h>

#include <cassert>

namespace agea
{

temp_dir_context::~temp_dir_context()
{
    if (folder)
    {
        std::filesystem::remove_all(folder->fs());
    }
}

utils::path
resource_locator::resource(category c, const std::string& resource_id)
{
    auto path = resource_dir(c);

    utils::path full_path(path);

    full_path.append(resource_id);

    if (!full_path.exists())
    {
        ALOG_WARN("[{0}] - doesn't exist", full_path.str());
        return {};
    }
    return full_path;
}

utils::path
resource_locator::resource_dir(category c)
{
    auto path = m_root;
    switch (c)
    {
        // clang-format off
    case category::all:              {                              break;}
    case category::assets:           {path /= "assets";             break;}
    case category::editor:           {path /= "editor";             break;}
    case category::configs:          {path /= "configs";            break;}
    case category::shaders_raw:      {path /= "shaders";            break;}
    case category::shaders_compiled: {path /= "cache/shaders";      break;}
    case category::shaders_includes: {path /= "shaders_includes";   break;}
    case category::tmp:              {path /= "tmp";                break;}
    case category::tools:            {path /= "tools";              break;}
    case category::levels:           {path /= "levels";             break;}
    case category::objects:          {path /= "objects";            break;}
    case category::components:       {path /= "components";         break;}
    case category::packages:         {path /= "packages";           break;}
    case category::fonts:            {path /= "fonts";              break;}
        // clang-format on
    default:
        AGEA_never("Not supported category");
        return {};
    }
    return utils::path(path);
}

bool
resource_locator::run_over_folder(category c, const cb& callback, const std::string& extention)
{
    for (auto& p : std::filesystem::recursive_directory_iterator(resource_dir(c).fs()))
    {
        if (p.is_directory())
        {
            continue;
        }

        auto file_path = p.path().generic_string();

        if (string_utils::ends_with(file_path, extention) && !callback(file_path))
        {
            return false;
        }
    }

    return true;
}

temp_dir_context
resource_locator::temp_dir()
{
    auto rand_part = [](size_t length)
    {
        std::string result;
        constexpr char data[] = "0123456789ABCDEF";
        for (size_t i = 0; i < length; ++i)
        {
            result += data[rand() % 16];
        }

        return result;
    };

    auto path = rand_part(16);

    auto root = resource_dir(category::tmp);

    auto full_path = root.fs() / path;

    std::error_code ec;
    std::filesystem::create_directories(full_path, ec);

    auto e = ec.message();

    return temp_dir_context(APATH(full_path));
}

bool
resource_locator::init_local_dirs()
{
    for (int i = (int)category::all; i < (int)category::last; ++i)
    {
        auto path = resource_dir((category)i);
        std::error_code ec;
        std::filesystem::create_directories(path.fs(), ec);
    }
    return true;
}

}  // namespace agea
