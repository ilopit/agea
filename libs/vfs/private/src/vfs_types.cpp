#include "vfs/vfs_types.h"

#include <filesystem>

namespace kryga
{

temp_dir_context::~temp_dir_context()
{
    if (!m_folder.empty())
    {
        std::filesystem::remove_all(m_folder.fs());
    }
}

}  // namespace kryga
