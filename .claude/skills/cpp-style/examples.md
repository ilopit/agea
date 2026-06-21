# Kryga C++ — canonical examples

Good vs. bad pairs only. Rules and rationale live in `rules.md`. When reviewing,
match code against the GOOD form; flag the BAD form with the GOOD as the fix.

Snippets follow the project clang-format: Allman braces, return type on its own
line, 4-space indent, access specifiers at column 0. `...` marks elided code.

## Error handling — assert, don't defend

```cpp
// BAD — defensive null-check / early return
mesh*
get_mesh(const utils::id& id)
{
    auto* m = cache.find(id);
    if (!m)
    {
        return nullptr;  // silently swallows a programmer error
    }
    return m;
}

// GOOD — invariant asserted with KRG_check
mesh*
get_mesh(const utils::id& id)
{
    auto* m = cache.find(id);
    KRG_check(m, "mesh not found in cache");
    return m;
}
```

```cpp
// BAD — unreachable handled by returning
switch (state)
{
case a:
    ...;
default:
    return;
}

// GOOD
switch (state)
{
case a:
    ...;
default:
    KRG_never("bad state");
}
```

## Naming

```cpp
// BAD
class MeshComponent
{
    int VertexCount;           // PascalCase member, no prefix
    static int InstanceCount;  // static without s_

public:
    void
    SetVisible(bool V);        // PascalCase fn + param
};

// GOOD
class mesh_component
{
    int m_vertex_count;           // m_ + snake_case
    static int s_instance_count;  // s_ for statics

public:
    void
    set_visible(bool visible);
};
```

```cpp
// GOOD — intentional name__suffix for an internal/guarded variant (do NOT flag)
result_code
property_handler__copy_guarded(...);

struct state_mutator__core
{
    ...
};
```

## Includes — manual order, sorting only inside a category

```cpp
// GOOD (SortIncludes is OFF; keep this order, never alphabetize across groups)
#include "core/level.h"  // local, quoted, first
#include "packages/base/model/x.h"

#include <kryga_port/format.h>  // engine/thirdparty, angle brackets
#include <utils/check.h>

#include <unordered_map>  // std last
#include <vector>
```

```cpp
// BAD
#ifndef CORE_LEVEL_H  // include guard instead of #pragma once
#define CORE_LEVEL_H
#include <vector>      // std first, local mixed in below
#include "core/level.h"
```

## Class layout

```cpp
// GOOD — public -> friend -> protected -> private; override on virtuals
class level
{
public:
    level();

    void
    load();

    friend struct state_mutator__core;

protected:
    void
    on_loaded() override;

private:
    std::unordered_map<utils::id, object> m_objects;  // refs/maps first
    level_state m_state;                              // primitives after
};
```

## Namespaces

```cpp
// GOOD — nested form with closing-brace comments (intentional)
namespace kryga::core
{
...
}  // namespace kryga::core
```

## C++23 idioms

```cpp
// BAD
std::string s = std::format("{}", id.str().c_str());  // redundant c_str
const std::vector<std::string_view>& deps();          // copyable view by const-ref
if (runtime_bool_for_compile_time_trait)
{
    ...
}

// GOOD
std::string s = std::format("{}", id.str());
std::span<const std::string_view> deps();
if constexpr (std::is_pointer_v<T>)
{
    ...
}
auto cfg = smart_object_flags{.instance_obj = false, .derived_obj = true};
```

## Global state access

```cpp
// BAD — nullable getter then manual check
auto* model = glob::glob_state().get_model();
if (model)
{
    model->caches...;
}

// GOOD — getr_* asserts non-null
glob::glob_state().getr_model().caches.objects.has_item(id);
```
