#pragma once

#include "core/agea_minimal.h"
#include "core/id.h"

#include "model/model_fwds.h"
#include "model/caches/cache_set.h"
#include "model/caches/line_cache.h"

namespace agea
{
namespace model
{
struct conteiner_loader
{
    static bool
    load_objects_conteiners(architype id,
                            bool is_class,
                            const utils::path& folder_path,
                            object_constructor_context& occ);

    static bool
    save_objects_conteiners(architype id,
                            bool is_class,
                            const utils::path& folder_path,
                            cache_set_ref class_objects_cs_ref,
                            cache_set_ref objects_cs_ref);
};
}  // namespace model
}  // namespace agea
