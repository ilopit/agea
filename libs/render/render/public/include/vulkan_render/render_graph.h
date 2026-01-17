#pragma once

#include <utils/id.h>
#include <utils/check.h>

#include <vector>
#include <functional>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <memory>

namespace kryga::render
{
enum class rg_resource_usage
{
    read = 0,
    write,
    read_write
};

// Pass type - determines synchronization behavior
enum class rg_pass_type
{
    graphics = 0,
    compute,
    transfer
};

// Resource type
enum class rg_resource_type
{
    buffer = 0,
    image
};

// Resource handle for graph-managed resources
struct rg_resource
{
    utils::id name;
    rg_resource_type type = rg_resource_type::image;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t format = 0;  // VkFormat or similar
    bool is_imported = false;
};

// Pass resource reference
struct rg_resource_ref
{
    utils::id resource;
    rg_resource_type type = rg_resource_type::buffer;
    rg_resource_usage usage;
};

// Pass declaration
struct render_path
{
    utils::id name;
    std::vector<rg_resource_ref> resources;
    std::function<void()> execute;
    rg_pass_type type = rg_pass_type::graphics;
    uint32_t order = 0;  // Computed during compile
};

class render_graph
{
public:
    render_graph() = default;

    // Create a transient image resource (lifetime managed by graph)
    rg_resource&
    create_resource(const utils::id& name, uint32_t width, uint32_t height, uint32_t format)
    {
        if (m_compiled)
            throw std::runtime_error("Cannot modify compiled graph");

        auto& res = m_resources[name];
        res.name = name;
        res.type = rg_resource_type::image;
        res.width = width;
        res.height = height;
        res.format = format;
        res.is_imported = false;
        return res;
    }

    // Create a transient buffer resource
    rg_resource&
    create_buffer(const utils::id& name, uint32_t size)
    {
        if (m_compiled)
            throw std::runtime_error("Cannot modify compiled graph");

        auto& res = m_resources[name];
        res.name = name;
        res.type = rg_resource_type::buffer;
        res.width = size;  // Use width as size for buffers
        res.height = 1;
        res.depth = 1;
        res.is_imported = false;
        return res;
    }

    // Import external resource (swapchain, persistent buffers)
    rg_resource&
    import_resource(const utils::id& name)
    {
        KRG_check(!m_compiled, "Cannot modify compiled graph");

        auto& res = m_resources[name];
        res.name = name;
        res.is_imported = true;
        return res;
    }

    // ========================================================================
    // Pass management
    // ========================================================================

    // Add a render pass
    void
    add_pass(const utils::id& name,
             const std::vector<rg_resource_ref>& resources,
             std::function<void()> execute,
             rg_pass_type type = rg_pass_type::graphics)
    {
        KRG_check(!m_compiled, "Cannot modify compiled graph");

        render_path pass;
        pass.name = name;
        pass.resources = resources;
        pass.execute = std::move(execute);
        pass.type = type;
        m_passes[name] = std::move(pass);
    }

    // Helper to create resource refs
    static rg_resource_ref
    read(const utils::id& name, rg_resource_type type = rg_resource_type::buffer)
    {
        return {name, type, rg_resource_usage::read};
    }

    static rg_resource_ref
    write(const utils::id& name, rg_resource_type type = rg_resource_type::buffer)
    {
        return {name, type, rg_resource_usage::write};
    }

    static rg_resource_ref
    read_write(const utils::id& name, rg_resource_type type = rg_resource_type::buffer)
    {
        return {name, type, rg_resource_usage::read_write};
    }

    // Compile: topological sort, validate dependencies
    bool
    compile()
    {
        if (m_compiled)
        {
            return true;
        }

        // Validate all resource refs exist
        for (const auto& [pass_id, pass] : m_passes)
        {
            for (const auto& ref : pass.resources)
            {
                if (m_resources.find(ref.resource) == m_resources.end())
                {
                    m_error = "Pass '" + std::string(pass.name.cstr()) +
                              "' references unknown resource '" + std::string(ref.resource.cstr()) +
                              "'";
                    return false;
                }
            }
        }

        // Build dependency graph
        // A pass depends on another if it reads a resource that the other writes
        std::unordered_map<utils::id, std::vector<utils::id>> writers;  // resource -> pass ids that
                                                                        // write it
        std::unordered_map<utils::id, std::vector<utils::id>> readers;  // resource -> pass ids that
                                                                        // read it

        for (const auto& [pass_id, pass] : m_passes)
        {
            for (const auto& ref : pass.resources)
            {
                if (ref.usage == rg_resource_usage::write ||
                    ref.usage == rg_resource_usage::read_write)
                {
                    writers[ref.resource].push_back(pass_id);
                }
                if (ref.usage == rg_resource_usage::read ||
                    ref.usage == rg_resource_usage::read_write)
                {
                    readers[ref.resource].push_back(pass_id);
                }
            }
        }

        // Build adjacency list (dependencies): pass_id -> list of pass_ids it depends on
        std::unordered_map<utils::id, std::vector<utils::id>> deps;
        for (const auto& [resource, reader_ids] : readers)
        {
            auto it = writers.find(resource);
            if (it != writers.end())
            {
                for (const auto& reader_id : reader_ids)
                {
                    for (const auto& writer_id : it->second)
                    {
                        if (reader_id != writer_id)
                        {
                            deps[reader_id].push_back(writer_id);
                        }
                    }
                }
            }
        }

        // Topological sort (Kahn's algorithm)
        std::unordered_map<utils::id, size_t> in_degree;
        std::unordered_map<utils::id, std::vector<utils::id>> dependents;

        // Initialize in_degree for all passes
        for (const auto& [pass_id, pass] : m_passes)
        {
            in_degree[pass_id] = deps[pass_id].size();
            for (const auto& dep_id : deps[pass_id])
            {
                dependents[dep_id].push_back(pass_id);
            }
        }

        std::vector<utils::id> queue;
        for (const auto& [pass_id, degree] : in_degree)
        {
            if (degree == 0)
            {
                queue.push_back(pass_id);
            }
        }

        m_execution_order.clear();
        while (!queue.empty())
        {
            utils::id curr = queue.back();
            queue.pop_back();
            m_execution_order.push_back(curr);

            for (const auto& dependent_id : dependents[curr])
            {
                in_degree[dependent_id]--;
                if (in_degree[dependent_id] == 0)
                {
                    queue.push_back(dependent_id);
                }
            }
        }

        if (m_execution_order.size() != m_passes.size())
        {
            m_error = "Cycle detected in render graph";
            return false;
        }

        // Assign order to passes
        for (size_t i = 0; i < m_execution_order.size(); ++i)
        {
            m_passes[m_execution_order[i]].order = static_cast<uint32_t>(i);
        }

        m_compiled = true;
        return true;
    }

    // Execute all passes in order
    bool
    execute()
    {
        if (!m_compiled)
        {
            if (!compile())
            {
                return false;
            }
        }

        for (const auto& pass_id : m_execution_order)
        {
            auto it = m_passes.find(pass_id);
            if (it != m_passes.end() && it->second.execute)
            {
                it->second.execute();
            }
        }
    }

    // Reset graph for reuse
    void
    reset()
    {
        m_resources.clear();
        m_passes.clear();
        m_execution_order.clear();
        m_compiled = false;
        m_error.clear();
    }

    // Accessors
    bool
    is_compiled() const
    {
        return m_compiled;
    }

    const std::string&
    get_error() const
    {
        return m_error;
    }

    const std::vector<utils::id>&
    get_execution_order() const
    {
        return m_execution_order;
    }

    const std::unordered_map<utils::id, render_path>&
    get_passes() const
    {
        return m_passes;
    }

    const render_path*
    get_pass(const utils::id& name) const
    {
        auto it = m_passes.find(name);
        if (it != m_passes.end())
            return &it->second;
        return nullptr;
    }

    size_t
    get_pass_count() const
    {
        return m_passes.size();
    }

    size_t
    get_resource_count() const
    {
        return m_resources.size();
    }

private:
    std::unordered_map<utils::id, rg_resource> m_resources;
    std::unordered_map<utils::id, render_path> m_passes;
    std::vector<utils::id> m_execution_order;
    bool m_compiled = false;
    std::string m_error;
};

}  // namespace kryga::render
