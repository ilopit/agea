#pragma once

#include <arl/arl_defines.h>

#include <glm_unofficial/glm.h>

namespace agea
{
namespace model
{

AGEA_struct();
struct color : ::glm::vec4
{
    glm::vec4 m_data;
};

}  // namespace model
}  // namespace agea