#pragma once

#include "model/model_minimal.h"
#include "model/model_fwds.h"

#include <functional>

namespace agea
{
namespace model
{
struct conteiner_loader
{
    using obj_loader =
        std::function<smart_object*(const utils::path& path, object_constructor_context& occ)>;

    using obj_saver = std::function<bool(const smart_object& obj, const utils::path& object_path)>;

    static bool
    load_objects_conteiners(architype id,
                            obj_loader l,
                            const utils::path& folder_path,
                            object_constructor_context& occ);

    static bool
    save_objects_conteiners(architype id,
                            obj_saver s,
                            const utils::path& folder_path,
                            cache_set_ref obj_set);
};
}  // namespace model
}  // namespace agea
