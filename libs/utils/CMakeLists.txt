file(GLOB LIB_SRC "public/include/utils/*.h" "private/src/*.cpp")

add_library(utils STATIC
   ${LIB_SRC}
)
target_link_libraries(utils spdlog)

# Ensure spdlog trace macros are active (matches kryga_log.h define)
target_compile_definitions(utils PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

target_precompile_headers(utils PUBLIC
    # spdlog (heaviest header in the project, ~1500 lines of templates)
    "$<$<COMPILE_LANGUAGE:CXX>:<spdlog/spdlog.h$<ANGLE-R>>"
    # Common STL headers
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<array$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cstdint$<ANGLE-R>>"
)

kryga_finalize_library(utils)

add_subdirectory(private/tests)