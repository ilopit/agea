#include "serialization/serialization.h"

#include <vfs/vfs.h>
#include <global_state/global_state.h>
#include <utils/kryga_log.h>

#include <fstream>
#include <sstream>

namespace kryga
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

bool
read_container(const vfs::rid& id, serialization::container& container)
{
    auto& vfs = glob::glob_state().getr_vfs();

    std::string yaml_text;
    if (!vfs.read_string(id, yaml_text))
    {
        ALOG_ERROR("read_container: failed to read {}", id.str());
        return false;
    }

    try
    {
        container = YAML::Load(yaml_text);
    }
    catch (const std::exception& e)
    {
        ALOG_ERROR("read_container failed {} {}", e.what(), id.str());
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
write_container(const vfs::rid& id, const serialization::container& container)
{
    auto& vfs = glob::glob_state().getr_vfs();

    std::stringstream ss;
    ss << container;
    auto content = ss.str();

    if (!vfs.write_string(id, content))
    {
        ALOG_ERROR("write_container: failed to write {}", id.str());
        return false;
    }

    return true;
}

}  // namespace serialization
}  // namespace kryga