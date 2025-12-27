#include "serialization/serialization.h"

#include <utils/agea_log.h>

#include <fstream>

namespace agea
{
namespace serialization
{

bool
read_container(const utils::path& path, serialization::container& container)
{
    try
    {
        container = YAML::LoadFile(path.str());
    }
    catch (const std::exception& e)
    {
        ALOG_ERROR("read_container failed {0} {1}", e.what(), path.str());
        return false;
    }

    if (!container)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
write_container(const utils::path& path, const serialization::container& container)
{
    std::ofstream file(path.fs());

    if (!file.is_open())
    {
        return false;
    }

    file << container;

    return true;
}

}  // namespace serialization
}  // namespace agea