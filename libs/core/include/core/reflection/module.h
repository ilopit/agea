#pragma once

#include <utils/id.h>
#include <utils/path.h>
#include <utils/singleton_instance.h>

#include <unordered_map>

namespace agea
{
namespace reflection
{
class module
{
public:
    module(const ::agea::utils::id& id) :m_id(id)
    {
    }

    virtual ~module()
    {
    }

    virtual bool
    init_reflection() = 0;

    virtual bool
    override_reflection_types() = 0;

    const agea::utils::id&
    get_id() const
    {
        return m_id;
    }

private:
    agea::utils::id m_id;
};

class module_manager
{
public:
    module_manager();
    ~module_manager();

    module*
    get_module(const agea::utils::id& id);

    template <typename module_t>
    bool
    register_module()
    {
        return register_module(module_t::instance().get_id(), module_t::instance());
    }

    std::vector<module*>&
    modules();

protected:
    bool
    register_module(const agea::utils::id& id, module& m);

    std::vector<module*> m_modules;
};

}  // namespace reflection

namespace glob
{
struct module_manager
    : public ::agea::singleton_instance<::agea::reflection::module_manager, module_manager>
{
};
}  // namespace glob
}  // namespace agea
