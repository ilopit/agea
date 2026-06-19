# Animation

Skeletal animation system built on ozz-animation.

## Core system (`animation_system.h`)
- Registers skeletons with joint hierarchies and inverse bind matrices
- Manages animation clips per skeleton
- Creates/destroys animation instances with playback control

## Data flow
1. `tick()` samples animations per-instance
2. Bone matrices pushed to global staging buffer
3. `render_data_resolver` callback decouples from render layer

## Gotchas
- Joint remapping: `joint_remaps` (int32 vectors) allow animation clip joints to differ from mesh rendering joints
- Staging buffer index 0 is reserved as identity matrix for non-skinned objects
- SOA transforms internally (ozz SIMD format), converted to `glm::mat4` for GPU upload
