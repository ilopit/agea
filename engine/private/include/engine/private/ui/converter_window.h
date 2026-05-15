#pragma once

#include "engine/ui.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kryga::ui
{

struct converter_result
{
    bool success = false;
    int exit_code = 0;
    std::string log_tail;
};

using converter_completed_callback = std::function<void(const converter_result&)>;

class converter_window : public window
{
public:
    static const char*
    window_title()
    {
        return "Asset Converter";
    }

    converter_window();
    ~converter_window();

    void
    handle() override;

    void
    set_completed_callback(converter_completed_callback cb);

    bool
    submit_conversion(const std::string& input,
                      const std::string& output_root,
                      const std::string& name,
                      const std::string& mode,
                      const std::string& existing_package,
                      const std::vector<std::string>& deps);

    void
    poll();

    bool
    is_running() const;

    std::string
    get_status_text() const;

    bool
    is_status_error() const;

    static std::vector<std::pair<std::string, bool>>
    list_deps(const std::string& output_root);

private:
    std::array<char, 512> m_input_path;
    std::array<char, 512> m_output_path;
    std::array<char, 128> m_name;
    std::array<char, 128> m_existing_package;
    int m_mode_index = 0;
    std::string m_status;
    bool m_status_is_error = false;

    std::vector<std::pair<std::string, bool>> m_dep_checklist;
    bool m_deps_initialized = false;
    std::string m_last_scanned_output_root;

    struct async_state;
    std::unique_ptr<async_state> m_async;

    converter_completed_callback m_completed_callback;
};

}  // namespace kryga::ui
