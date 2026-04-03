#include "core/objects_mapping.h"

#include <serialization/serialization.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>

#include <utils/kryga_log.h>

namespace kryga
{
namespace core
{

bool
object_mapping::build_object_mapping(const vfs::rid& id)
{
    serialization::container c;
    if (!serialization::read_container(id, c))
    {
        return false;
    }

    if (!build_object_mapping(c, true))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!build_object_mapping(c, false))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

bool
object_mapping::build_object_mapping(serialization::container& c, bool is_class)
{
    auto mapping = c[is_class ? "class_obj_mapping" : "instance_obj_mapping"];
    auto mapping_size = mapping.size();

    for (size_t i = 0; i < mapping_size; ++i)
    {
        auto item = mapping[i];
        for (const auto& ss : item)
        {
            auto f = AID(ss.first.as<std::string>());
            auto s = APATH(ss.second.as<std::string>());
            auto& m = m_items[f];
            m.is_class = is_class;
            m.p = s;
        }
    }
    return true;
}

void
object_mapping::clear()
{
    m_items.clear();
}

object_mapping&
object_mapping::add(const utils::id& id, bool is_class, const utils::path& p)
{
    auto& i = m_items[id];
    i.is_class = is_class;
    i.p = p;

    return *this;
}

bool
object_mapping::build_from_vfs(const vfs::rid& root, bool is_class)
{
    auto& vfs = glob::glob_state().getr_vfs();

    bool ok = vfs.enumerate(
        root,
        [&](std::string_view path, bool is_dir) -> bool
        {
            if (is_dir)
            {
                return true;
            }

            // Extract filename without extension as object id
            auto slash = path.rfind('/');
            auto name = (slash != std::string_view::npos) ? path.substr(slash + 1) : path;
            auto dot = name.rfind('.');
            if (dot == std::string_view::npos)
            {
                return true;
            }

            auto id_str = std::string(name.substr(0, dot));
            auto& m = m_items[AID(id_str)];
            m.is_class = is_class;
            m.p = APATH(std::string(path));

            return true;
        },
        true,    // recursive
        ".aobj"  // extension filter
    );

    return ok;
}

}  // namespace core
}  // namespace kryga
