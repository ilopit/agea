#include "serialization/serialization.h"

#include "utils/agea_log.h"
#include <fstream>

namespace agea
{
namespace serialization
{

bool
read_container(const utils::path& path, serialization::conteiner& conteiner)
{
    conteiner = YAML::LoadFile(path.str());

    if (!conteiner)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
write_container(const utils::path& path, const serialization::conteiner& conteiner)
{
    std::ofstream file(path.fs());

    if (!file.is_open())
    {
        return false;
    }

    file << conteiner;

    return true;
}

}  // namespace serialization
}  // namespace agea