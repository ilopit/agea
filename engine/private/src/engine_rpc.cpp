#include "engine/private/engine_rpc.h"

#include "engine/kryga_engine.h"
#include "engine/editor.h"
#include "engine/editor_system.h"
#include "engine/ui.h"
#include "engine/private/property_rpc.h"
#include "engine/private/ui/bake_editor.h"
#include "engine/private/ui/converter_window.h"
#include "engine/private/ui/material_previewer.h"

#include <rpc/rpc_server.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/model_system.h>
#include <core/package.h>
#include <core/caches/caches_map.h>
#include <core/reflection/lua_api.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>

#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_light_data.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/render_cache.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/render_config.h>
#include <vulkan_render/render_system.h>

#include <global_state/global_state.h>

#include <render_bridge/render_bridge.h>

#include <packages/root/model/assets/asset.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/assets/texture.h>
#include <packages/root/model/assets/sampler.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/components/component.h>
#include <packages/base/model/components/mesh_component.h>

#include <vfs/vfs.h>

#include <utils/kryga_log.h>

#include <sol2_unofficial/sol.h>

#include <json/json.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace kryga::engine_private
{

namespace
{

Json::Value
vec3_json(const glm::vec3& v)
{
    Json::Value a(Json::arrayValue);
    a.append(v.x);
    a.append(v.y);
    a.append(v.z);
    return a;
}

Json::Value
mat4_json(const glm::mat4& m)
{
    Json::Value a(Json::arrayValue);
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            a.append(m[c][r]);
        }
    }
    return a;
}

}  // namespace

// Shader diagnostic protocol: the render layer emits "diagnostics.shader"
// notifications with {diagnostics: [{file, line, column, severity, message}]}
// when shader compilation fails. "diagnostics.clear" clears them on success.
// See shader_compiler.h for the structured error type.

void
register_rpc_handlers(vulkan_engine& eng, rpc::rpc_server& server)
{
    server.on_request("ping",
                      [](const Json::Value& params, Json::Value& result, std::string&)
                      { result = params; });

    server.on_request("engine.waitFrame",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          int count = params.get("count", 1).asInt();
                          if (count < 1)
                          {
                              count = 1;
                          }
                          if (count > 60)
                          {
                              count = 60;
                          }
                          for (int i = 0; i < count; ++i)
                          {
                              if (!eng.wait_main_action([]() {}))
                              {
                                  err = "waitFrame timed out";
                                  return;
                              }
                          }
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    server.on_request(
        "sync.reload",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("path"))
            {
                err = "missing 'path' parameter";
                return;
            }
            std::string path_str = params["path"].asString();
            std::string out;
            bool done = eng.wait_main_action(
                [&]()
                {
                    utils::path p(path_str);
                    std::string name, ext;
                    p.parse_file_name_and_ext(name, ext);

                    if (ext == "lua")
                    {
                        auto lua = glob::glob_state().get_lua();
                        auto lua_r = lua->state().script_file(p.str());
                        out = lua->buffer();
                        lua->reset();
                        if (lua_r.status() != sol::call_status::ok)
                        {
                            sol::error e = lua_r;
                            out += e.what();
                        }
                    }
                    else if (ext == "vert" || ext == "frag" || ext == "comp")
                    {
                        auto& sec = glob::glob_state().getr_model().class_caches.shader_effects;
                        auto ptr = sec.get_item(AID(name));
                        if (!ptr)
                        {
                            out = "shader effect not found: " + name;
                            return;
                        }
                        ptr->mark_render_dirty();
                        auto dep =
                            glob::glob_state().getr_render_bridge().get_dependency().get_node(ptr);
                        for (auto o : dep.m_children)
                        {
                            auto mt = o->as<root::asset>();
                            KRG_check(mt, "shader dep child is not an asset");
                            mt->mark_render_dirty();
                        }
                        // Clear previous diagnostics on successful reload
                        Json::Value clear(Json::objectValue);
                        server.notify("diagnostics.clear", clear);
                    }
                    else if (ext == "glsl")
                    {
                        // Shared includes changed — mark all shader effects dirty.
                        auto& sec = glob::glob_state().getr_model().class_caches.shader_effects;
                        for (auto& [id, obj] : sec.get_items())
                        {
                            auto se = obj->as<root::shader_effect>();
                            if (se)
                            {
                                se->mark_render_dirty();
                            }
                        }
                        auto& dep = glob::glob_state().getr_render_bridge().get_dependency();
                        for (auto& [id, obj] : sec.get_items())
                        {
                            auto node = dep.get_node(obj);
                            for (auto o : node.m_children)
                            {
                                auto mt = o->as<root::asset>();
                                if (mt)
                                {
                                    mt->mark_render_dirty();
                                }
                            }
                        }
                        Json::Value clear(Json::objectValue);
                        server.notify("diagnostics.clear", clear);
                    }
                    else
                    {
                        out = "unsupported file extension: " + ext;
                    }
                },
                std::chrono::seconds(5));
            if (!done)
            {
                err = "sync.reload timed out after 5s";
                return;
            }
            Json::Value r(Json::objectValue);
            r["result"] = out;
            result = r;
        });

    server.on_request("model.selection.get",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          Json::Value r(Json::objectValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto sel =
                                      glob::glob_state().getr_editor_system().editor.get_selected();
                                  r["id"] = sel.valid() ? sel.str() : std::string();
                              });
                          if (!done)
                          {
                              err = "selection.get timed out";
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "model.selection.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            bool done = eng.wait_main_action(
                [&server, id_str]()
                {
                    utils::id new_sel = id_str.empty() ? utils::id() : AID(id_str);
                    glob::glob_state().getr_editor_system().editor.set_selected(new_sel);
                    Json::Value note(Json::objectValue);
                    note["id"] = id_str;
                    server.notify("model.selection.changed", note);
                });
            if (!done)
            {
                err = "selection.set timed out";
                return;
            }
            result = Json::Value(Json::objectValue);
        });

    server.on_request("model.scene.getRoot",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          Json::Value r(Json::objectValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto lvl = glob::glob_state().getr_model().current_level;
                                  if (!lvl)
                                  {
                                      r["level"] = std::string();
                                      r["children"] = Json::Value(Json::arrayValue);
                                      return;
                                  }
                                  r["level"] = lvl->get_id().str();

                                  Json::Value children(Json::arrayValue);
                                  for (const auto& kv : lvl->get_game_objects().get_items())
                                  {
                                      auto* go = kv.second->as<root::game_object>();
                                      Json::Value node(Json::objectValue);
                                      node["id"] = kv.first.str();
                                      node["label"] = kv.first.str();
                                      node["kind"] = "game_object";
                                      bool has_components = go && !go->get_subcomponents().empty();
                                      node["has_children"] = has_components;
                                      if (go && go->get_reflection())
                                      {
                                          node["type_name"] = go->get_reflection()->type_name.str();
                                      }
                                      children.append(std::move(node));
                                  }
                                  r["children"] = std::move(children);
                              });
                          if (!done)
                          {
                              err = "scene.getRoot timed out";
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "model.scene.getChildren",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    r["children"] = Json::Value(Json::arrayValue);
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        return;
                    }

                    auto make_component_node = [](root::component* comp) -> Json::Value
                    {
                        Json::Value node(Json::objectValue);
                        node["id"] = comp->get_id().str();
                        std::string label = comp->get_id().str();
                        if (comp->get_reflection())
                        {
                            label += " (" + comp->get_reflection()->type_name.str() + ")";
                        }
                        node["label"] = label;
                        node["kind"] = "component";
                        node["has_children"] = !comp->get_children().empty();
                        if (comp->get_reflection())
                        {
                            node["type_name"] = comp->get_reflection()->type_name.str();
                        }
                        return node;
                    };

                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj)
                    {
                        return;
                    }

                    Json::Value children(Json::arrayValue);
                    if (auto* go = obj->as<root::game_object>())
                    {
                        for (auto* comp : go->get_subcomponents())
                        {
                            if (comp->get_parent_idx() != root::NO_parent)
                            {
                                continue;
                            }
                            children.append(make_component_node(comp));
                        }
                    }
                    else if (auto* comp = obj->as<root::component>())
                    {
                        for (auto* child : comp->get_children())
                        {
                            children.append(make_component_node(child));
                        }
                    }
                    r["children"] = std::move(children);
                });
            if (!done)
            {
                err = "scene.getChildren timed out";
                return;
            }
            result = r;
        });

    server.on_request(
        "model.scene.create",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("name"))
            {
                err = "missing 'name' parameter";
                return;
            }
            std::string name = params["name"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    root::game_object::construct_params cp;
                    auto* go = lvl->spawn_object<root::game_object>(AID(name), cp);
                    if (!go)
                    {
                        local_err = "spawn_object failed";
                        return;
                    }
                    server.notify("model.scene.changed", Json::Value(Json::objectValue));
                });
            if (!done)
            {
                err = "scene.create timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            Json::Value r(Json::objectValue);
            r["id"] = name;
            result = r;
        });

    server.on_request(
        "model.scene.delete",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    auto* go = lvl->find_game_object(AID(id_str));
                    if (!go)
                    {
                        local_err = "game_object not found: " + id_str;
                        return;
                    }
                    auto& cache = lvl->get_game_objects();
                    cache.remove_item(*go);
                    server.notify("model.scene.changed", Json::Value(Json::objectValue));
                });
            if (!done)
            {
                err = "scene.delete timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
        });

    server.on_request(
        "model.scene.duplicate",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            std::string new_id;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    auto* go = lvl->find_game_object(AID(id_str));
                    if (!go)
                    {
                        local_err = "game_object not found: " + id_str;
                        return;
                    }
                    auto gen_id = glob::glob_state().getr_model().id_gen.generate(AID(id_str));
                    core::spawn_parameters sp;
                    sp.position = go->get_position();
                    auto* clone =
                        lvl->spawn_object_as_clone<root::game_object>(go->get_id(), gen_id, sp);
                    if (!clone)
                    {
                        local_err = "duplicate failed";
                        return;
                    }
                    new_id = gen_id.str();
                    server.notify("model.scene.changed", Json::Value(Json::objectValue));
                });
            if (!done)
            {
                err = "scene.duplicate timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            Json::Value r(Json::objectValue);
            r["id"] = new_id;
            result = r;
        });

    server.on_request(
        "model.scene.rename",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id") || !params.isMember("name"))
            {
                err = "missing 'id' or 'name' parameter";
                return;
            }
            // Rename is not trivially supported in the object model (id is immutable
            // in the cache). Return a clear error until the engine supports it.
            err = "rename not yet supported by the engine object model";
        });

    server.on_request("model.properties.get",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          std::string local_err;
                          Json::Value r;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* lvl = glob::glob_state().getr_model().current_level;
                                  if (!lvl)
                                  {
                                      local_err = "no level loaded";
                                      return;
                                  }
                                  auto* obj = lvl->find_object(AID(id_str));
                                  if (!obj)
                                  {
                                      local_err = "object not found: " + id_str;
                                      return;
                                  }
                                  if (auto* go = obj->as<root::game_object>())
                                  {
                                      r = engine_private::encode_game_object_properties(*go);
                                  }
                                  else
                                  {
                                      r = engine_private::encode_component_properties(
                                          *static_cast<root::component*>(obj));
                                  }
                              });
                          if (!done)
                          {
                              err = "properties.get timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "model.level.list",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    Json::Value arr(Json::arrayValue);
                    auto& vfs = glob::glob_state().getr_vfs();
                    auto p = vfs.real_path(vfs::rid("data", "levels"));
                    if (p && std::filesystem::exists(*p))
                    {
                        const std::string ext = ".alvl";
                        for (const auto& entry : std::filesystem::directory_iterator(*p))
                        {
                            auto name = entry.path().filename().string();
                            if (name.size() > ext.size() &&
                                name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
                            {
                                arr.append(name.substr(0, name.size() - ext.size()));
                            }
                        }
                    }
                    r["levels"] = arr;
                    if (auto* lvl = glob::glob_state().getr_model().current_level)
                    {
                        r["current"] = lvl->get_id().str();
                    }
                    else
                    {
                        r["current"] = std::string();
                    }
                });
            if (!done)
            {
                err = "level.list timed out";
                return;
            }
            result = r;
        });

    server.on_request("model.level.load",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          eng.queue_main_action(
                              [&eng, id_str]()
                              {
                                  if (!eng.load_level(AID(id_str)))
                                  {
                                      ALOG_WARN("level.load: failed to load '{}'", id_str);
                                  }
                              });
                          Json::Value r(Json::objectValue);
                          r["queued"] = true;
                          result = r;
                      });

    server.on_request(
        "engine.getMode",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto mode = glob::glob_state().getr_editor_system().editor.get_mode();
                    r["mode"] = (mode == engine::editor_mode::playing) ? std::string("play")
                                                                       : std::string("edit");
                });
            if (!done)
            {
                err = "engine.getMode timed out";
                return;
            }
            result = r;
        });

    server.on_request("engine.setMode",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("mode"))
                          {
                              err = "missing 'mode' parameter (expect 'edit' or 'play')";
                              return;
                          }
                          std::string m = params["mode"].asString();
                          if (m != "edit" && m != "play")
                          {
                              err = "mode must be 'edit' or 'play', got: " + m;
                              return;
                          }
                          eng.queue_main_action(
                              [m]()
                              {
                                  auto& ge = glob::glob_state().getr_editor_system().editor;
                                  auto current = ge.get_mode();
                                  if (m == "play" && current != engine::editor_mode::playing)
                                  {
                                      ge.enter_play_mode();
                                  }
                                  else if (m == "edit" && current == engine::editor_mode::playing)
                                  {
                                      ge.exit_play_mode();
                                  }
                              });
                          Json::Value r(Json::objectValue);
                          r["queued"] = true;
                          result = r;
                      });

    server.on_request("engine.shutdown",
                      [&eng](const Json::Value&, Json::Value& result, std::string&)
                      {
                          eng.request_shutdown();
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    server.on_request(
        "model.properties.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("owner_id") || !params.isMember("name") ||
                !params.isMember("value"))
            {
                err = "missing 'owner_id'/'name'/'value' parameter";
                return;
            }
            std::string owner_id = params["owner_id"].asString();
            std::string name = params["name"].asString();
            Json::Value value = params["value"];
            eng.queue_main_action(
                [&server, owner_id, name, value]()
                {
                    auto* owner = engine_private::find_owner(owner_id);
                    if (!owner)
                    {
                        ALOG_WARN("properties.set: owner not found: {}", owner_id);
                        return;
                    }
                    Json::Value canonical;
                    std::string set_err =
                        engine_private::write_property(*owner, name, value, canonical);
                    if (!set_err.empty())
                    {
                        ALOG_WARN("properties.set: {}", set_err);
                        return;
                    }
                    Json::Value note(Json::objectValue);
                    note["owner_id"] = owner_id;
                    note["name"] = name;
                    note["value"] = canonical;
                    server.notify("model.properties.changed", note);
                });
            Json::Value r(Json::objectValue);
            r["queued"] = true;
            result = r;
        });

    // =========================================================================
    // Transform API
    // =========================================================================

    server.on_request("model.transform.get",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          std::string local_err;
                          Json::Value r(Json::objectValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* lvl = glob::glob_state().getr_model().current_level;
                                  if (!lvl)
                                  {
                                      local_err = "no level loaded";
                                      return;
                                  }
                                  auto* obj = lvl->find_object(AID(id_str));
                                  if (!obj)
                                  {
                                      local_err = "object not found: " + id_str;
                                      return;
                                  }

                                  root::game_object_component* goc = nullptr;
                                  if (auto* go = obj->as<root::game_object>())
                                  {
                                      goc = go->get_root_component();
                                  }
                                  else if (auto* comp = obj->as<root::component>())
                                  {
                                      goc = dynamic_cast<root::game_object_component*>(comp);
                                  }

                                  if (!goc)
                                  {
                                      local_err = "object has no transform";
                                      return;
                                  }

                                  auto pos = goc->get_position();
                                  auto rot = goc->get_rotation();
                                  auto scl = goc->get_scale();

                                  Json::Value jp(Json::arrayValue);
                                  jp.append(pos.x);
                                  jp.append(pos.y);
                                  jp.append(pos.z);
                                  r["position"] = jp;

                                  Json::Value jr(Json::arrayValue);
                                  jr.append(rot.x);
                                  jr.append(rot.y);
                                  jr.append(rot.z);
                                  r["rotation"] = jr;

                                  Json::Value js(Json::arrayValue);
                                  js.append(scl.x);
                                  js.append(scl.y);
                                  js.append(scl.z);
                                  r["scale"] = js;
                              });
                          if (!done)
                          {
                              err = "transform.get timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "model.transform.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            Json::Value pos_val = params.get("position", Json::nullValue);
            Json::Value rot_val = params.get("rotation", Json::nullValue);
            Json::Value scl_val = params.get("scale", Json::nullValue);
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj)
                    {
                        local_err = "object not found: " + id_str;
                        return;
                    }

                    root::game_object_component* goc = nullptr;
                    if (auto* go = obj->as<root::game_object>())
                    {
                        goc = go->get_root_component();
                    }
                    else if (auto* comp = obj->as<root::component>())
                    {
                        goc = dynamic_cast<root::game_object_component*>(comp);
                    }

                    if (!goc)
                    {
                        local_err = "object has no transform";
                        return;
                    }

                    if (pos_val.isArray() && pos_val.size() == 3)
                    {
                        goc->set_position(
                            {pos_val[0].asFloat(), pos_val[1].asFloat(), pos_val[2].asFloat()});
                    }
                    if (rot_val.isArray() && rot_val.size() == 3)
                    {
                        goc->set_rotation(
                            {rot_val[0].asFloat(), rot_val[1].asFloat(), rot_val[2].asFloat()});
                    }
                    if (scl_val.isArray() && scl_val.size() == 3)
                    {
                        goc->set_scale(
                            {scl_val[0].asFloat(), scl_val[1].asFloat(), scl_val[2].asFloat()});
                    }
                });
            if (!done)
            {
                err = "transform.set timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
        });

    // =========================================================================
    // Component API
    // =========================================================================

    server.on_request("model.component.listTypes",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          Json::Value r(Json::arrayValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto& rm = glob::glob_state().getr_model().reflection;
                                  for (auto& [name, rt] : rm.get_types_to_name())
                                  {
                                      if (rt->arch != core::architype::component)
                                      {
                                          continue;
                                      }
                                      Json::Value entry(Json::objectValue);
                                      entry["type_id"] = rt->type_name.str();
                                      r.append(std::move(entry));
                                  }
                              });
                          if (!done)
                          {
                              err = "component.listTypes timed out";
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "model.component.add",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("object_id") || !params.isMember("type_id"))
            {
                err = "missing 'object_id' or 'type_id' parameter";
                return;
            }
            std::string object_id = params["object_id"].asString();
            std::string type_id = params["type_id"].asString();
            std::string comp_name = params.get("name", type_id).asString();
            std::string local_err;
            std::string new_id;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    auto* go = lvl->find_game_object(AID(object_id));
                    if (!go)
                    {
                        local_err = "game_object not found: " + object_id;
                        return;
                    }
                    auto* parent = go->get_root_component();
                    if (!parent)
                    {
                        local_err = "game_object has no root component";
                        return;
                    }
                    root::component::construct_params cp;
                    auto* comp = go->spawn_component(parent, AID(type_id), AID(comp_name), cp);
                    if (!comp)
                    {
                        local_err = "failed to spawn component";
                        return;
                    }
                    new_id = comp->get_id().str();
                    server.notify("model.scene.changed", Json::Value(Json::objectValue));
                });
            if (!done)
            {
                err = "component.add timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            Json::Value r(Json::objectValue);
            r["id"] = new_id;
            result = r;
        });

    // =========================================================================
    // Level API
    // =========================================================================

    server.on_request("model.level.save",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          std::string local_err;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* lvl = glob::glob_state().getr_model().current_level;
                                  if (!lvl)
                                  {
                                      local_err = "no level loaded";
                                      return;
                                  }
                                  auto& vfs = glob::glob_state().getr_vfs();
                                  auto levels_path = vfs.real_path(vfs::rid("data://levels"));
                                  if (!levels_path)
                                  {
                                      local_err = "cannot resolve levels path";
                                      return;
                                  }
                                  kryga::utils::path save_path(levels_path.value());
                                  if (!core::level_manager::save_level(*lvl, save_path))
                                  {
                                      local_err = "save_level failed";
                                  }
                              });
                          if (!done)
                          {
                              err = "level.save timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          Json::Value r(Json::objectValue);
                          r["ok"] = true;
                          result = r;
                      });

    // =========================================================================
    // Visibility / Layer API
    // =========================================================================

    // =========================================================================
    // Render Config API
    // =========================================================================

    server.on_request(
        "render.config.get",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cfg = glob::glob_state().getr_render().renderer.get_render_config();

                    // Shadows
                    Json::Value sh(Json::objectValue);
                    sh["enabled"] = cfg.shadows.enabled;
                    sh["pcf"] = static_cast<int>(cfg.shadows.pcf);

                    const char* pcf_names[] = {
                        "pcf_3x3", "pcf_5x5", "pcf_7x7", "poisson16", "poisson32"};
                    int pcf_idx = static_cast<int>(cfg.shadows.pcf);
                    sh["pcf_name"] = (pcf_idx >= 0 && pcf_idx < 5) ? pcf_names[pcf_idx] : "unknown";

                    sh["bias"] = cfg.shadows.bias;
                    sh["normal_bias"] = cfg.shadows.normal_bias;
                    sh["cascade_count"] = cfg.shadows.cascade_count;
                    sh["distance"] = cfg.shadows.distance;
                    sh["map_size"] = cfg.shadows.map_size;
                    r["shadows"] = sh;

                    // Clusters
                    Json::Value cl(Json::objectValue);
                    cl["tile_size"] = cfg.clusters.tile_size;
                    cl["depth_slices"] = cfg.clusters.depth_slices;
                    cl["max_lights_per_cluster"] = cfg.clusters.max_lights_per_cluster;
                    r["clusters"] = cl;

                    // Lighting
                    Json::Value lt(Json::objectValue);
                    lt["directional_enabled"] = cfg.lighting.directional_enabled;
                    lt["local_enabled"] = cfg.lighting.local_enabled;
                    lt["baked_enabled"] = cfg.lighting.baked_enabled;
                    r["lighting"] = lt;

                    // Debug
                    Json::Value db(Json::objectValue);
                    db["editor_mode"] = cfg.debug.editor_mode;
                    db["show_grid"] = cfg.debug.show_grid;
                    db["light_wireframe"] = cfg.debug.light_wireframe;
                    db["light_icons"] = cfg.debug.light_icons;
                    db["frustum_culling"] = cfg.debug.frustum_culling;
                    r["debug"] = db;

                    // Render scale
                    Json::Value rs(Json::objectValue);
                    rs["enabled"] = cfg.render_scale.enabled;
                    rs["divisor"] = cfg.render_scale.divisor;
                    r["render_scale"] = rs;

                    // Outline
                    Json::Value ol(Json::objectValue);
                    ol["enabled"] = cfg.outline.enabled;
                    Json::Value color(Json::arrayValue);
                    for (int i = 0; i < 4; ++i)
                    {
                        color.append(cfg.outline.color[i]);
                    }
                    ol["color"] = color;
                    ol["depth_threshold"] = cfg.outline.depth_threshold;
                    ol["normal_threshold"] = cfg.outline.normal_threshold;
                    r["outline"] = ol;
                });
            if (!done)
            {
                err = "render_config.get timed out";
                return;
            }
            result = r;
        });

    server.on_request(
        "render.config.set",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject())
            {
                err = "params must be an object";
                return;
            }
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cfg =
                        glob::glob_state().getr_render().renderer.get_pending_render_config();

                    // Shadows
                    if (params.isMember("shadows"))
                    {
                        auto& s = params["shadows"];
                        if (s.isMember("enabled"))
                        {
                            cfg.shadows.enabled = s["enabled"].asBool();
                        }
                        if (s.isMember("pcf"))
                        {
                            auto& v = s["pcf"];
                            if (v.isString())
                            {
                                std::string name = v.asString();
                                if (name == "pcf_3x3")
                                {
                                    cfg.shadows.pcf = render::pcf_mode::pcf_3x3;
                                }
                                else if (name == "pcf_5x5")
                                {
                                    cfg.shadows.pcf = render::pcf_mode::pcf_5x5;
                                }
                                else if (name == "pcf_7x7")
                                {
                                    cfg.shadows.pcf = render::pcf_mode::pcf_7x7;
                                }
                                else if (name == "poisson16")
                                {
                                    cfg.shadows.pcf = render::pcf_mode::poisson16;
                                }
                                else if (name == "poisson32")
                                {
                                    cfg.shadows.pcf = render::pcf_mode::poisson32;
                                }
                                else
                                {
                                    local_err = "unknown pcf mode: " + name;
                                    return;
                                }
                            }
                            else
                            {
                                cfg.shadows.pcf = static_cast<render::pcf_mode>(v.asInt());
                            }
                        }
                        if (s.isMember("bias"))
                        {
                            cfg.shadows.bias = s["bias"].asFloat();
                        }
                        if (s.isMember("normal_bias"))
                        {
                            cfg.shadows.normal_bias = s["normal_bias"].asFloat();
                        }
                        if (s.isMember("cascade_count"))
                        {
                            cfg.shadows.cascade_count = s["cascade_count"].asUInt();
                        }
                        if (s.isMember("distance"))
                        {
                            cfg.shadows.distance = s["distance"].asFloat();
                        }
                        if (s.isMember("map_size"))
                        {
                            cfg.shadows.map_size = s["map_size"].asUInt();
                        }
                    }

                    // Clusters
                    if (params.isMember("clusters"))
                    {
                        auto& c = params["clusters"];
                        if (c.isMember("tile_size"))
                        {
                            cfg.clusters.tile_size = c["tile_size"].asUInt();
                        }
                        if (c.isMember("depth_slices"))
                        {
                            cfg.clusters.depth_slices = c["depth_slices"].asUInt();
                        }
                        if (c.isMember("max_lights_per_cluster"))
                        {
                            cfg.clusters.max_lights_per_cluster =
                                c["max_lights_per_cluster"].asUInt();
                        }
                    }

                    // Lighting
                    if (params.isMember("lighting"))
                    {
                        auto& l = params["lighting"];
                        if (l.isMember("directional_enabled"))
                        {
                            cfg.lighting.directional_enabled = l["directional_enabled"].asBool();
                        }
                        if (l.isMember("local_enabled"))
                        {
                            cfg.lighting.local_enabled = l["local_enabled"].asBool();
                        }
                        if (l.isMember("baked_enabled"))
                        {
                            cfg.lighting.baked_enabled = l["baked_enabled"].asBool();
                        }
                    }

                    // Debug
                    if (params.isMember("debug"))
                    {
                        auto& d = params["debug"];
                        if (d.isMember("editor_mode"))
                        {
                            cfg.debug.editor_mode = d["editor_mode"].asBool();
                        }
                        if (d.isMember("show_grid"))
                        {
                            cfg.debug.show_grid = d["show_grid"].asBool();
                        }
                        if (d.isMember("light_wireframe"))
                        {
                            cfg.debug.light_wireframe = d["light_wireframe"].asBool();
                        }
                        if (d.isMember("light_icons"))
                        {
                            cfg.debug.light_icons = d["light_icons"].asBool();
                        }
                        if (d.isMember("frustum_culling"))
                        {
                            cfg.debug.frustum_culling = d["frustum_culling"].asBool();
                        }
                    }

                    // Render scale
                    if (params.isMember("render_scale"))
                    {
                        auto& rs = params["render_scale"];
                        if (rs.isMember("enabled"))
                        {
                            cfg.render_scale.enabled = rs["enabled"].asBool();
                        }
                        if (rs.isMember("divisor"))
                        {
                            cfg.render_scale.divisor = rs["divisor"].asUInt();
                        }
                    }

                    // Outline
                    if (params.isMember("outline"))
                    {
                        auto& o = params["outline"];
                        if (o.isMember("enabled"))
                        {
                            cfg.outline.enabled = o["enabled"].asBool();
                        }
                        if (o.isMember("color") && o["color"].isArray() && o["color"].size() == 4)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                cfg.outline.color[i] = o["color"][i].asFloat();
                            }
                        }
                        if (o.isMember("depth_threshold"))
                        {
                            cfg.outline.depth_threshold = o["depth_threshold"].asFloat();
                        }
                        if (o.isMember("normal_threshold"))
                        {
                            cfg.outline.normal_threshold = o["normal_threshold"].asFloat();
                        }
                    }

                    cfg.validate();
                });
            if (!done)
            {
                err = "render_config.set timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
            result["ok"] = true;
        });

    // =========================================================================
    // Render State (read-only debug inspection)
    // =========================================================================

    server.on_request("render.state.camera",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          Json::Value r(Json::objectValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto& cam =
                                      glob::glob_state().getr_render().renderer.get_camera();
                                  r["position"] = vec3_json(cam.position);
                                  r["view"] = mat4_json(cam.view);
                                  r["projection"] = mat4_json(cam.projection);
                                  r["inv_projection"] = mat4_json(cam.inv_projection);
                              });
                          if (!done)
                          {
                              err = "render_state.camera timed out";
                              return;
                          }
                          result = r;
                      });

    server.on_request(
        "render.state.object",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cache = glob::glob_state().getr_render().renderer.get_cache();
                    auto* obj = cache.objects.find_by_id(AID(id_str));
                    if (!obj)
                    {
                        local_err = "render object not found: " + id_str;
                        return;
                    }

                    r["id"] = obj->id().str();
                    r["slot"] = obj->slot();
                    r["renderable"] = obj->renderable;
                    r["outlined"] = obj->outlined;
                    r["queue_id"] = obj->queue_id;
                    r["layer_flags"] = obj->layer_flags;
                    r["distance_to_camera"] = obj->distance_to_camera;
                    r["bounding_radius"] = obj->bounding_radius;

                    // GPU data
                    auto& gd = obj->gpu_data;
                    Json::Value gpu(Json::objectValue);
                    gpu["model"] = mat4_json(gd.model);
                    gpu["normal"] = mat4_json(gd.normal);
                    gpu["obj_pos"] = vec3_json(gd.obj_pos);
                    gpu["bounding_sphere_center"] = vec3_json(gd.bounding_sphere_center);
                    gpu["bounding_radius"] = gd.bounding_radius;
                    gpu["material_id"] = gd.material_id;
                    gpu["bone_offset"] = gd.bone_offset;
                    gpu["bone_count"] = gd.bone_count;
                    gpu["probe_index"] = gd.probe_index;

                    Json::Value lm_scale(Json::arrayValue);
                    lm_scale.append(gd.lightmap_scale.x);
                    lm_scale.append(gd.lightmap_scale.y);
                    gpu["lightmap_scale"] = lm_scale;

                    Json::Value lm_off(Json::arrayValue);
                    lm_off.append(gd.lightmap_offset.x);
                    lm_off.append(gd.lightmap_offset.y);
                    gpu["lightmap_offset"] = lm_off;

                    gpu["lightmap_texture_index"] = gd.lightmap_texture_index;
                    r["gpu_data"] = gpu;

                    // Mesh info
                    if (obj->mesh)
                    {
                        Json::Value m(Json::objectValue);
                        m["id"] = obj->mesh->get_id().str();
                        m["vertices"] = obj->mesh->vertices_size();
                        m["indices"] = obj->mesh->indices_size();
                        m["is_skinned"] = obj->mesh->m_is_skinned;
                        m["local_centroid"] = vec3_json(obj->mesh->m_local_centroid);
                        m["bounding_radius"] = obj->mesh->m_bounding_radius;
                        r["mesh"] = m;
                    }

                    // Material info
                    if (obj->material)
                    {
                        Json::Value mt(Json::objectValue);
                        mt["id"] = obj->material->get_id().str();
                        mt["gpu_idx"] = obj->material->gpu_idx();
                        mt["has_gpu_data"] = obj->material->has_gpu_data();

                        auto& tex_indices = obj->material->get_bindless_texture_indices();
                        Json::Value ti(Json::arrayValue);
                        for (auto idx : tex_indices)
                        {
                            ti.append(idx);
                        }
                        mt["texture_indices"] = ti;

                        r["material"] = mt;
                    }
                });
            if (!done)
            {
                err = "render_state.object timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = r;
        });

    server.on_request(
        "render.state.stats",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& vr = glob::glob_state().getr_render().renderer;
                    auto& cache = vr.get_cache();
                    r["width"] = vr.get_width();
                    r["height"] = vr.get_height();
                    r["all_draws"] = vr.get_all_draws();
                    r["culled_draws"] = vr.get_culled_draws();
                    r["object_count"] = static_cast<Json::UInt64>(cache.objects.get_actual_size());
                    r["directional_light_count"] =
                        static_cast<Json::UInt64>(cache.directional_lights.get_actual_size());
                    r["universal_light_count"] =
                        static_cast<Json::UInt64>(cache.universal_lights.get_actual_size());
                    r["texture_count"] =
                        static_cast<Json::UInt64>(cache.textures.get_actual_size());
                });
            if (!done)
            {
                err = "render_state.stats timed out";
                return;
            }
            result = r;
        });

    server.on_request(
        "render.state.objects",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cache = glob::glob_state().getr_render().renderer.get_cache();

                    auto summarize = [](const render::vulkan_render_data& obj) -> Json::Value
                    {
                        Json::Value o(Json::objectValue);
                        o["id"] = obj.id().str();
                        o["slot"] = obj.slot();
                        o["position"] = vec3_json(obj.gpu_data.obj_pos);
                        o["bounding_sphere_center"] =
                            vec3_json(obj.gpu_data.bounding_sphere_center);
                        o["bounding_radius"] = obj.gpu_data.bounding_radius;
                        o["material_id"] = obj.gpu_data.material_id;
                        o["queue_id"] = obj.queue_id;
                        o["layer_flags"] = obj.layer_flags;
                        o["renderable"] = obj.renderable;
                        o["outlined"] = obj.outlined;
                        if (obj.mesh)
                        {
                            o["mesh_id"] = obj.mesh->get_id().str();
                        }
                        if (obj.material)
                        {
                            o["material_name"] = obj.material->get_id().str();
                        }
                        return o;
                    };

                    Json::Value arr(Json::arrayValue);

                    // Mode 1: specific IDs
                    if (params.isObject() && params.isMember("ids") && params["ids"].isArray())
                    {
                        for (const auto& id_val : params["ids"])
                        {
                            auto* obj = cache.objects.find_by_id(AID(id_val.asString()));
                            if (obj)
                            {
                                arr.append(summarize(*obj));
                            }
                        }
                    }
                    // Mode 2: offset + limit pagination (or all)
                    else
                    {
                        uint32_t offset = 0;
                        uint32_t limit = UINT32_MAX;
                        if (params.isObject())
                        {
                            if (params.isMember("offset"))
                            {
                                offset = params["offset"].asUInt();
                            }
                            if (params.isMember("limit"))
                            {
                                limit = params["limit"].asUInt();
                            }
                        }

                        uint32_t idx = 0;
                        uint32_t added = 0;
                        cache.objects.for_each(
                            [&](const render::vulkan_render_data& obj)
                            {
                                if (added >= limit)
                                {
                                    return;
                                }
                                if (idx >= offset)
                                {
                                    arr.append(summarize(obj));
                                    ++added;
                                }
                                ++idx;
                            });
                    }

                    r["objects"] = arr;
                    r["total"] = static_cast<Json::UInt64>(cache.objects.get_actual_size());
                });
            if (!done)
            {
                err = "render_state.objects timed out";
                return;
            }
            result = r;
        });

    server.on_request("render.state.lights",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          Json::Value r(Json::objectValue);
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto& cache =
                                      glob::glob_state().getr_render().renderer.get_cache();

                                  // Directional lights
                                  Json::Value dir(Json::arrayValue);
                                  cache.directional_lights.for_each(
                                      [&](const render::vulkan_directional_light_data& dl)
                                      {
                                          Json::Value l(Json::objectValue);
                                          l["id"] = dl.id().str();
                                          l["slot"] = dl.slot();
                                          l["direction"] = vec3_json(dl.gpu_data.direction);
                                          l["ambient"] = vec3_json(dl.gpu_data.ambient);
                                          l["diffuse"] = vec3_json(dl.gpu_data.diffuse);
                                          l["specular"] = vec3_json(dl.gpu_data.specular);
                                          dir.append(std::move(l));
                                      });
                                  r["directional"] = dir;

                                  // Universal lights (point + spot)
                                  Json::Value uni(Json::arrayValue);
                                  cache.universal_lights.for_each(
                                      [&](const render::vulkan_universal_light_data& ul)
                                      {
                                          Json::Value l(Json::objectValue);
                                          l["id"] = ul.id().str();
                                          l["slot"] = ul.slot();
                                          l["type"] = ul.gpu_data.type == 0 ? "spot" : "point";
                                          l["position"] = vec3_json(ul.gpu_data.position);
                                          l["direction"] = vec3_json(ul.gpu_data.direction);
                                          l["ambient"] = vec3_json(ul.gpu_data.ambient);
                                          l["diffuse"] = vec3_json(ul.gpu_data.diffuse);
                                          l["specular"] = vec3_json(ul.gpu_data.specular);
                                          l["radius"] = ul.gpu_data.radius;
                                          l["cut_off"] = ul.gpu_data.cut_off;
                                          l["outer_cut_off"] = ul.gpu_data.outer_cut_off;
                                          l["shadow_index"] = ul.gpu_data.shadow_index;
                                          uni.append(std::move(l));
                                      });
                                  r["universal"] = uni;
                              });
                          if (!done)
                          {
                              err = "render_state.lights timed out";
                              return;
                          }
                          result = r;
                      });

    server.on_request("render.visibility.set",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          bool visible = params.get("visible", true).asBool();
                          std::string local_err;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* lvl = glob::glob_state().getr_model().current_level;
                                  if (!lvl)
                                  {
                                      local_err = "no level loaded";
                                      return;
                                  }
                                  auto* obj = lvl->find_object(AID(id_str));
                                  if (!obj)
                                  {
                                      local_err = "object not found: " + id_str;
                                      return;
                                  }

                                  root::game_object_component* goc = nullptr;
                                  if (auto* go = obj->as<root::game_object>())
                                  {
                                      goc = go->get_root_component();
                                  }
                                  else if (auto* comp = obj->as<root::component>())
                                  {
                                      goc = dynamic_cast<root::game_object_component*>(comp);
                                  }

                                  if (!goc)
                                  {
                                      local_err = "object has no layer flags";
                                      return;
                                  }
                                  goc->set_layer_flag(render::LAYER_VISIBLE, visible);
                                  goc->mark_render_dirty();
                              });
                          if (!done)
                          {
                              err = "visibility.set timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                      });

    // ── Materials ─────────────────────────────────────────────────────

    server.on_request(
        "model.material.list",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            bool done = eng.wait_main_action(
                [&]()
                {
                    Json::Value arr(Json::arrayValue);
                    for (auto& [id, obj] :
                         glob::glob_state().getr_model().class_caches.materials.get_items())
                    {
                        auto* rt = obj->get_reflection();
                        Json::Value item(Json::objectValue);
                        item["id"] = id.str();
                        item["type"] = rt ? rt->type_name.str() : std::string();
                        auto* pkg = obj->get_package();
                        item["package"] = pkg ? pkg->get_id().str() : std::string();

                        auto& mat = obj->asr<root::material>();
                        item["has_preview"] = mat.get_shader_effect() != nullptr;

                        arr.append(std::move(item));
                    }
                    result = Json::Value(Json::objectValue);
                    result["materials"] = std::move(arr);
                });
            if (!done)
            {
                err = "material.list timed out";
            }
        });

    server.on_request(
        "model.material.get",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* mat = glob::glob_state().getr_model().class_caches.materials.get_item(
                        AID(id_str));
                    if (!mat)
                    {
                        local_err = "material not found: " + id_str;
                        return;
                    }
                    result = Json::Value(Json::objectValue);
                    result["material"] = encode_owner(*mat);
                });
            if (!done)
            {
                err = "material.get timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
        });

    server.on_request("render.material.preview",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          uint32_t size = params.get("size", 128).asUInt();

                          std::string b64;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  b64 = glob::glob_state()
                                            .getr_editor_system()
                                            .ui.get_material_previewer()
                                            .render_preview(AID(id_str), size);
                              });
                          if (!done)
                          {
                              err = "material.preview timed out";
                              return;
                          }
                          if (b64.empty())
                          {
                              err = "material not found or preview failed: " + id_str;
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["image"] = "data:image/png;base64," + b64;
                      });

    server.on_request(
        "model.material.assign",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("owner_id") ||
                !params.isMember("material_id"))
            {
                err = "missing 'owner_id' or 'material_id' parameter";
                return;
            }
            std::string owner_id = params["owner_id"].asString();
            std::string material_id = params["material_id"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().getr_model().current_level;
                    if (!lvl)
                    {
                        local_err = "no level loaded";
                        return;
                    }
                    auto* comp = lvl->find_component(AID(owner_id));
                    if (!comp)
                    {
                        local_err = "component not found: " + owner_id;
                        return;
                    }
                    auto* mesh_comp = comp->as<base::mesh_component>();
                    if (!mesh_comp)
                    {
                        local_err = "not a mesh component: " + owner_id;
                        return;
                    }
                    auto* mat_obj = glob::glob_state().getr_model().class_caches.materials.get_item(
                        AID(material_id));
                    if (!mat_obj)
                    {
                        local_err = "material not found: " + material_id;
                        return;
                    }
                    mesh_comp->set_material(mat_obj->as<root::material>());
                });
            if (!done)
            {
                err = "model.material.assign timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
            result["ok"] = true;
            Json::Value note(Json::objectValue);
            note["owner_id"] = owner_id;
            note["material_id"] = material_id;
            server.notify("model.material.assigned", note);
        });

    server.on_request("model.material.edit",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          std::string local_err;
                          Json::Value props;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* inst = glob::glob_state()
                                                   .getr_editor_system()
                                                   .ui.get_material_previewer()
                                                   .begin_edit(AID(id_str));
                                  if (!inst)
                                  {
                                      local_err = "failed to begin edit for: " + id_str;
                                      return;
                                  }
                                  props = encode_owner(*inst);
                              });
                          if (!done)
                          {
                              err = "material.edit timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["material"] = props;
                      });

    server.on_request(
        "model.material.setField",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id") || !params.isMember("field") ||
                !params.isMember("value"))
            {
                err = "missing 'id', 'field', or 'value' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string field = params["field"].asString();
            Json::Value value = params["value"];

            std::string local_err;
            Json::Value echo_value;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* inst = glob::glob_state()
                                     .getr_editor_system()
                                     .ui.get_material_previewer()
                                     .get_editing(AID(id_str));
                    if (!inst)
                    {
                        local_err = "no active edit session for: " + id_str;
                        return;
                    }
                    local_err = write_property(*inst, field, value, echo_value);

                    glob::glob_state().getr_editor_system().ui.get_material_previewer().invalidate(
                        AID(id_str));
                });
            if (!done)
            {
                err = "material.setField timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
            result["value"] = echo_value;
        });

    server.on_request(
        "model.texture.list",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            bool done = eng.wait_main_action(
                [&]()
                {
                    Json::Value arr(Json::arrayValue);
                    for (auto& [id, obj] :
                         glob::glob_state().getr_model().class_caches.textures.get_items())
                    {
                        Json::Value item(Json::objectValue);
                        item["id"] = id.str();
                        auto* pkg = obj->get_package();
                        item["package"] = pkg ? pkg->get_id().str() : std::string();
                        arr.append(std::move(item));
                    }
                    result = Json::Value(Json::objectValue);
                    result["textures"] = std::move(arr);
                });
            if (!done)
            {
                err = "texture.list timed out";
            }
        });

    server.on_request("model.material.save",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          std::string local_err;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  if (!glob::glob_state()
                                           .getr_editor_system()
                                           .ui.get_material_previewer()
                                           .save_edit(AID(id_str)))
                                  {
                                      local_err = "save failed for: " + id_str;
                                  }
                              });
                          if (!done)
                          {
                              err = "material.save timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    server.on_request("model.material.discard",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("id"))
                          {
                              err = "missing 'id' parameter";
                              return;
                          }
                          std::string id_str = params["id"].asString();
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  glob::glob_state()
                                      .getr_editor_system()
                                      .ui.get_material_previewer()
                                      .discard_edit(AID(id_str));
                              });
                          if (!done)
                          {
                              err = "material.discard timed out";
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    // ── Action queue ─────────────────────────────────────────────────────

    server.on_request(
        "actions.getStatus",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& actions = glob::glob_state().getr_editor_system().ui.m_actions;

                    result = Json::Value(Json::objectValue);
                    result["busy"] = actions.is_busy();
                    result["current_name"] = actions.current_name();
                    result["progress"] = actions.current_progress()->progress.load();
                    result["status"] = actions.current_progress()->get_status();
                    result["queued_count"] = static_cast<Json::UInt>(actions.queued_count());

                    Json::Value fin(Json::arrayValue);
                    for (auto& r : actions.finished())
                    {
                        Json::Value item(Json::objectValue);
                        item["name"] = r.name;
                        item["success"] = r.success;
                        if (!r.error.empty())
                        {
                            item["error"] = r.error;
                        }
                        item["duration_ms"] = r.duration_ms;
                        fin.append(std::move(item));
                    }
                    result["finished"] = std::move(fin);
                });
            if (!done)
            {
                err = "actions.getStatus timed out";
            }
        });

    server.on_request(
        "actions.clearFinished",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            bool done = eng.wait_main_action(
                [&]() { glob::glob_state().getr_editor_system().ui.m_actions.clear_finished(); });
            if (!done)
            {
                err = "actions.clearFinished timed out";
                return;
            }
            result = Json::Value(Json::objectValue);
            result["ok"] = true;
        });

    // ── Bake editor ────────────────────────────────────────────────────

    server.on_request("bake.getConfig",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* be = ui::get_window<ui::bake_editor>();
                                  KRG_check(be, "bake_editor window not registered");
                                  auto& cfg = be->get_config();

                                  result = Json::Value(Json::objectValue);
                                  result["resolution"] = cfg.resolution;
                                  result["samples_per_texel"] = cfg.samples_per_texel;
                                  result["bounce_count"] = cfg.bounce_count;
                                  result["denoise_iterations"] = cfg.denoise_iterations;
                                  result["ao_radius"] = cfg.ao_radius;
                                  result["ao_intensity"] = cfg.ao_intensity;
                                  result["bake_direct"] = cfg.bake_direct;
                                  result["bake_indirect"] = cfg.bake_indirect;
                                  result["bake_ao"] = cfg.bake_ao;
                                  result["save_png"] = cfg.save_png;
                                  result["texels_per_unit"] = cfg.texels_per_unit;
                                  result["min_tile"] = cfg.min_tile;
                                  result["max_tile"] = cfg.max_tile;
                                  result["shadow_bias"] = cfg.shadow_bias;
                                  result["shadow_samples"] = cfg.shadow_samples;
                                  result["shadow_spread"] = cfg.shadow_spread;
                                  result["dilate_iterations"] = cfg.dilate_iterations;
                              });
                          if (!done)
                          {
                              err = "bake.getConfig timed out";
                          }
                      });

    server.on_request("bake.setConfig",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject())
                          {
                              err = "params must be an object";
                              return;
                          }
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* be = ui::get_window<ui::bake_editor>();
                                  KRG_check(be, "bake_editor window not registered");
                                  auto& cfg = be->get_config();

                                  if (params.isMember("resolution"))
                                  {
                                      cfg.resolution = params["resolution"].asUInt();
                                  }
                                  if (params.isMember("samples_per_texel"))
                                  {
                                      cfg.samples_per_texel = params["samples_per_texel"].asUInt();
                                  }
                                  if (params.isMember("bounce_count"))
                                  {
                                      cfg.bounce_count = params["bounce_count"].asUInt();
                                  }
                                  if (params.isMember("denoise_iterations"))
                                  {
                                      cfg.denoise_iterations =
                                          params["denoise_iterations"].asUInt();
                                  }
                                  if (params.isMember("ao_radius"))
                                  {
                                      cfg.ao_radius = params["ao_radius"].asFloat();
                                  }
                                  if (params.isMember("ao_intensity"))
                                  {
                                      cfg.ao_intensity = params["ao_intensity"].asFloat();
                                  }
                                  if (params.isMember("bake_direct"))
                                  {
                                      cfg.bake_direct = params["bake_direct"].asBool();
                                  }
                                  if (params.isMember("bake_indirect"))
                                  {
                                      cfg.bake_indirect = params["bake_indirect"].asBool();
                                  }
                                  if (params.isMember("bake_ao"))
                                  {
                                      cfg.bake_ao = params["bake_ao"].asBool();
                                  }
                                  if (params.isMember("save_png"))
                                  {
                                      cfg.save_png = params["save_png"].asBool();
                                  }
                                  if (params.isMember("texels_per_unit"))
                                  {
                                      cfg.texels_per_unit = params["texels_per_unit"].asFloat();
                                  }
                                  if (params.isMember("min_tile"))
                                  {
                                      cfg.min_tile = params["min_tile"].asInt();
                                  }
                                  if (params.isMember("max_tile"))
                                  {
                                      cfg.max_tile = params["max_tile"].asInt();
                                  }
                                  if (params.isMember("shadow_bias"))
                                  {
                                      cfg.shadow_bias = params["shadow_bias"].asFloat();
                                  }
                                  if (params.isMember("shadow_samples"))
                                  {
                                      cfg.shadow_samples = params["shadow_samples"].asUInt();
                                  }
                                  if (params.isMember("shadow_spread"))
                                  {
                                      cfg.shadow_spread = params["shadow_spread"].asFloat();
                                  }
                                  if (params.isMember("dilate_iterations"))
                                  {
                                      cfg.dilate_iterations = params["dilate_iterations"].asUInt();
                                  }
                              });
                          if (!done)
                          {
                              err = "bake.setConfig timed out";
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    server.on_request("bake.applyPreset",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          if (!params.isObject() || !params.isMember("preset"))
                          {
                              err = "missing 'preset' parameter";
                              return;
                          }
                          std::string preset_str = params["preset"].asString();
                          render::bake::bake_preset preset;
                          if (preset_str == "low")
                          {
                              preset = render::bake::bake_preset::low;
                          }
                          else if (preset_str == "medium")
                          {
                              preset = render::bake::bake_preset::medium;
                          }
                          else if (preset_str == "high")
                          {
                              preset = render::bake::bake_preset::high;
                          }
                          else if (preset_str == "maximum")
                          {
                              preset = render::bake::bake_preset::maximum;
                          }
                          else
                          {
                              err = "invalid preset: " + preset_str;
                              return;
                          }

                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* be = ui::get_window<ui::bake_editor>();
                                  KRG_check(be, "bake_editor window not registered");
                                  be->get_config().apply_preset(preset);
                              });
                          if (!done)
                          {
                              err = "bake.applyPreset timed out";
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["ok"] = true;
                      });

    server.on_request("bake.getSceneInfo",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* be = ui::get_window<ui::bake_editor>();
                                  KRG_check(be, "bake_editor window not registered");
                                  auto info = be->collect_scene_info();

                                  result = Json::Value(Json::objectValue);
                                  result["static_count"] = info.static_count;
                                  result["directional_count"] = info.directional_count;
                                  result["local_light_count"] = info.local_light_count;
                                  result["level_loaded"] = info.level_loaded;
                              });
                          if (!done)
                          {
                              err = "bake.getSceneInfo timed out";
                          }
                      });

    server.on_request("bake.start",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          std::string local_err;
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* be = ui::get_window<ui::bake_editor>();
                                  KRG_check(be, "bake_editor window not registered");

                                  if (!be->submit_bake())
                                  {
                                      local_err =
                                          "cannot bake: no level, no meshes, no lights, or busy";
                                  }
                              });
                          if (!done)
                          {
                              err = "bake.start timed out";
                              return;
                          }
                          if (!local_err.empty())
                          {
                              err = std::move(local_err);
                              return;
                          }
                          result = Json::Value(Json::objectValue);
                          result["queued"] = true;
                      });

    // ── Converter ─────────────────────────────────────────────────────

    server.on_request(
        "converter.start",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject())
            {
                err = "params must be an object";
                return;
            }

            std::string input = params.get("input", "").asString();
            std::string output_root = params.get("output_root", "").asString();
            std::string name = params.get("name", "").asString();
            std::string mode = params.get("mode", "package").asString();
            std::string existing = params.get("existing_package", "").asString();

            std::vector<std::string> deps;
            if (params.isMember("deps") && params["deps"].isArray())
            {
                for (const auto& d : params["deps"])
                {
                    deps.push_back(d.asString());
                }
            }

            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* conv = ui::get_window<ui::converter_window>();
                    KRG_check(conv, "converter_window not registered");

                    if (!conv->submit_conversion(input, output_root, name, mode, existing, deps))
                    {
                        local_err = conv->get_status_text();
                    }
                });
            if (!done)
            {
                err = "converter.start timed out";
                return;
            }
            if (!local_err.empty())
            {
                err = std::move(local_err);
                return;
            }
            result = Json::Value(Json::objectValue);
            result["queued"] = true;
        });

    server.on_request("converter.getStatus",
                      [&eng](const Json::Value&, Json::Value& result, std::string& err)
                      {
                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto* conv = ui::get_window<ui::converter_window>();
                                  KRG_check(conv, "converter_window not registered");

                                  result = Json::Value(Json::objectValue);
                                  result["running"] = conv->is_running();
                                  result["status"] = conv->get_status_text();
                                  result["is_error"] = conv->is_status_error();
                              });
                          if (!done)
                          {
                              err = "converter.getStatus timed out";
                          }
                      });

    server.on_request("converter.listDeps",
                      [&eng](const Json::Value& params, Json::Value& result, std::string& err)
                      {
                          std::string output_root;
                          if (params.isObject() && params.isMember("output_root"))
                          {
                              output_root = params["output_root"].asString();
                          }

                          bool done = eng.wait_main_action(
                              [&]()
                              {
                                  auto deps = ui::converter_window::list_deps(output_root);

                                  result = Json::Value(Json::objectValue);
                                  Json::Value arr(Json::arrayValue);
                                  for (const auto& [id, checked] : deps)
                                  {
                                      Json::Value item(Json::objectValue);
                                      item["id"] = id;
                                      item["checked"] = checked;
                                      arr.append(std::move(item));
                                  }
                                  result["deps"] = std::move(arr);
                              });
                          if (!done)
                          {
                              err = "converter.listDeps timed out";
                          }
                      });

    // Wire converter completion → RPC notification
    {
        auto* conv = ui::get_window<ui::converter_window>();
        KRG_check(conv, "converter_window not registered");
        conv->set_completed_callback(
            [&server](const ui::converter_result& cr)
            {
                Json::Value params(Json::objectValue);
                params["success"] = cr.success;
                params["exit_code"] = cr.exit_code;
                if (!cr.log_tail.empty())
                {
                    params["log_tail"] = cr.log_tail;
                }
                server.notify("converter.completed", params);
            });
    }

    // Wire action queue events → RPC notifications
    glob::glob_state().getr_editor_system().ui.m_actions.set_event_callback(
        [&server](const engine::action_event& evt)
        {
            Json::Value params(Json::objectValue);
            params["name"] = evt.name;

            switch (evt.type)
            {
            case engine::action_event_type::started:
                server.notify("action.started", params);
                break;
            case engine::action_event_type::progress:
                params["progress"] = evt.progress;
                params["status"] = evt.status;
                server.notify("action.progress", params);
                break;
            case engine::action_event_type::completed:
                params["success"] = evt.success;
                if (!evt.error.empty())
                {
                    params["error"] = evt.error;
                }
                params["duration_ms"] = evt.duration_ms;
                server.notify("action.completed", params);
                break;
            }
        });
}

}  // namespace kryga::engine_private
