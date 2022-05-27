#include "serialization/serialization.h"

#include "utils/agea_log.h"
#include <fstream>

namespace agea
{
namespace serialization
{

bool
read_container(const std::string& path, serialization::conteiner& conteiner)
{
    conteiner = YAML::LoadFile(path);

    if (!conteiner)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
write_container(const std::string& path, const serialization::conteiner& conteiner)
{
    std::ofstream file(path);

    file << conteiner;

    return true;
}

}  // namespace serialization
}  // namespace agea