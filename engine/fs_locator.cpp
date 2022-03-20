#include "fs_locator.h"

#include <cassert>

namespace agea
{
temp_dir_context::~temp_dir_context()
{
    if (folder)
    {
        std::filesystem::remove_all(*folder);
    }
}

std::string
resource_locator::resource(category c, const std::string& resource)
{
    auto path = resource_dir(c);

    std::filesystem::path full_path = path;

    full_path /= std::filesystem::path(resource);

    if (!std::filesystem::exists(full_path))
    {
        return {};
    }
    return full_path.generic_string();
}

std::string
resource_locator::resource_dir(category c)
{
    auto path = m_root;
    switch (c)
    {
        // clang-format off
    case category::all:{break;}
    case category::assets:{path /= "assets"; break;}
    case category::configs:{path /= "configs";break;}
    case category::shaders_raw:{path /= "shaders";break;}
    case category::shaders_compiled:{path /= "cache/shaders";break;}
    case category::tmp:{path /= "tmp";break;}
    case category::levels:{path /= "levels";break;}
    case category::objects:{path /= "objects"; break; }
    case category::components:{path /= "components"; break; }
        // clang-format on
    default:
        return "";
    }
    return path.generic_string();
}

temp_dir_context
resource_locator::temp_dir()
{
    auto rand_part = [](size_t length)
    {
        std::string result;
        constexpr char* data = "0123456789ABCDEF";
        for (size_t i = 0; i < length; ++i)
        {
            result += data[rand() % 16];
        }

        return result;
    };

    auto path = rand_part(16);

    auto root = resource_dir(category::tmp);

    auto full_path = std::filesystem::path(root) / path;

    std::error_code ec;
    std::filesystem::create_directories(full_path, ec);

    auto e = ec.message();

    return temp_dir_context(full_path.generic_string());
}

bool
resource_locator::init_local_dirs()
{
    for (int i = (int)category::all; i < (int)category::last; ++i)
    {
        auto path = resource_dir((category)i);
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
    }
    return true;
}

}  // namespace agea
