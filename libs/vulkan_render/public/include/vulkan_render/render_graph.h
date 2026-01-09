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

// Resource usage flags
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

using resource_sptr = std::shared_ptr<rg_resource>;

// Pass resource reference
struct rg_resource_ref
{
    utils::id resource;
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
        m_passes.push_back(std::move(pass));
    }

    // Helper to create resource refs
    static rg_resource_ref
    read(const utils::id& name)
    {
        return {name, rg_resource_usage::read};
    }

    static rg_resource_ref
    write(const utils::id& name)
    {
        return {name, rg_resource_usage::write};
    }

    static rg_resource_ref
    read_write(const utils::id& name)
    {
        return {name, rg_resource_usage::read_write};
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
        for (const auto& pass : m_passes)
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
        std::unordered_map<utils::id, std::vector<size_t>> writers;  // resource -> pass indices
                                                                     // that write it
        std::unordered_map<utils::id, std::vector<size_t>> readers;  // resource -> pass indices
                                                                     // that read it

        for (size_t i = 0; i < m_passes.size(); ++i)
        {
            for (const auto& ref : m_passes[i].resources)
            {
                if (ref.usage == rg_resource_usage::write ||
                    ref.usage == rg_resource_usage::read_write)
                {
                    writers[ref.resource].push_back(i);
                }
                if (ref.usage == rg_resource_usage::read ||
                    ref.usage == rg_resource_usage::read_write)
                {
                    readers[ref.resource].push_back(i);
                }
            }
        }

        // Build adjacency list (dependencies)
        std::vector<std::vector<size_t>> deps(m_passes.size());
        for (const auto& [resource, reader_indices] : readers)
        {
            auto it = writers.find(resource);
            if (it != writers.end())
            {
                for (size_t reader_idx : reader_indices)
                {
                    for (size_t writer_idx : it->second)
                    {
                        if (reader_idx != writer_idx)
                        {
                            deps[reader_idx].push_back(writer_idx);
                        }
                    }
                }
            }
        }

        // Topological sort (Kahn's algorithm)
        std::vector<size_t> in_degree(m_passes.size(), 0);
        for (size_t i = 0; i < m_passes.size(); ++i)
        {
            for (size_t dep : deps[i])
            {
                (void)dep;
                // Each dependency increases in-degree of dependent
            }
        }

        // Recalculate in_degree properly
        std::vector<std::vector<size_t>> dependents(m_passes.size());
        for (size_t i = 0; i < m_passes.size(); ++i)
        {
            for (size_t dep : deps[i])
            {
                dependents[dep].push_back(i);
            }
            in_degree[i] = deps[i].size();
        }

        std::vector<size_t> queue;
        for (size_t i = 0; i < m_passes.size(); ++i)
        {
            if (in_degree[i] == 0)
            {
                queue.push_back(i);
            }
        }

        m_execution_order.clear();
        while (!queue.empty())
        {
            size_t curr = queue.back();
            queue.pop_back();
            m_execution_order.push_back(curr);

            for (size_t dependent : dependents[curr])
            {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0)
                {
                    queue.push_back(dependent);
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
    void
    execute()
    {
        if (!m_compiled)
        {
            if (!compile())
                throw std::runtime_error("Failed to compile render graph: " + m_error);
        }

        for (size_t idx : m_execution_order)
        {
            if (m_passes[idx].execute)
            {
                m_passes[idx].execute();
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

    const std::vector<size_t>&
    get_execution_order() const
    {
        return m_execution_order;
    }

    const std::vector<render_path>&
    get_passes() const
    {
        return m_passes;
    }

    const render_path*
    get_pass(const utils::id& name) const
    {
        for (const auto& pass : m_passes)
        {
            if (pass.name == name)
                return &pass;
        }
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
    std::vector<render_path> m_passes;
    std::vector<size_t> m_execution_order;
    bool m_compiled = false;
    std::string m_error;
};

}  // namespace kryga::render
