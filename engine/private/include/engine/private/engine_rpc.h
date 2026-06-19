#pragma once

// All RPC method registrations for the editor build. Lives in its own TU
// to keep kryga_engine.cpp focused on lifecycle/frame work; handlers
// obtain the engine via glob::glob_state().getr_engine() and the RPC
// server via eng.get_rpc_server() when needed.
//
// All state-touching handlers route through engine's main-thread action
// queue (queue_main_action / wait_main_action) — single-thread ownership
// of engine state, no scattered mutexes.

namespace kryga
{

class vulkan_engine;

namespace rpc
{
class rpc_server;
}

namespace engine_private
{

// Register every JSON-RPC handler on `server`. Called once from
// vulkan_engine::init() under KRG_HAS_EDITOR after the server starts listening.
void
register_rpc_handlers(vulkan_engine& eng, rpc::rpc_server& server);

}  // namespace engine_private
}  // namespace kryga
