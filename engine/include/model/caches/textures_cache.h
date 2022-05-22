#pragma once

#include "core/agea_minimal.h"

#include "model/rendering/texture.h"

namespace agea
{
namespace model
{
class textures_cache
{
public:
    void
    init();

    std::shared_ptr<model::texture>
    get(const std::string& id);

    std::unordered_map<std::string, std::shared_ptr<model::texture>> m_textures;
};

}  // namespace model

namespace glob
{
struct textures_cache : public weird_singleton<::agea::model::textures_cache>
{
};
}  // namespace glob

}  // namespace agea