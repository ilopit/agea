# translators domain: model <-> render translation libraries (producer-side).
# render_translator is the model thread's Vulkan-aware render-production arm; future
# translators (asset/format converters, other model->X bridges) belong here too.
add_subdirectory(render_translator)
add_subdirectory(audio_bridge)
