#include "core/lightmap_manifest.h"

#include <serialization/serialization.h>
#include <utils/kryga_log.h>

namespace kryga
{
namespace core
{

bool
lightmap_manifest::save(const vfs::rid& id) const
{
    serialization::container root;
    root["atlas_width"] = atlas_width;
    root["atlas_height"] = atlas_height;

    for (auto& [obj_id, entry] : objects)
    {
        serialization::container obj;
        obj["id"] = obj_id.str();
        obj["region_x"] = entry.region_x;
        obj["region_y"] = entry.region_y;
        obj["region_w"] = entry.region_w;
        obj["region_h"] = entry.region_h;
        obj["scale_x"] = entry.lightmap_scale.x;
        obj["scale_y"] = entry.lightmap_scale.y;
        obj["offset_x"] = entry.lightmap_offset.x;
        obj["offset_y"] = entry.lightmap_offset.y;
        root["objects"].push_back(obj);
    }

    return serialization::write_container(id, root);
}

bool
lightmap_manifest::load(const vfs::rid& id)
{
    serialization::container root;
    if (!serialization::read_container(id, root))
    {
        return false;
    }

    atlas_width = root["atlas_width"].as<uint32_t>(0);
    atlas_height = root["atlas_height"].as<uint32_t>(0);

    if (root["objects"])
    {
        for (const auto& obj : root["objects"])
        {
            auto id_str = obj["id"].as<std::string>("");
            if (id_str.empty())
                continue;

            lightmap_object_entry entry;
            entry.region_x = obj["region_x"].as<uint32_t>(0);
            entry.region_y = obj["region_y"].as<uint32_t>(0);
            entry.region_w = obj["region_w"].as<uint32_t>(0);
            entry.region_h = obj["region_h"].as<uint32_t>(0);
            entry.lightmap_scale.x = obj["scale_x"].as<float>(0.0f);
            entry.lightmap_scale.y = obj["scale_y"].as<float>(0.0f);
            entry.lightmap_offset.x = obj["offset_x"].as<float>(0.0f);
            entry.lightmap_offset.y = obj["offset_y"].as<float>(0.0f);

            objects[AID(id_str.c_str())] = entry;
        }
    }

    ALOG_INFO("lightmap_manifest: loaded {}x{} atlas with {} entries",
              atlas_width, atlas_height, objects.size());
    return true;
}

const lightmap_object_entry*
lightmap_manifest::find_entry(const utils::id& component_id) const
{
    auto it = objects.find(component_id);
    return it != objects.end() ? &it->second : nullptr;
}

}  // namespace core
}  // namespace kryga
