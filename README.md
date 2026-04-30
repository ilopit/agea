# kryga
KRYGA

## Build

Build is driven by CMake presets (`CMakePresets.json`). The bash wrappers
auto-configure on first run.

### Quick start (MSYS/bash)
```bash
./tools/build.sh               # Build kryga_editor (Debug) — configures build/ on first run
./tools/build.sh -r            # Release
./tools/build_android.sh       # Android arm64-v8a Debug (needs NDK r26d)
```

### Build options
```bash
./tools/build.sh               # Build kryga_editor (Debug, quiet)
./tools/build.sh -a            # Build all targets
./tools/build.sh -v            # Verbose output
./tools/build.sh -r            # Build Release
./tools/build.sh -j 8          # Use 8 parallel jobs
./tools/build.sh <target>      # Build specific target
```

### Direct preset usage (IDE-friendly)
```bash
cmake --preset host                        # configure host
cmake --build --preset host-debug          # build Debug
cmake --workflow --preset android-debug    # configure + build Android Debug
```

# Stages of type loading 
 - Make glue type ids 
 - Make type resolver
 - Handle packages in topoorder
    - Create Model types. For every type in topoorder
        - Create RT type
            - Assign type handers
            - Assing architecture
            - Assign parent
    - Create types  properties. For every type in topoorder (Why separate? We need properties needs to have all tipes. We don't want to create type->property dependancy)
        - Create properties
        - Inherit properties 
    - Create Render. For every type in topoorder
        - Assign render types overrides
        - Assign render properties
    - Finalize type. Inherit handlers and architecture