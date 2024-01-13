#pragma once

#include <ar/ar_defines.h>

#include <glm_unofficial/glm.h>

namespace agea
{
namespace core
{

AGEA_ar_struct();
struct color : ::glm::vec4
{
    glm::vec4 m_data;
};

}  // namespace core
}  // namespace agea