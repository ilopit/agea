# agea
AGEA - Awesome Game Engine AGEA

## Build

### Quick start (MSYS/bash)
```bash
./tools/configure.sh    # Configure cmake
./tools/build.sh        # Build (Debug)
```

### Configure options
```bash
./tools/configure.sh           # Quiet output
./tools/configure.sh -v        # Verbose output
./tools/configure.sh -c        # Clean build directory first
./tools/configure.sh -c -v     # Clean + verbose
```

### Build options
```bash
./tools/build.sh               # Build engine_app (Debug, quiet)
./tools/build.sh -a            # Build all targets
./tools/build.sh -v            # Verbose output
./tools/build.sh -r            # Build Release
./tools/build.sh -j 8          # Use 8 parallel jobs
./tools/build.sh <target>      # Build specific target
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