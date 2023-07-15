#pragma once

#include <utils/path.h>
#include <utils/id.h>

namespace agea
{
namespace assets_importer
{

bool
convert_3do_to_amsh(const utils::path& obj_path,
                    const utils::path& dst_folder_path,
                    const utils::id& mesh_id);

bool
convert_imager_to_atxt(const utils::path& obj_path,
                       const utils::path& dst_folder_path,
                       const utils::id& texture_id);

}  // namespace assets_importer
}  // namespace agea