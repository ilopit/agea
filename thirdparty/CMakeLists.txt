find_package(Vulkan REQUIRED)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(BASH_EXECUTABLE "bash.exe"  CACHE INTERNAL "")

function(externalize)
set(name "")
get_target_property(name ${ARGV0} FOLDER)
message(${ARGV0})

if(name STREQUAL "name-NOTFOUND")
set(name "")
endif()

set_target_properties(${ARGV0} PROPERTIES FOLDER "thirdparty/${ARGV1}/${name}")
message(external/${name})
endfunction()

#fetch
add_subdirectory(fetch)
externalize(boost_chrono "boost")
externalize(boost_container "boost")
externalize(boost_context "boost")
externalize(boost_coroutine "boost")
externalize(boost_date_time "boost")
externalize(boost_exception "boost")
externalize(boost_url "boost")

#imgui
add_subdirectory(unofficial/imgui)
externalize(imgui_unofficial "imgui")

#vma
add_subdirectory(unofficial/vma)
externalize(vma_unofficial "vma")

#textedit
add_subdirectory(unofficial/textedit)
externalize(textedit_unofficial "textedit")

#stb
add_subdirectory(unofficial/stb)
externalize(stb_unofficial "stb")

#sdl2
add_subdirectory(unofficial/sdl2)
externalize(sdl2 "sdl2")

set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)

#glm
add_subdirectory(unofficial/glm)
externalize(glm_unofficial "glm")

#glm
add_subdirectory(upstream/glm)
externalize(glm_static "glm")

#jsoncpp
set(JSONCPP_WITH_TESTS OFF)
set(BUILD_OBJECT_LIBS OFF)

add_subdirectory(upstream/jsoncpp)
externalize(jsoncpp_static "jsoncpp")

#lua
add_subdirectory(upstream/lua)
externalize(lua_static "lua")
externalize(lua_shared "lua")
externalize(luac "lua")

#luaa
add_subdirectory(upstream/lua_bindings)
add_subdirectory(unofficial/lua_bindings)

#vkbootstrap
add_subdirectory(upstream/vkbootstrap)
externalize(vk-bootstrap "vk-bootstrap")


SET(SPDLOG_USE_STD_FORMAT ON)

#spdlog
add_subdirectory(upstream/spdlog)
externalize(spdlog "spdlog")

#spirvreflect
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)

add_subdirectory(upstream/spirvreflect)
externalize(spirv-reflect-static "spirvreflect")

#tinyobjloader
add_subdirectory(upstream/tinyobjloader)
externalize(tinyobjloader "tinyobjloader")

#gtests
set(gtest_force_shared_crt ON)

add_subdirectory(upstream/gtests)
externalize(gtest "gtests")
externalize(gmock "gtests")
externalize(gtest_main "gtests")
externalize(gmock_main "gtests")

#yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_INSTALL OFF)
add_subdirectory(upstream/yaml-cpp)
externalize(yaml-cpp "yaml-cpp")