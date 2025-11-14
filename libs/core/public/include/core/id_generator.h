#pragma once

#include <unordered_map>

#include <utils/id.h>
#include <utils/singleton_instance.h>

namespace agea
{
namespace core
{
class id_generator
{
    struct name_counter
    {
        uint32_t ctr = 1;
    };

public:
    utils::id
    generate(const utils::id& obj_id);

public:
    std::unordered_map<utils::id, name_counter> m_mapping;
};

}  // namespace core

}  // namespace agea