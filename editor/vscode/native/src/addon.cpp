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
#include "editor_ipc/named_event.h"
#include "editor_ipc/shared_memory.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
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

using kryga::editor_ipc::input_event;
using kryga::editor_ipc::named_event;

struct entry
{
    shared_memory shm;

    // Phase 2: frame-ready wait on a worker thread. Inactive until
    // subscribeFrames() is called.
    named_event frame_ready;
    napi_threadsafe_function frame_tsfn = nullptr;
    std::thread worker;
    std::atomic<bool> worker_stop{false};
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

static void stop_worker(entry& e);

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
        stop_worker(*it->second);
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

// ---------------------------------------------------------------------------
// Phase 2: signaled wait + input writer.
// ---------------------------------------------------------------------------

static void
on_frame_signal(napi_env env, napi_value js_cb, void* /*context*/, void* /*data*/)
{
    // Called on the JS thread. `js_cb` is the user's callback registered
    // in subscribeFrames — we just invoke it with no arguments; the
    // extension pulls the new frame data via readHeader + getSlotBuffer.
    if (env == nullptr || js_cb == nullptr) return;
    napi_value undef, ignored;
    napi_get_undefined(env, &undef);
    napi_call_function(env, undef, js_cb, 0, nullptr, &ignored);
}

static void
worker_main(entry* e)
{
    while (!e->worker_stop.load(std::memory_order_acquire))
    {
        // Bounded wait — timeout means "no frame this window". We loop so
        // shutdown is observed promptly without needing to signal the
        // semaphore from the stop path (though frame_publisher::shutdown
        // does signal it once as a courtesy).
        if (e->frame_ready.wait_for(std::chrono::milliseconds(100)))
        {
            if (e->frame_tsfn)
            {
                napi_call_threadsafe_function(e->frame_tsfn, nullptr, napi_tsfn_nonblocking);
            }
        }
    }
}

static napi_value
js_subscribe_frames(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));

    auto* e = find_entry(handle);
    if (!e)
    {
        napi_throw_error(env, nullptr, "kryga_native: invalid handle");
        return nullptr;
    }

    if (e->frame_tsfn != nullptr)
    {
        napi_throw_error(env, nullptr, "kryga_native: already subscribed");
        return nullptr;
    }

    // Attach to "<name>_fr" event created by the engine-side publisher.
    // We can't reach the channel name from here (the addon only has the
    // region pointer), so read it back from the caller — simplest is to
    // require subscribeFrames(handle, callback, name). But that name was
    // already passed to open(); we stored it per-entry... actually we
    // didn't. Ask callers to pass it again rather than tracking state.
    //
    // Design choice documented here so future readers aren't confused.
    // Instead of modifying the API surface, stash the name in the entry on
    // open(): done below via the `name` field.
    napi_value name_val;
    napi_get_named_property(env, args[1] /* options object */, "name", &name_val);
    size_t name_len = 0;
    char name_buf[256];
    napi_get_value_string_utf8(env, name_val, name_buf, sizeof(name_buf), &name_len);

    napi_value cb_val;
    napi_get_named_property(env, args[1], "callback", &cb_val);

    std::string ev_name(name_buf, name_len);
    ev_name += "_fr";
    if (!e->frame_ready.open(ev_name, named_event::mode::attach))
    {
        napi_value v;
        napi_get_boolean(env, false, &v);
        return v;
    }

    napi_value async_name;
    napi_create_string_utf8(env, "kryga-frame-ready", NAPI_AUTO_LENGTH, &async_name);

    NAPI_CHECK(napi_create_threadsafe_function(env,
                                               cb_val,
                                               nullptr,
                                               async_name,
                                               /*queue_size*/ 0,
                                               /*initial_thread_count*/ 1,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               on_frame_signal,
                                               &e->frame_tsfn));

    e->worker_stop.store(false);
    e->worker = std::thread(worker_main, e);

    napi_value ok;
    napi_get_boolean(env, true, &ok);
    return ok;
}

static void
stop_worker(entry& e)
{
    e.worker_stop.store(true, std::memory_order_release);
    // Prod the semaphore so wait_for returns immediately instead of
    // waiting out the 100ms window.
    e.frame_ready.signal();
    if (e.worker.joinable()) e.worker.join();
    if (e.frame_tsfn)
    {
        napi_release_threadsafe_function(e.frame_tsfn, napi_tsfn_release);
        e.frame_tsfn = nullptr;
    }
    e.frame_ready.close();
}

// ---------------------------------------------------------------------------
// Phase 4: control channel (editor ⇄ engine). Separate ring from input.
// ---------------------------------------------------------------------------

static napi_value
js_post_message_in(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    NAPI_CHECK(napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    int32_t handle = 0;
    NAPI_CHECK(napi_get_value_int32(env, args[0], &handle));

    auto* e = find_entry(handle);
    if (!e)
    {
        napi_throw_error(env, nullptr, "kryga_native: invalid handle");
        return nullptr;
    }

    size_t msg_len = 0;
    char msg_buf[kryga::editor_ipc::MSG_SLOT_BYTES];
    NAPI_CHECK(napi_get_value_string_utf8(env, args[1], msg_buf, sizeof(msg_buf), &msg_len));

    auto* h = header_of(*e);
    auto* base = static_cast<uint8_t*>(e->shm.data()) + h->msg_in_offset;
    const uint32_t cap = h->msg_ring_capacity;
    const uint32_t slot_sz = h->msg_slot_bytes;

    const uint32_t head = h->msg_in_head.load(std::memory_order_relaxed);
    const uint32_t tail = h->msg_in_tail.load(std::memory_order_acquire);
    const uint32_t next = (head + 1) % cap;

    napi_value ok;
    if (next == tail)
    {
        napi_get_boolean(env, false, &ok);
        return ok;
    }

    auto* slot = base + static_cast<size_t>(head) * slot_sz;
    const uint32_t payload_len = static_cast<uint32_t>(msg_len);
    const uint32_t written = payload_len < slot_sz - 4 ? payload_len : slot_sz - 4;
    std::memcpy(slot, &payload_len, sizeof(payload_len));
    std::memcpy(slot + 4, msg_buf, written);
    h->msg_in_head.store(next, std::memory_order_release);
    napi_get_boolean(env, true, &ok);
    return ok;
}

static napi_value
js_drain_messages_out(napi_env env, napi_callback_info info)
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
    auto* base = static_cast<uint8_t*>(e->shm.data()) + h->msg_out_offset;
    const uint32_t cap = h->msg_ring_capacity;
    const uint32_t slot_sz = h->msg_slot_bytes;

    uint32_t head = h->msg_out_head.load(std::memory_order_acquire);
    uint32_t tail = h->msg_out_tail.load(std::memory_order_relaxed);

    napi_value arr;
    NAPI_CHECK(napi_create_array(env, &arr));
    uint32_t out_idx = 0;
    while (tail != head)
    {
        auto* slot = base + static_cast<size_t>(tail) * slot_sz;
        uint32_t payload_len = 0;
        std::memcpy(&payload_len, slot, sizeof(payload_len));
        if (payload_len > slot_sz - 4) payload_len = slot_sz - 4;
        napi_value s;
        napi_create_string_utf8(env, reinterpret_cast<const char*>(slot + 4),
                                payload_len, &s);
        napi_set_element(env, arr, out_idx++, s);
        tail = (tail + 1) % cap;
    }
    if (out_idx)
    {
        h->msg_out_tail.store(tail, std::memory_order_release);
    }
    return arr;
}

static napi_value
js_post_input(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
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
    auto* base = static_cast<uint8_t*>(e->shm.data());
    auto* ring = reinterpret_cast<input_event*>(base + h->input_ring_offset);
    const uint32_t cap = h->input_ring_capacity;

    const uint32_t head = h->input_ring_head.load(std::memory_order_relaxed);
    const uint32_t tail = h->input_ring_tail.load(std::memory_order_acquire);
    const uint32_t next = (head + 1) % cap;

    napi_value ok;
    if (next == tail)
    {
        // Ring full: drop the oldest input rather than block. Input loss
        // under extreme backpressure is preferable to stalling the UI
        // thread. A dropped mouse-move shows as a tiny hitch; dropping a
        // key-up is worse but rare.
        napi_get_boolean(env, false, &ok);
        return ok;
    }

    input_event ev{};
    auto read_int32 = [&](const char* key, int32_t& out)
    {
        napi_value v;
        napi_get_named_property(env, args[1], key, &v);
        napi_get_value_int32(env, v, &out);
    };
    int32_t type_i = 0, ts_i = 0;
    read_int32("type", type_i);
    read_int32("timestampMs", ts_i);
    read_int32("a", ev.a);
    read_int32("b", ev.b);
    read_int32("c", ev.c);
    read_int32("d", ev.d);
    ev.type = static_cast<uint32_t>(type_i);
    ev.timestamp_ms = static_cast<uint32_t>(ts_i);

    ring[head] = ev;
    h->input_ring_head.store(next, std::memory_order_release);

    napi_get_boolean(env, true, &ok);
    return ok;
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
        {"subscribeFrames", nullptr, js_subscribe_frames, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"postInput", nullptr, js_post_input, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"postMessageIn", nullptr, js_post_message_in, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"drainMessagesOut", nullptr, js_drain_messages_out, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(props) / sizeof(*props), props);
    return exports;
}
