file(GLOB BENCH_SOURCES
    "*.h"
    "*.cpp"
)
source_group("bench_sources" FILES ${BENCH_SOURCES})

add_executable(render_types_bench
    ${BENCH_SOURCES}
)

target_link_libraries(render_types_bench
    kryga::render_types
    benchmark::benchmark
    benchmark::benchmark_main
)

kryga_finalize_executable(render_types_bench)
