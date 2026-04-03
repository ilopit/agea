#include "vfs/backend.h"

#include <utils/kryga_log.h>

namespace kryga
{
namespace vfs
{

bool
backend::build_index(std::string_view ext_filter)
{
    m_index.clear();

    bool duplicate = false;

    enumerate(
        "",
        [&](std::string_view path, bool is_dir) -> bool
        {
            if (is_dir)
            {
                return true;
            }

            auto slash = path.rfind('/');
            auto filename = (slash != std::string_view::npos) ? path.substr(slash + 1) : path;
            auto dot = filename.rfind('.');
            if (dot == std::string_view::npos)
            {
                return true;
            }

            auto stem = std::string(filename.substr(0, dot));

            if (!m_index.add(stem, std::string(path)))
            {
                duplicate = true;
                return false;
            }

            return true;
        },
        true,
        ext_filter);

    return !duplicate;
}

}  // namespace vfs
}  // namespace kryga
