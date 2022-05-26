#include "serialization/serialization.h"

#include "utils/agea_log.h"

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

}  // namespace serialization
}  // namespace agea