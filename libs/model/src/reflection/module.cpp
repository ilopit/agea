#include "model/reflection/module.h"

namespace agea
{
agea::glob::module_manager::type agea::glob::module_manager::type::s_instance;

namespace reflection
{

module_manager::module_manager()
{
}

module_manager::~module_manager()
{
}

module*
module_manager::get_module(const utils::id& id)
{
    return nullptr;
}

bool
module_manager::register_module(const utils::id& id, module& m)
{
    m_modules.push_back(&m);

    return true;
}

std::vector<module*>&
module_manager::modules()
{
    return m_modules;
}

}  // namespace reflection
}  // namespace agea