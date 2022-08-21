#pragma once

#include <utils/id.h>
#include <utils/path.h>

#include <serialization/serialization_fwds.h>

#include <unordered_map>

namespace agea
{
    namespace model
    {
        class object_mapping
        {
            public:
            bool buiild_object_mapping(const utils::path& p);

            bool buiild_object_mapping(serialization::conteiner& c, bool is_class);

            std::unordered_map<utils::id,std::pair<bool, utils::path>> m_items;
        };
    }
}