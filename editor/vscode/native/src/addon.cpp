// Kryga editor native addon (N-API).
//
// Exposed JS API:
//
//   open(name: string, sizeBytes: number): number
//     Attach to the shared-memory region published by the engine. Returns a
//     numeric handle used by the rest of the API. Returns -1 on failure.
//
//   close(handle: number): void
//
//   readHeader(handle: number): object
//     Snapshot of the mutable + immutable header fields. Fields named in
//     camelCase to match the TypeScript surface.
//
//   getSlotBuffer(handle: number, slotIndex: number): Buffer
//     Zero-copy view of the chosen slot's pixel bytes. The buffer is backed
//     by the mmap; the consumer must not keep it alive across resize /
//     generation bumps.
//
//   claimSlot(handle: number, slotIndex: number): void
//   releaseSlot(handle: number): void
//     Publisher-protocol hooks: setting `reading_slot` so the engine avoids
//     writing into the slot being read.
//
// No signaling yet — Phase 1 is polling from JS.

#include <node_api.h>

#include "editor_ipc/frame_protocol.h"
#include "editor_ipc/shared_memory.h"

#include <memory>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Handle table. JS side receives small integer handles that index into this
// map; keeps the TypeScript surface simple and makes the addon stateless
// across re-requires.
// ---------------------------------------------------------------------------

namespace
{
using kryga::editor_ipc::frame_header;
using kryga::editor_ipc::shared_memory;
using kryga::editor_ipc::SHM_MAGIC;
using kryga::editor_ipc::SHM_VERSION;
using kryga::editor_ipc::NUM_SLOTS;

struct entry
{
    shared_memory shm;
};

int g_next_handle = 1;
std::unordered_map<int, std::unique_ptr<entry>> g_table;

entry*
find_entry(int handle)
{
    auto it = g_table.find(handle);
    return (it == g_table.end()) ? nullptr : it->second.get();
}

frame_header*
header_of(entry& e)
{
    return static_cast<frame_header*>(e.shm.data());
}
}  // namespace

// ---------------------------------------------------------------------------
// JS bindings via raw node_api so the addon has no node-addon-api build dep.
// ---------------------------------------------------------------------------

#define NAPI_CHECK(call)                                                              \
    do                                                                                \
    {                                                                                 \
        napi_status _s = (call);                                                      \
        if (_s != napi_ok)                                                            \
        {                                                                             \
            napi_throw_error(env, nullptr, "kryga_native: N-API call failed");        \
            return nullptr;                                                           \
        }                                                                             \
    } while (0)

static napi_value
js_open(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    if (argc < 2)
    {
        napi_throw_type_error(env, nullptr, "open(name, sizeBytes) requires 2 args");
        return nullptr;
    }

    char name_buf[256];
    size_t name_len = 0;
    NAPI_CHECK(napi_get_value_string_utf8(env, args[0], name_buf, sizeof(name_buf), &name_len));

    int64_t size_bytes = 0;
    NAPI_CHECK(napi_get_value_int64(env, args[1], &size_bytes));

    auto e = std::make_unique<entry>();
    if (!e->shm.open({name_buf, name_len}, shared_memory::mode::attach,
                     static_cast<size_t>(size_bytes)))
    {
        // Return -1 rather than throwing; the extension polls while the
        // engine isn't up yet.
        napi_value minus_one;
        NAPI_CHECK(napi_create_int32(env, -1, &minus_one));
        return minus_one;
    }

    // Validate magic / version before trusting the header.
    auto* h = header_of(*e);
    if (!h || h->magic != SHM_MAGIC || h->version != SHM_VERSION)
    {
        napi_value minus_one;
        NAPI_CHECK(napi_create_int32(env, -1, &minus_one));
        return minus_one;
    }

    int handle = g_next_handle++;
    g_table[handle] = std::move(e);

    // Signal consumer_attached to the publisher. Publisher uses this for
    // "does anyone care" liveness heuristics.
    h->consumer_attached.store(1, std::memory_order_release);

    napi_value result;
    NAPI_CHECK(napi_create_int32(env, handle, &result));
    return result;
}

static napi_value
js_close(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));

    auto it = g_table.find(handle);
    if (it != g_table.end())
    {
        if (auto* h = header_of(*it->second))
        {
            h->consumer_attached.store(0, std::memory_order_release);
        }
        g_table.erase(it);
    }

    napi_value undef;
    NAPI_CHECK(napi_get_undefined(env, &undef));
    return undef;
}

static napi_value
js_read_header(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));

    auto* e = find_entry(handle);
    if (!e)
    {
        napi_throw_error(env, nullptr, "kryga_native: invalid handle");
        return nullptr;
    }
    auto* h = header_of(*e);

    napi_value obj;
    NAPI_CHECK(napi_create_object(env, &obj));

    auto set_u32 = [&](const char* key, uint32_t value)
    {
        napi_value v;
        napi_create_uint32(env, value, &v);
        napi_set_named_property(env, obj, key, v);
    };
    auto set_u64 = [&](const char* key, uint64_t value)
    {
        napi_value v;
        // BigInt — frame_counter is 64-bit to avoid the Number precision cliff
        // at 2^53 after a few days of streaming.
        napi_create_bigint_uint64(env, value, &v);
        napi_set_named_property(env, obj, key, v);
    };

    set_u32("magic", h->magic);
    set_u32("version", h->version);
    set_u32("maxWidth", h->max_width);
    set_u32("maxHeight", h->max_height);
    set_u32("pixelFormat", h->pixel_format);
    set_u32("strideBytes", h->stride_bytes);
    set_u32("numSlots", h->num_slots);
    set_u32("slotBytes", h->slot_bytes);
    set_u32("generation", h->generation.load(std::memory_order_acquire));
    set_u64("frameCounter", h->frame_counter.load(std::memory_order_acquire));
    set_u32("latestReadySlot", h->latest_ready_slot.load(std::memory_order_acquire));
    set_u32("readingSlot", h->reading_slot.load(std::memory_order_acquire));
    set_u32("currentWidth", h->current_width.load(std::memory_order_acquire));
    set_u32("currentHeight", h->current_height.load(std::memory_order_acquire));
    set_u32("publisherAlive", h->publisher_alive.load(std::memory_order_acquire));
    set_u32("consumerAttached", h->consumer_attached.load(std::memory_order_acquire));

    return obj;
}

static napi_value
js_get_slot_buffer(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0, slot = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));
    NAPI_CHECK(napi_get_value_int32(env, args[1], &slot));

    auto* e = find_entry(handle);
    if (!e || slot < 0 || static_cast<uint32_t>(slot) >= NUM_SLOTS)
    {
        napi_throw_error(env, nullptr, "kryga_native: bad handle or slot");
        return nullptr;
    }
    auto* h = header_of(*e);
    auto* base = static_cast<uint8_t*>(e->shm.data());

    // Zero-copy — the Buffer is a view into the mmap. Finalizer is a no-op;
    // the mmap is owned by the handle, not the Buffer.
    napi_value buffer;
    NAPI_CHECK(napi_create_external_buffer(env,
                                           h->slot_bytes,
                                           base + h->slot_offsets[slot],
                                           /*finalize_cb*/ nullptr,
                                           /*finalize_hint*/ nullptr,
                                           &buffer));
    return buffer;
}

static napi_value
js_claim_slot(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0, slot = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));
    NAPI_CHECK(napi_get_value_int32(env, args[1], &slot));

    if (auto* e = find_entry(handle))
    {
        header_of(*e)->reading_slot.store(
            static_cast<uint32_t>(slot), std::memory_order_release);
    }
    napi_value undef;
    NAPI_CHECK(napi_get_undefined(env, &undef));
    return undef;
}

static napi_value
js_release_slot(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));

    if (auto* e = find_entry(handle))
    {
        header_of(*e)->reading_slot.store(
            kryga::editor_ipc::SLOT_NONE, std::memory_order_release);
    }
    napi_value undef;
    NAPI_CHECK(napi_get_undefined(env, &undef));
    return undef;
}

NAPI_MODULE_INIT()
{
    napi_property_descriptor props[] = {
        {"open", nullptr, js_open, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"close", nullptr, js_close, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"readHeader", nullptr, js_read_header, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getSlotBuffer", nullptr, js_get_slot_buffer, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"claimSlot", nullptr, js_claim_slot, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"releaseSlot", nullptr, js_release_slot, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(props) / sizeof(*props), props);
    return exports;
}
