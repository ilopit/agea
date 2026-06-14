add_executable(kryga_audio_import
    main.cpp
)

target_link_libraries(kryga_audio_import PRIVATE
    kryga::asset_converter
    kryga::asset_importer
    kryga::project_paths
    kryga::utils
)

kryga_finalize_executable(kryga_audio_import)
kryga_ide_path(kryga_audio_import)
